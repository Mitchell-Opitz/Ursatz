#include <ursatz_analyzer/analysis_report.h>

#include <stdlib.h>

void analysis_chord_reading_destroy(AnalysisChordReading *reading) {
    if (!reading) {
        return;
    }
    free(reading->label);
    free(reading->root);
    free(reading->quality);
    free(reading->bass);
    *reading = (AnalysisChordReading){NULL, NULL, NULL, NULL, -1, 0.0, {false, 0, 0}};
}

void analysis_report_destroy(AnalysisReport *report) {
    if (!report) {
        return;
    }
    for (size_t i = 0; i < report->entry_count; i++) {
        free(report->entries[i].chord_label);
        free(report->entries[i].key_label);
        free(report->entries[i].note_ids);
        analysis_chord_reading_destroy(&report->entries[i].chord);
        analysis_chord_reading_destroy(&report->entries[i].runner_up);
    }
    free(report->entries);
    report->entries = NULL;
    report->entry_count = 0;
    report->key_found = false;
    report->key_method = ANALYSIS_KEY_METHOD_NONE;
    report->key_confidence = (AnalysisConfidence){false, 0, 0};
}
