#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "pipeline/analysis_pipeline.h"
#include "pipeline/midi_pipeline.h"

/*
 * Regression coverage over the real-world corpus fixtures that exposed
 * the original key-detection bug: a fixed-order pitch-class scan with
 * one-directional collection containment let chromatic content in a
 * piece spuriously match the wrong (earlier-tried) tonic, and had no
 * fallback for flat keys at all. Each fixture below is checked against
 * its actual key, exercising both new paths this fix adds: the MIDI
 * key signature meta-event (Bach Prelude, Mozart 545, Clementi 36,
 * Field 5) and the first/last-chord bass-note fallback for files that
 * carry no key signature (Bach 114, Field 1).
 */

typedef struct {
    const char *path;
    const char *expected_key_label;
} CorpusCase;

static const CorpusCase CASES[] = {
    {"../external/ursatz/tests/fixtures/arpeggiation-investigation/Bach_Prelude_C_Major.mid", "C major"},
    {"../external/ursatz/tests/fixtures/framework-v1-corpus/Bach 114.mid", "G major"},
    {"../external/ursatz/tests/fixtures/framework-v1-corpus/Mozart 545.mid", "C major"},
    {"../external/ursatz/tests/fixtures/framework-v1-corpus/Clementi 36.mid", "C major"},
    {"../external/ursatz/tests/fixtures/framework-v1-corpus/Field 1.mid", "Eb major"},
    {"../external/ursatz/tests/fixtures/framework-v1-corpus/Field 5.mid", "Bb major"},
};

int main(void) {
    for (size_t i = 0; i < sizeof(CASES) / sizeof(CASES[0]); i++) {
        ImportedNotes notes;
        MidiPipelineResult import_result = midi_pipeline_import_file(CASES[i].path, &notes);
        assert(import_result == MIDI_PIPELINE_OK);

        AnalysisReport report;
        assert(run_analysis_pipeline(&notes, &report));
        assert(report.key_found);
        assert(report.entry_count > 0);

        if (strcmp(report.entries[0].key_label, CASES[i].expected_key_label) != 0) {
            fprintf(stderr, "%s: expected key '%s', got '%s'\n", CASES[i].path, CASES[i].expected_key_label,
                    report.entries[0].key_label);
            assert(0);
        }

        analysis_report_destroy(&report);
        imported_notes_destroy(&notes);
    }

    printf("test_corpus_key_estimation: ok\n");
    return 0;
}
