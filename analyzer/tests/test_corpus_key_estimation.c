#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <ursatz_analyzer/analysis_pipeline.h>
#include <ursatz_analyzer/midi_pipeline.h>

/*
 * The home key of the six reference pieces: the longest of the key regions Ursatz's key-region detection finds.
 * Each piece's home key is its actual key. Four of the files carry a key signature (Bach Prelude, Mozart 545,
 * Clementi 36, Field 5), which the library takes as a boundary; Bach 114 and Field 1 carry none.
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
        assert(report.key_region_count > 0);
        assert(report.entry_count > 0);

        const char *home_key = report.key_regions[report.home_region].label;
        if (strcmp(home_key, CASES[i].expected_key_label) != 0) {
            fprintf(stderr, "%s: expected home key '%s', got '%s'\n", CASES[i].path, CASES[i].expected_key_label,
                    home_key);
            assert(0);
        }

        analysis_report_destroy(&report);
        imported_notes_destroy(&notes);
    }

    printf("test_corpus_key_estimation: ok\n");
    return 0;
}
