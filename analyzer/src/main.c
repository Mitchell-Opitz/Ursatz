#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_pipeline.h>
#include <ursatz_analyzer/markdown_report.h>
#include <ursatz_analyzer/midi_pipeline.h>
#include <ursatz_analyzer/report_writer.h>

static bool write_markdown_report(const char *path, const AnalysisReport *report) {
    char *markdown = markdown_report_render(path, report);
    if (!markdown) {
        fprintf(stderr, "Error: out of memory while rendering report for '%s'\n", path);
        return false;
    }

    char *output_path = derive_report_path(path);
    if (!output_path) {
        fprintf(stderr, "Error: out of memory while deriving report path for '%s'\n", path);
        free(markdown);
        return false;
    }

    bool wrote = write_report_file(output_path, markdown);
    if (wrote) {
        printf("Report written to %s\n", output_path);
    }

    free(output_path);
    free(markdown);
    return wrote;
}

static bool write_analysis_document(const char *path, const ImportedNotes *notes, const AnalysisReport *report) {
    char *document = analysis_document_render(path, notes, report);
    if (!document) {
        fprintf(stderr, "Error: out of memory while rendering analysis document for '%s'\n", path);
        return false;
    }

    char *output_path = derive_analysis_document_path(path);
    if (!output_path) {
        fprintf(stderr, "Error: out of memory while deriving analysis document path for '%s'\n", path);
        free(document);
        return false;
    }

    bool wrote = write_report_file(output_path, document);
    if (wrote) {
        printf("Analysis document written to %s\n", output_path);
    }

    free(output_path);
    free(document);
    return wrote;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <midi-file-path>\n", argv[0]);
        return 1;
    }

    const char *path = argv[1];
    ImportedNotes notes;
    if (midi_pipeline_import_file(path, &notes) != MIDI_PIPELINE_OK) {
        return 1;
    }

    AnalysisReport report;
    if (!run_analysis_pipeline(&notes, &report)) {
        fprintf(stderr, "Error: out of memory while analyzing '%s'\n", path);
        imported_notes_destroy(&notes);
        return 1;
    }

    bool ok = write_markdown_report(path, &report) && write_analysis_document(path, &notes, &report);

    analysis_report_destroy(&report);
    imported_notes_destroy(&notes);
    return ok ? 0 : 1;
}
