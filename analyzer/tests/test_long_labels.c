#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_pipeline.h>
#include <ursatz_analyzer/markdown_report.h>
#include <ursatz_analyzer/midi_pipeline.h>

/*
 * Labels are owned strings, not fixed 32-byte buffers: a report whose
 * first entry carries labels longer than 31 characters (swapped in
 * through the report API) keeps them intact through the analysis
 * document and the Markdown report, and analysis_report_destroy frees
 * them.
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

    ImportedNotes notes;
    assert(midi_pipeline_import_file(URSATZ_FIXTURE_DIR "/framework-v1-corpus/Mozart 545.mid", &notes) ==
           MIDI_PIPELINE_OK);
    AnalysisReport report;
    assert(run_analysis_pipeline(&notes, &report));
    assert(report.key_found);
    assert(report.entry_count > 0);

    free(report.entries[0].key_label);
    free(report.entries[0].chord_label);
    report.entries[0].key_label = copy_string(key_label);
    report.entries[0].chord_label = copy_string(chord_label);

    AnalysisDocument *document = analysis_document_build("long_labels.mid", &notes, &report);
    assert(document != NULL);
    char *markdown = markdown_report_render(document);
    assert(markdown != NULL);
    assert(strstr(markdown, "**Detected key:** C major (long key label beyond thirty-one characters)\n") != NULL);
    assert(strstr(markdown, "- Beat 0.00: I (long chord label beyond thirty-one characters)\n") != NULL);

    free(markdown);
    analysis_document_destroy(document);
    analysis_report_destroy(&report);
    assert(report.entries == NULL);
    assert(report.entry_count == 0);
    imported_notes_destroy(&notes);

    printf("test_long_labels: ok\n");
    return 0;
}
