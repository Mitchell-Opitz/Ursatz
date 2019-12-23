#include <stdio.h>

#include "pipeline/analysis_pipeline.h"
#include "pipeline/midi_pipeline.h"

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

    printf("Received file: %s\n", path);
    printf("Imported %zu note events\n", notes.count);

    AnalysisReport report;
    if (!run_analysis_pipeline(&notes, &report)) {
        fprintf(stderr, "Error: out of memory while analyzing '%s'\n", path);
        imported_notes_destroy(&notes);
        return 1;
    }

    printf("Analyzed %zu chord regions, %d key regions\n", report.entry_count, report.key_found ? 1 : 0);
    for (size_t i = 0; i < report.entry_count; i++) {
        const AnalysisReportEntry *entry = &report.entries[i];
        printf("  time=%ld/%lu chord=%s key=%s\n", entry->time_numerator, entry->time_denominator,
               entry->chord_label, entry->key_label);
    }

    analysis_report_destroy(&report);
    imported_notes_destroy(&notes);
    return 0;
}
