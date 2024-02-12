#include <ursatz_analyzer/analysis_report.h>

#include <stdlib.h>

void analysis_outer_note_destroy(AnalysisOuterNote *note) {
    if (!note) {
        return;
    }
    free(note->pitch_name);
    free(note->degree);
    *note = (AnalysisOuterNote){ANALYSIS_OUTER_ABSENT, 0, 0, false, NULL, NULL};
}

void analysis_roman_numeral_destroy(AnalysisRomanNumeral *roman) {
    if (!roman) {
        return;
    }
    free(roman->numeral);
    free(roman->reason);
    free(roman->applied_numeral);
    free(roman->target_numeral);
    *roman = (AnalysisRomanNumeral){false, NULL, NULL, 0, 0.0, false, NULL, NULL, 0, NULL, 0, false};
}

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
        analysis_outer_note_destroy(&report->entries[i].soprano);
        analysis_outer_note_destroy(&report->entries[i].bass);
        analysis_roman_numeral_destroy(&report->entries[i].roman);
    }
    free(report->entries);
    for (size_t i = 0; i < report->non_chord_tone_count; i++) {
        free(report->non_chord_tones[i].type);
    }
    free(report->non_chord_tones);
    for (size_t i = 0; i < report->key_region_count; i++) {
        free(report->key_regions[i].label);
    }
    free(report->key_regions);
    for (size_t i = 0; i < report->key_change_count; i++) {
        analysis_chord_reading_destroy(&report->key_changes[i].pivot);
    }
    free(report->key_changes);
    report->entries = NULL;
    report->entry_count = 0;
    report->non_chord_tones = NULL;
    report->non_chord_tone_count = 0;
    report->key_regions = NULL;
    report->key_region_count = 0;
    report->key_changes = NULL;
    report->key_change_count = 0;
    report->home_region = 0;
    report->home_duration_numerator = 0;
    report->home_duration_denominator = 1;
}
