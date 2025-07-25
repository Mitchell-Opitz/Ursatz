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
    free(roman->base_numeral);
    free(roman->applied_numeral);
    free(roman->target_numeral);
    *roman = (AnalysisRomanNumeral){false, NULL, NULL, NULL, 0, 0.0, false, NULL, NULL, 0, NULL, 0, false};
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
    for (size_t i = 0; i < report->cadence_count; i++) {
        free(report->cadences[i].type);
        free(report->cadences[i].soprano_degree);
    }
    free(report->cadences);
    for (size_t i = 0; i < report->prolongation_count; i++) {
        free(report->prolongations[i].reasons);
    }
    free(report->prolongations);
    for (size_t i = 0; i < report->progression_count; i++) {
        for (size_t n = 0; n < report->progressions[i].length; n++) {
            free(report->progressions[i].pattern[n]);
        }
        free(report->progressions[i].pattern);
        free(report->progressions[i].occurrences);
    }
    free(report->progressions);
    free(report->harmonic_rhythm);
    free(report->cadence_approaches);
    free(report->phrases);
    for (size_t i = 0; i < report->idea_count; i++) {
        free(report->ideas[i].note_ids);
    }
    free(report->ideas);
    for (size_t i = 0; i < report->subphrase_count; i++) {
        free(report->subphrases[i].note_ids);
    }
    free(report->subphrases);
    for (size_t i = 0; i < report->theme_count; i++) {
        free(report->themes[i].note_ids);
    }
    free(report->themes);
    for (size_t i = 0; i < report->expansion_count; i++) {
        free(report->expansions[i].note_ids);
    }
    free(report->expansions);
    free(report->sections);
    free(report->thematic_returns);
    for (size_t i = 0; i < report->motive_count; i++) {
        for (size_t o = 0; o < report->motives[i].occurrence_count; o++) {
            free(report->motives[i].occurrences[o].note_ids);
        }
        free(report->motives[i].occurrences);
    }
    free(report->motives);
    report->entries = NULL;
    report->entry_count = 0;
    report->non_chord_tones = NULL;
    report->non_chord_tone_count = 0;
    report->key_regions = NULL;
    report->key_region_count = 0;
    report->key_changes = NULL;
    report->key_change_count = 0;
    report->cadences = NULL;
    report->cadence_count = 0;
    report->prolongations = NULL;
    report->prolongation_count = 0;
    report->progressions = NULL;
    report->progression_count = 0;
    report->harmonic_rhythm = NULL;
    report->harmonic_rhythm_count = 0;
    report->cadence_approaches = NULL;
    report->cadence_approach_count = 0;
    report->phrases = NULL;
    report->phrase_count = 0;
    report->ideas = NULL;
    report->idea_count = 0;
    report->subphrases = NULL;
    report->subphrase_count = 0;
    report->themes = NULL;
    report->theme_count = 0;
    report->expansions = NULL;
    report->expansion_count = 0;
    report->sections = NULL;
    report->section_count = 0;
    report->thematic_returns = NULL;
    report->thematic_return_count = 0;
    report->motives = NULL;
    report->motive_count = 0;
    report->home_region = 0;
    report->home_duration_numerator = 0;
    report->home_duration_denominator = 1;
}
