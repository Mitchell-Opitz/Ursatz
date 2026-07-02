#include <ursatz_analyzer/analysis_report.h>

#include <stdlib.h>
#include <string.h>

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
        free(report->cadences[i].alternative.type);
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
        free(report->motives[i].development);
    }
    free(report->motives);
    for (size_t i = 0; i < report->melody_count; i++) {
        free(report->melodies[i].note_ids);
        free(report->melodies[i].directions);
        free(report->melodies[i].leaps);
        free(report->melodies[i].consecutive_leaps);
        free(report->melodies[i].ncts);
        free(report->melodies[i].seventh_breaks);
    }
    free(report->melodies);
    for (size_t i = 0; i < report->density_count; i++) {
        free(report->densities[i].note_ids);
    }
    free(report->densities);
    free(report->syncopations);
    for (size_t i = 0; i < report->texture_passage_count; i++) {
        free(report->texture_passages[i].note_ids);
    }
    free(report->texture_passages);
    free(report->texture_changes);
    for (size_t i = 0; i < report->tempo_marking_count; i++) {
        free(report->tempo_markings[i].text);
    }
    free(report->tempo_markings);
    for (size_t i = 0; i < report->dynamic_count; i++) {
        free(report->dynamics[i].marking);
    }
    free(report->dynamics);
    free(report->counterpoint_check.violations);
    free(report->counterpoint_check.note_ids);
    free(report->voice_leading.pairs);
    free(report->voice_leading.violations);
    free(report->voice_leading.resolutions);
    free(report->voice_leading.chords);
    for (size_t i = 0; i < report->schema_count; i++) {
        free(report->schemas[i].name);
        free(report->schemas[i].stages);
    }
    free(report->schemas);
    for (size_t i = 0; i < report->sequence_count; i++) {
        free(report->sequences[i].stages);
    }
    free(report->sequences);
    for (size_t i = 0; i < report->hemiola_count; i++) {
        free(report->hemiolas[i].note_ids);
    }
    free(report->hemiolas);
    for (size_t i = 0; i < report->hypermeter_count; i++) {
        free(report->hypermeters[i].hypermeasures);
        free(report->hypermeters[i].note_ids);
    }
    free(report->hypermeters);
    for (size_t i = 0; i < report->tuplet_count; i++) {
        free(report->tuplets[i].note_ids);
    }
    free(report->tuplets);
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
    report->melodies = NULL;
    report->melody_count = 0;
    report->densities = NULL;
    report->density_count = 0;
    report->syncopations = NULL;
    report->syncopation_count = 0;
    report->hemiolas = NULL;
    report->hemiola_count = 0;
    report->meter_check = (AnalysisMeterCheck){0};
    report->hypermeters = NULL;
    report->hypermeter_count = 0;
    report->tuplets = NULL;
    report->tuplet_count = 0;
    report->texture_passages = NULL;
    report->texture_passage_count = 0;
    report->texture_changes = NULL;
    report->texture_change_count = 0;
    report->tempo_markings = NULL;
    report->tempo_marking_count = 0;
    report->dynamics = NULL;
    report->dynamic_count = 0;
    memset(&report->voice_leading, 0, sizeof(report->voice_leading));
    report->schemas = NULL;
    report->schema_count = 0;
    report->sequences = NULL;
    report->sequence_count = 0;
    memset(&report->counterpoint_check, 0, sizeof(report->counterpoint_check));
    report->home_region = 0;
    report->home_duration_numerator = 0;
    report->home_duration_denominator = 1;
}
