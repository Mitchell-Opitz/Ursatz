#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ursatz_chain/subset_chord_analyzer.h"
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
 * Reports the subset-match triad-identification fix's real effect on the
 * framework-v1-reference-set corpus (tests/fixtures/framework-v1-corpus/),
 * one beat-grid Sonority at a time -- NOT bucketed into measures.
 *
 * Deliberately does not use measure_index_of() the way
 * test_bach_prelude_arpeggiated_chords.c does: that arithmetic
 * (floor(elapsed-whole-notes)) only equals a real notated measure when
 * the piece is in 4/4, because midi_importer.c hardcodes MusicalTime's
 * denominator to 4*PPQN and never reads the file's time_signature
 * meta-event (see Known_Gaps.md -- filed separately, out of scope for
 * this branch). Field 1.mid is in 12/8, where that arithmetic would
 * silently misgroup notes into the wrong "measure". Checking at the
 * Sonority (beat-grid) level sidesteps the bug entirely: each Sonority
 * is already exactly the per-attack grouping sonority_builder produces,
 * independent of any notated-measure-length assumption.
 *
 * This is an INFORMATIONAL report, not a hard-fail correctness test:
 * only Field 1's key (Eb major, per the piece) is a caller-confirmed
 * tonic. The other four pieces' keys are not yet verified (Phase 2.5
 * corpus-verification work, tracked separately, not a prerequisite for
 * this branch) and are run here under a placeholder C-major tonic
 * purely to exercise the pipeline without crashing -- their printed
 * output is not meaningful and is not asserted against.
 */

#define FIXTURE_DIR "../fixtures/framework-v1-corpus/"

typedef struct {
    const char *filename;
    int tonic_pitch_class; /* 0=C, 3=Eb, etc. */
    bool tonic_verified;
} CorpusPiece;

static const CorpusPiece CORPUS_PIECES[] = {
    {"Bach 114.mid", 0, false},
    {"Clementi 36.mid", 0, false},
    {"Field 1.mid", 3, true}, /* Eb major, per known analysis: I V I I I V I I */
    {"Field 5.mid", 0, false},
    {"Mozart 545.mid", 0, false},
};
#define CORPUS_PIECE_COUNT (sizeof(CORPUS_PIECES) / sizeof(CORPUS_PIECES[0]))

/* Major-scale semitone offsets from tonic -> scale degree (1-7). Offsets
 * not in this table (e.g. chromatic tones) map to the sentinel 0, which
 * cannot match any of common-practice-minimal-v1's seven diatonic triads. */
static int scale_degree_for_offset(int offset_from_tonic) {
    switch (offset_from_tonic) {
        case 0:
            return 1;
        case 2:
            return 2;
        case 4:
            return 3;
        case 5:
            return 4;
        case 7:
            return 5;
        case 9:
            return 6;
        case 11:
            return 7;
        default:
            return 0;
    }
}

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

static void run_piece(const CorpusPiece *piece, Framework *framework) {
    char path[512];
    snprintf(path, sizeof(path), "%s%s", FIXTURE_DIR, piece->filename);

    size_t size = 0;
    unsigned char *data = read_fixture(path, &size);

    NoteEvent **events = NULL;
    PitchNumericValue **pitch_values = NULL;
    size_t count = 0;
    MidiImportResult import_result = midi_importer_import(data, size, &events, &pitch_values, &count);
    free(data);
    assert(import_result == MIDI_IMPORT_OK);
    assert(count > 0);

    Sonority **sonorities;
    size_t sonority_count;
    SonorityBuilderResult builder_result =
        sonority_builder_build_from_notes(events, count, &sonorities, &sonority_count);
    assert(builder_result == SONORITY_BUILDER_OK);
    assert(sonority_count > 0);

    size_t matched = 0;
    printf("-- %s (%zu notes, %zu beat-grid sonorities, tonic pc=%d%s) --\n", piece->filename, count,
           sonority_count, piece->tonic_pitch_class, piece->tonic_verified ? "" : ", UNVERIFIED placeholder");

    for (size_t i = 0; i < sonority_count; i++) {
        const MusicalTime *start = time_span_start(sonority_time_window(sonorities[i]));

        size_t degree_count = sonority_event_id_count(sonorities[i]);
        int *degrees = (int *)malloc(sizeof(int) * degree_count);
        assert(degrees != NULL);
        for (size_t j = 0; j < degree_count; j++) {
            long midi_note =
                midi_note_for_event_id(events, pitch_values, count, sonority_event_id_at(sonorities[i], j));
            int pitch_class = (int)(((midi_note % 12) + 12) % 12);
            int offset = ((pitch_class - piece->tonic_pitch_class) % 12 + 12) % 12;
            degrees[j] = scale_degree_for_offset(offset);
        }

        ChainSubsetChordInput input = {sonorities[i], framework, degrees, degree_count, NULL, 0};
        AnalysisResult *result;
        AnalyzerResult analyze_result = chain_subset_chord_analyze(&input, NULL, NULL, &result);
        free(degrees);

        const char *label = "(no match)";
        if (analyze_result == ANALYZER_OK) {
            matched++;
            const Interpretation *interpretation = analysis_result_interpretation_at(result, 0);
            size_t claim_size;
            label = (const char *)value_content(interpretation_claim(interpretation), &claim_size);
        }
        printf("  beat @ %ld/%ld: %s\n", musical_time_numerator(start), musical_time_denominator(start), label);
        if (analyze_result == ANALYZER_OK) {
            analysis_result_destroy(result);
        }
    }

    printf("-- %s: %zu/%zu beats matched a triad --\n\n", piece->filename, matched, sonority_count);

    for (size_t i = 0; i < sonority_count; i++) {
        sonority_destroy(sonorities[i]);
    }
    free(sonorities);
    for (size_t i = 0; i < count; i++) {
        note_event_destroy(events[i]);
        pitch_numeric_value_destroy(pitch_values[i]);
    }
    free(events);
    free(pitch_values);
}

int main(void) {
    EntityID *framework_id;
    assert(entity_id_create("framework-common-practice-minimal", &framework_id) == ENTITY_ID_OK);
    Framework *framework;
    assert(framework_create(framework_id, "common-practice-minimal-v1", &framework) == FRAMEWORK_OK);

    for (size_t i = 0; i < CORPUS_PIECE_COUNT; i++) {
        run_piece(&CORPUS_PIECES[i], framework);
    }

    framework_destroy(framework);

    printf("framework_v1_corpus_subset_match_report: ran all %zu pieces without error\n", CORPUS_PIECE_COUNT);
    return 0;
}
