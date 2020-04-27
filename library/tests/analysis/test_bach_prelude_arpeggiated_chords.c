#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis/chord_identification.h"
#include "core/value.h"
#include "events/note_event.h"
#include "identity/entity_id.h"
#include "interpretation/interpretation.h"
#include "io/midi_importer.h"
#include "pitch/pitch_numeric_value.h"
#include "semantics/sonority.h"
#include "semantics/sonority_builder.h"
#include "theory/framework.h"
#include "time/duration.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"
#include "time/time_span.h"

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

/*
 * Real pipeline: sonority_builder (src/semantics/sonority_builder.h)
 * derives Sonority objects from raw onset/release note activity sampled
 * on the fixed quarter-note beat grid, then chord_identification
 * (src/analysis/chord_identification.h) is called against each,
 * unmodified -- this is the requirement 3/4 validation the naive
 * pitch-class check above stands in for until this point.
 *
 * common-practice-minimal-v1's diatonic scale degrees are caller-
 * supplied (chord_identification.h documents this: PitchIdentity has no
 * numeric semantics for the analyzer to derive them itself). This
 * fixture is known (by the Registry-approved expected-chord table
 * above) to stay in C major for the checked measures, so scale degree
 * is computed directly from MIDI pitch class under a fixed C-major
 * mapping -- not a general key-aware scale-degree computation, valid
 * only because the key is already known here, same as every other
 * caller-supplies-the-dimension boundary this codebase already draws.
 * A pitch class outside the C-major diatonic collection (e.g. the D
 * major secondary dominant's F#) maps to the sentinel 0, which cannot
 * match any of common-practice-minimal-v1's seven diatonic triads (all
 * degrees 1-7) -- so that measure is expected to fail identification,
 * not to be forced into a wrong triad.
 */
static const char *EXPECTED_ROMAN[MEASURE_COUNT_CHECKED] = {
    "I", "ii", "V", "I", "vi", NULL /* D major (V/V): not diatonic to C major, no match expected */, "V", "I",
};

static int scale_degree_for_pitch_class(int pitch_class) {
    switch (pitch_class) {
        case 0:
            return 1; /* C */
        case 2:
            return 2; /* D */
        case 4:
            return 3; /* E */
        case 5:
            return 4; /* F */
        case 7:
            return 5; /* G */
        case 9:
            return 6; /* A */
        case 11:
            return 7; /* B */
        default:
            return 0; /* not diatonic to C major -- sentinel, matches no triad */
    }
}

static long measure_index_of(const MusicalTime *pos) {
    return musical_time_numerator(pos) / (long)musical_time_denominator(pos);
}

/* Finds pitch_values[i] for the event whose NoteEvent id equals target_id, by linear scan over the
 * index-paired events/pitch_values arrays midi_importer_import produced. */
static long midi_note_for_event_id(NoteEvent *const *events, PitchNumericValue *const *pitch_values, size_t count,
                                    const EntityID *target_id) {
    for (size_t i = 0; i < count; i++) {
        if (entity_id_equals(note_event_id(events[i]), target_id)) {
            return pitch_numeric_value_position_numerator(pitch_values[i]);
        }
    }
    fprintf(stderr, "sonority event id not found among imported notes\n");
    exit(1);
}

static void run_chord_identification_pipeline(NoteEvent *const *events, PitchNumericValue *const *pitch_values,
                                               size_t count, int *out_failures) {
    Sonority **sonorities;
    size_t sonority_count;
    SonorityBuilderResult builder_result =
        sonority_builder_build_from_notes(events, count, &sonorities, &sonority_count);
    if (builder_result != SONORITY_BUILDER_OK) {
        fprintf(stderr, "sonority_builder_build_from_notes failed: %d\n", builder_result);
        exit(1);
    }

    EntityID *framework_id;
    assert(entity_id_create("framework-common-practice-minimal", &framework_id) == ENTITY_ID_OK);
    Framework *framework;
    assert(framework_create(framework_id, "common-practice-minimal-v1", &framework) == FRAMEWORK_OK);

    for (int m = 0; m < MEASURE_COUNT_CHECKED; m++) {
        /* One representative Sonority per checked measure: the first
         * beat-grid sample point landing in that measure. Harmony in
         * this fixture is static within a measure, so any of its four
         * per-beat samples would agree. */
        const Sonority *sonority = NULL;
        for (size_t i = 0; i < sonority_count; i++) {
            const MusicalTime *start = time_span_start(sonority_time_window(sonorities[i]));
            if (measure_index_of(start) == m) {
                sonority = sonorities[i];
                break;
            }
        }
        if (sonority == NULL) {
            fprintf(stderr, "no sonority found for measure %d\n", m + 1);
            exit(1);
        }

        size_t degree_count = sonority_event_id_count(sonority);
        int *degrees = (int *)malloc(sizeof(int) * degree_count);
        assert(degrees != NULL);
        for (size_t i = 0; i < degree_count; i++) {
            long midi_note = midi_note_for_event_id(events, pitch_values, count, sonority_event_id_at(sonority, i));
            int pitch_class = (int)(((midi_note % 12) + 12) % 12);
            degrees[i] = scale_degree_for_pitch_class(pitch_class);
        }

        ChordIdentificationInput input = {sonority, framework, degrees, degree_count, NULL, 0};
        AnalysisResult *result;
        AnalyzerResult analyze_result = chord_identification_analyze(&input, NULL, NULL, &result);
        free(degrees);

        bool ok;
        if (EXPECTED_ROMAN[m] == NULL) {
            ok = analyze_result == ANALYZER_ERR_ANALYSIS_FAILED;
            printf("measure %d: expected no diatonic match  %s\n", m + 1, ok ? "OK" : "MISMATCH");
        } else {
            ok = false;
            if (analyze_result == ANALYZER_OK) {
                const Interpretation *interpretation = analysis_result_interpretation_at(result, 0);
                size_t claim_size;
                const char *claim =
                    (const char *)value_content(interpretation_claim(interpretation), &claim_size);
                ok = strcmp(claim, EXPECTED_ROMAN[m]) == 0;
                analysis_result_destroy(result);
            }
            printf("measure %d: expected %-4s (chord identification) %s\n", m + 1, EXPECTED_ROMAN[m],
                   ok ? "OK" : "MISMATCH");
        }
        if (!ok) {
            (*out_failures)++;
        }
    }

    framework_destroy(framework);
    for (size_t i = 0; i < sonority_count; i++) {
        sonority_destroy(sonorities[i]);
    }
    free(sonorities);
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

    run_chord_identification_pipeline(events, pitch_values, count, &failures);

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
