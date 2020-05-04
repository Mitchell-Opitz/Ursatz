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
#include "time/musical_time.h"
#include "time/time_span.h"

/*
 * Regression check for arpeggiated chord grouping: sonority_builder
 * (src/semantics/sonority_builder.h) derives Sonority objects from this
 * fixture's raw NoteEvents by grouping notes onto a fixed quarter-note
 * beat grid (one Sonority per beat, formed from whichever notes onset
 * within it), and each beat-grid Sonority is fed unmodified into
 * chord_identification (src/analysis/chord_identification.h). Checks
 * the result against the known-correct harmonic analysis of the
 * fixture's first 8 measures. See sonority_builder.h and
 * docs/modules/semantics.md for why the beat grid is fixed rather than
 * adaptive (Registry Problem A, not Problem B/harmonic-rhythm
 * detection) and out of scope for non-chord-tone tolerance.
 */

#define FIXTURE_PATH "../fixtures/arpeggiation-investigation/Bach_Prelude_C_Major.mid"
#define MEASURE_COUNT_CHECKED 8

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

/*
 * common-practice-minimal-v1's diatonic scale degrees are caller-
 * supplied (chord_identification.h documents this: PitchIdentity has no
 * numeric semantics for the analyzer to derive them itself). This
 * fixture is known to stay in C major for the checked measures (C Dm G
 * C Am D-major-as-V/V G C), so scale degree is computed directly from
 * MIDI pitch class under a fixed C-major mapping -- not a general
 * key-aware scale-degree computation, valid only because the key is
 * already known here, same as every other caller-supplies-the-dimension
 * boundary this codebase already draws. A pitch class outside the
 * C-major diatonic collection (e.g. the D major secondary dominant's
 * F#) maps to the sentinel 0, which cannot match any of
 * common-practice-minimal-v1's seven diatonic triads (all degrees 1-7)
 * -- so that measure is expected to fail identification, not to be
 * forced into a wrong triad.
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
        /* Each beat-grid Sonority in the measure is fed to
         * chord_identification independently (this fixture's harmony
         * is static within a measure, but a given beat's own note
         * content can still include a spilled-over neighbor/suspension
         * tone from an adjacent beat, which breaks
         * common_practice_identify_triad's exact-3-distinct-degree
         * match for THAT beat alone -- triad_identification.h; this
         * branch does not attempt to filter such non-chord tones,
         * System Registry Problem B, explicitly out of scope). The
         * measure matches its expected chord if AT LEAST ONE of its
         * beats resolves to it -- the notated harmony is stated
         * cleanly at some point in nearly every beat of an arpeggiated
         * texture, even when a particular beat's own sample is
         * contaminated. A measure with no expected match (measure 6:
         * the D major secondary dominant, not diatonic to C major)
         * requires that NONE of its beats resolve to anything.
         */
        bool measure_matched_expected = false;
        bool measure_matched_anything = false;
        bool found_sonority_in_measure = false;
        for (size_t i = 0; i < sonority_count; i++) {
            const MusicalTime *start = time_span_start(sonority_time_window(sonorities[i]));
            if (measure_index_of(start) != m) {
                continue;
            }
            found_sonority_in_measure = true;

            size_t degree_count = sonority_event_id_count(sonorities[i]);
            int *degrees = (int *)malloc(sizeof(int) * degree_count);
            assert(degrees != NULL);
            for (size_t j = 0; j < degree_count; j++) {
                long midi_note =
                    midi_note_for_event_id(events, pitch_values, count, sonority_event_id_at(sonorities[i], j));
                int pitch_class = (int)(((midi_note % 12) + 12) % 12);
                degrees[j] = scale_degree_for_pitch_class(pitch_class);
            }

            ChordIdentificationInput input = {sonorities[i], framework, degrees, degree_count, NULL, 0};
            AnalysisResult *result;
            AnalyzerResult analyze_result = chord_identification_analyze(&input, NULL, NULL, &result);
            free(degrees);

            if (analyze_result == ANALYZER_OK) {
                measure_matched_anything = true;
                const Interpretation *interpretation = analysis_result_interpretation_at(result, 0);
                size_t claim_size;
                const char *claim = (const char *)value_content(interpretation_claim(interpretation), &claim_size);
                if (EXPECTED_ROMAN[m] != NULL && strcmp(claim, EXPECTED_ROMAN[m]) == 0) {
                    measure_matched_expected = true;
                }
                analysis_result_destroy(result);
            }
        }
        if (!found_sonority_in_measure) {
            fprintf(stderr, "no sonority found for measure %d\n", m + 1);
            exit(1);
        }

        bool ok = EXPECTED_ROMAN[m] == NULL ? !measure_matched_anything : measure_matched_expected;
        if (EXPECTED_ROMAN[m] == NULL) {
            printf("measure %d: expected no diatonic match  %s\n", m + 1, ok ? "OK" : "MISMATCH");
        } else {
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

    int failures = 0;
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
