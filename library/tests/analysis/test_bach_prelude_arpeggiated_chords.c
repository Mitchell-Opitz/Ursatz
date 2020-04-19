#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "events/note_event.h"
#include "io/midi_importer.h"
#include "pitch/pitch_numeric_value.h"
#include "time/duration.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"

/*
 * Regression check for arpeggiated chord grouping: no production code
 * path in this repo currently groups sequential (non-simultaneous)
 * notes into a Sonority before chord identification -- see
 * src/semantics/sonority.h/.c, which only accepts an already-selected
 * set of simultaneous event ids, and src/analysis/chord_identification.c,
 * which requires that Sonority (or a pre-computed scale-degree/interval
 * set) as input. There is no code path anywhere that derives one from
 * raw NoteEvents by time-windowing.
 *
 * This test is a placeholder acceptance check, not an integration of
 * that missing step: it does its OWN naive per-measure pitch-class
 * grouping (measure = one whole note in 4/4, matching this fixture),
 * and checks the result against the known-correct harmonic analysis of
 * the fixture's first 8 measures. It intentionally ignores inversions,
 * octave, and any 7th/extension tones -- only whether the notated root
 * triad's pitch classes are all present in the measure.
 *
 * Once real chord-identification grouping exists, this test's grouping
 * helper should be deleted in favor of calling that code directly; the
 * expected-chord table below is the part worth keeping.
 */

#define FIXTURE_PATH "../fixtures/arpeggiation-investigation/Bach_Prelude_C_Major.mid"
#define MEASURE_COUNT_CHECKED 8

typedef struct {
    const char *name;
    int pitch_classes[3];
} ExpectedTriad;

/* C Dm G C Am D(secondary dominant, i.e. V/V - D major, not D minor) G C */
static const ExpectedTriad EXPECTED[MEASURE_COUNT_CHECKED] = {
    {"C major", {0, 4, 7}},
    {"D minor", {2, 5, 9}},
    {"G major", {7, 11, 2}},
    {"C major", {0, 4, 7}},
    {"A minor", {9, 0, 4}},
    {"D major (V/V)", {2, 6, 9}},
    {"G major", {7, 11, 2}},
    {"C major", {0, 4, 7}},
};

/* Deliberately does not rely on assert() for the fopen/fseek/fread
 * calls themselves: assert() is compiled out under -DNDEBUG (this
 * repo's default CMake Release build defines it), which would silently
 * skip reading the fixture entirely rather than failing loudly. */
static unsigned char *read_fixture(const char *path, size_t *out_size) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        fprintf(stderr, "failed to open fixture: %s\n", path);
        exit(1);
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fprintf(stderr, "fseek(SEEK_END) failed on fixture: %s\n", path);
        exit(1);
    }
    long size = ftell(f);
    if (size <= 0) {
        fprintf(stderr, "ftell returned non-positive size for fixture: %s\n", path);
        exit(1);
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fprintf(stderr, "fseek(SEEK_SET) failed on fixture: %s\n", path);
        exit(1);
    }

    unsigned char *buf = malloc((size_t)size);
    if (buf == NULL) {
        fprintf(stderr, "malloc failed for fixture: %s\n", path);
        exit(1);
    }
    if (fread(buf, 1, (size_t)size, f) != (size_t)size) {
        fprintf(stderr, "short read on fixture: %s\n", path);
        exit(1);
    }
    fclose(f);

    *out_size = (size_t)size;
    return buf;
}

static bool measure_contains_triad(const bool *pitch_class_present, const ExpectedTriad *triad) {
    for (int i = 0; i < 3; i++) {
        if (!pitch_class_present[triad->pitch_classes[i]]) {
            return false;
        }
    }
    return true;
}

int main(void) {
    size_t size = 0;
    unsigned char *data = read_fixture(FIXTURE_PATH, &size);

    NoteEvent **events = NULL;
    PitchNumericValue **pitch_values = NULL;
    size_t count = 0;
    MidiImportResult import_result = midi_importer_import(data, size, &events, &pitch_values, &count);
    free(data);
    if (import_result != MIDI_IMPORT_OK) {
        fprintf(stderr, "midi_importer_import failed: %d\n", import_result);
        return 1;
    }
    if (count == 0) {
        fprintf(stderr, "midi_importer_import produced no notes\n");
        return 1;
    }

    /* One measure == one whole note in 4/4: MusicalTime position is an
     * exact fraction (in lowest terms, so the denominator varies note
     * to note) of a whole note, so a position's measure index is just
     * floor(numerator / denominator). */
    bool pitch_class_present[MEASURE_COUNT_CHECKED][12];
    memset(pitch_class_present, 0, sizeof(pitch_class_present));

    for (size_t i = 0; i < count; i++) {
        const MusicalTime *pos = temporal_anchor_position(note_event_temporal_anchor(events[i]));
        long numerator = musical_time_numerator(pos);
        unsigned long denominator = musical_time_denominator(pos);

        long measure_index = numerator / (long)denominator;
        if (measure_index < 0 || measure_index >= MEASURE_COUNT_CHECKED) {
            continue;
        }

        long midi_note = pitch_numeric_value_position_numerator(pitch_values[i]);
        int pitch_class = (int)(((midi_note % 12) + 12) % 12);
        pitch_class_present[measure_index][pitch_class] = true;
    }

    int failures = 0;
    for (int m = 0; m < MEASURE_COUNT_CHECKED; m++) {
        bool ok = measure_contains_triad(pitch_class_present[m], &EXPECTED[m]);
        printf("measure %d: expected %-16s %s\n", m + 1, EXPECTED[m].name, ok ? "OK" : "MISMATCH");
        if (!ok) {
            failures++;
        }
    }

    for (size_t i = 0; i < count; i++) {
        note_event_destroy(events[i]);
        pitch_numeric_value_destroy(pitch_values[i]);
    }
    free(events);
    free(pitch_values);

    if (failures > 0) {
        fprintf(stderr, "bach_prelude_arpeggiated_chords: %d/%d measures mismatched\n", failures, MEASURE_COUNT_CHECKED);
        return 1;
    }

    printf("bach_prelude_arpeggiated_chords: ok (%zu notes, %d measures)\n", count, MEASURE_COUNT_CHECKED);
    return 0;
}
