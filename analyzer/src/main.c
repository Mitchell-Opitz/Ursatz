#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "pipeline/analysis_pipeline.h"
#include "pipeline/markdown_report.h"
#include "pipeline/midi_pipeline.h"
#include "pipeline/report_writer.h"

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

    char *markdown = markdown_report_render(path, &report);
    if (!markdown) {
        fprintf(stderr, "Error: out of memory while rendering report for '%s'\n", path);
        analysis_report_destroy(&report);
        imported_notes_destroy(&notes);
        return 1;
    }

    char *output_path = derive_report_path(path);
    if (!output_path) {
        fprintf(stderr, "Error: out of memory while deriving report path for '%s'\n", path);
        free(markdown);
        analysis_report_destroy(&report);
        imported_notes_destroy(&notes);
        return 1;
    }

    bool wrote = write_report_file(output_path, markdown);
    if (wrote) {
        printf("Report written to %s\n", output_path);
    }

    free(output_path);
    free(markdown);
    analysis_report_destroy(&report);
    imported_notes_destroy(&notes);
    return wrote ? 0 : 1;
}
