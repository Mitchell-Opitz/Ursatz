#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_pipeline.h>
#include <io/imported_score.h>
#include "test_knowledge.h"

/*
 * Labels are owned strings, not fixed 32-byte buffers: a report whose
 * first entry carries labels longer than 31 characters (swapped in
 * through the report API) keeps them intact through the analysis
 * document, and analysis_report_destroy frees them.
 */

static char *copy_string(const char *s) {
    size_t size = strlen(s) + 1;
    char *copy = malloc(size);
    assert(copy != NULL);
    memcpy(copy, s, size);
    return copy;
}

int main(void) {
    const char *key_label = "C major (long key label beyond thirty-one characters)";
    const char *chord_label = "I (long chord label beyond thirty-one characters)";
    assert(strlen(key_label) > 31);
    assert(strlen(chord_label) > 31);

    ImportedScore notes;
    assert(imported_score_import_file(URSATZ_FIXTURES_DIR "/framework-v1-corpus/Mozart 545.mid", IMPORTED_FORMAT_MIDI, &notes, NULL) ==
           IMPORTED_SCORE_OK);
    AnalysisReport report;
    assert(run_analysis_pipeline(&notes, test_knowledge(), &report));
    assert(report.key_region_count > 0);
    assert(report.entry_count > 0);

    free(report.key_regions[report.home_region].label);
    report.key_regions[report.home_region].label = copy_string(key_label);
    free(report.entries[0].key_label);
    free(report.entries[0].chord_label);
    report.entries[0].key_label = copy_string(key_label);
    report.entries[0].chord_label = copy_string(chord_label);

    AnalysisDocument *document = analysis_document_build("long_labels.mid", &notes, &report, test_knowledge());
    assert(document != NULL);
    char *json = analysis_document_to_json(document);
    assert(json);
    assert(strstr(json, key_label));
    assert(strstr(json, chord_label));
    free(json);
    analysis_document_destroy(document);
    analysis_report_destroy(&report);
    assert(report.entries == NULL);
    assert(report.entry_count == 0);
    imported_score_destroy(&notes);

    printf("test_long_labels: ok\n");
    return 0;
}
