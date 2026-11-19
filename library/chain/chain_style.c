#include "chain/chain_style.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "model/core/rational.h"
#include "model/time/meter_map.h"
#include "model/time/musical_time.h"
#include "model/time/temporal_anchor.h"
#include "model/time/time_signature.h"
#include "theory/style/style_profile.h"

static bool add_time_signature(StylePiece *piece, const TimeSignature *signature) {
    return style_piece_add_time_signature(piece, (long)time_signature_top_number(signature),
                                          (long)time_signature_bottom_number(signature));
}

/* The meter's signatures: the first full measure's, then each change's (the document's "meter" claim). */
static bool fill_meter(StylePiece *piece, const ImportedScore *score) {
    const MeterMap *map = score->meter.meter_map;
    size_t measures = 0;
    if (!map || meter_map_measure_count(map, &score->meter.end_time, &measures) != METER_MAP_OK) {
        return false;
    }
    size_t primary = meter_map_has_pickup(map) ? 1 : 0;
    if (!add_time_signature(piece, meter_map_region_time_signature(meter_map_region(map, primary)))) {
        return false;
    }
    for (size_t i = primary + 1; i < meter_map_region_count(map); i++) {
        if (!add_time_signature(piece, meter_map_region_time_signature(meter_map_region(map, i)))) {
            return false;
        }
    }
    return true;
}

static bool fill_harmony(StylePiece *piece, const ChainResult *report) {
    for (size_t i = 0; i < report->entry_count; i++) {
        const ChainEntry *entry = &report->entries[i];
        bool in_region = entry->in_region && entry->region < report->key_region_count;
        if (!style_piece_add_chord(piece, entry->roman.base_numeral, in_region, in_region ? (long)entry->region : 0)) {
            return false;
        }
    }
    for (size_t i = 0; i < report->cadence_count; i++) {
        if (!style_piece_add_cadence_type(piece, report->cadences[i].type)) {
            return false;
        }
    }
    for (size_t i = 0; i < report->harmonic_rhythm_count; i++) {
        if (!style_piece_add_harmonic_rhythm(piece, (long)report->harmonic_rhythm[i].changes)) {
            return false;
        }
    }
    return true;
}

static bool fill_rhythm(StylePiece *piece, const ChainResult *report, const ImportedScore *score) {
    if (!fill_meter(piece, score)) {
        return false;
    }
    style_piece_set_tempo(piece, score->source.tempo_declared, score->source.initial_quarters_per_minute);
    for (size_t i = 0; i < report->density_count; i++) {
        int line = report->densities[i].line;
        if ((line < 0 || line > 2) && !style_piece_add_density(piece, report->densities[i].attacks_per_beat_mean)) {
            return false;
        }
    }
    for (size_t i = 0; i < report->syncopation_count; i++) {
        if (report->syncopations[i].line == 0 && !style_piece_add_syncopation(piece, report->syncopations[i].measure)) {
            return false;
        }
    }
    for (size_t i = 0; i < report->phrase_count; i++) {
        const AnalysisPhrase *phrase = &report->phrases[i];
        if (!style_piece_add_phrase(piece, phrase->first_measure, phrase->end_measure, phrase->length_measures)) {
            return false;
        }
    }
    return true;
}

/* The soprano of every voice segment, a note's length as its end less its onset. */
static bool fill_soprano(StylePiece *piece, const ChainResult *report, const ImportedScore *score) {
    for (size_t i = 0; i < report->outer_line_count; i++) {
        const AnalysisOuterNote *voice = &report->outer_lines[i].soprano;
        StylePieceSopranoNote note = {0};
        if (voice->status == ANALYSIS_OUTER_PRESENT) {
            const NoteEvent *event = score->events[voice->note_id];
            Rational onset = temporal_anchor_position(note_event_temporal_anchor(event))->value;
            Rational end, length;
            if (rational_add(onset, note_event_duration(event)->value, &end) != RATIONAL_OK ||
                rational_sub(end, onset, &length) != RATIONAL_OK) {
                return false;
            }
            note = (StylePieceSopranoNote){true, (long)voice->note_id, (long)voice->pitch, true, length};
        }
        if (!style_piece_add_soprano_note(piece, &note)) {
            return false;
        }
    }
    return true;
}

static bool fill_motive(StylePiece *piece, const AnalysisMotive *motive) {
    StylePieceMotive *family = style_piece_add_motive(piece);
    if (!family) {
        return false;
    }
    if (motive->has_features) {
        const AnalysisMotiveFeatures *features = &motive->features;
        size_t count = features->note_count;
        char (*texts)[48] = calloc(count > 0 ? count : 1, sizeof *texts);
        const char **rhythm = calloc(count > 0 ? count : 1, sizeof *rhythm);
        bool ok = texts && rhythm;
        for (size_t k = 0; ok && k < count; k++) {
            snprintf(texts[k], sizeof texts[k], "%llu/%llu", (unsigned long long)features->notes[k].duration.num,
                     (unsigned long long)features->notes[k].duration.den);
            rhythm[k] = texts[k];
        }
        ok = ok && style_piece_motive_set_features(family, features->degrees, count, features->intervals,
                                                   count > 0 ? count - 1 : 0, rhythm, count);
        free(texts);
        free(rhythm);
        if (!ok) {
            return false;
        }
    }
    for (size_t o = 0; o < motive->occurrence_count; o++) {
        const AnalysisMotiveOccurrence *occurrence = &motive->occurrences[o];
        size_t count = occurrence->note_id_count;
        long *ids = malloc((count > 0 ? count : 1) * sizeof *ids);
        if (!ids) {
            return false;
        }
        for (size_t k = 0; k < count; k++) {
            ids[k] = (long)occurrence->note_ids[k];
        }
        bool ok = style_piece_motive_add_occurrence(family, ids, occurrence->degrees, count);
        free(ids);
        if (!ok) {
            return false;
        }
    }
    return true;
}

static bool fill_melody(StylePiece *piece, const ChainResult *report, const ImportedScore *score) {
    if (!fill_soprano(piece, report, score)) {
        return false;
    }
    for (size_t i = 0; i < report->non_chord_tone_count; i++) {
        const AnalysisNonChordTone *tone = &report->non_chord_tones[i];
        if (tone->type && !style_piece_add_non_chord_tone(piece, (long)tone->note_id, tone->type)) {
            return false;
        }
    }
    for (size_t i = 0; i < report->motive_count; i++) {
        if (!fill_motive(piece, &report->motives[i])) {
            return false;
        }
    }
    for (size_t i = 0; i < report->melody_count; i++) {
        if (!style_piece_add_melody_range(piece, report->melodies[i].range_semitones)) {
            return false;
        }
    }
    return true;
}

static bool fill_form(StylePiece *piece, const ChainResult *report) {
    for (size_t i = 0; i < report->theme_count; i++) {
        if (report->themes[i].form && !style_piece_add_theme_label(piece, report->themes[i].form)) {
            return false;
        }
    }
    for (size_t i = 0; i < report->section_count; i++) {
        if (!style_piece_add_section_label(piece, report->sections[i].label)) {
            return false;
        }
    }
    if (report->key_region_count > 0) {
        return style_piece_set_home_key_label(piece, report->key_regions[report->home_region].label);
    }
    return true;
}

bool chain_style_piece(const AnalysisChain *chain, StylePiece *out) {
    if (!out) {
        return false;
    }
    memset(out, 0, sizeof *out);
    const ImportedScore *score = chain ? analysis_chain_score(chain) : NULL;
    if (!score) {
        return false;
    }
    const ChainResult *report = analysis_chain_result(chain);
    if (!fill_harmony(out, report) || !fill_rhythm(out, report, score) || !fill_melody(out, report, score) ||
        !fill_form(out, report)) {
        style_piece_destroy(out);
        return false;
    }
    return true;
}

StyleReferenceResult chain_style_distance(const AnalysisChain *chain, const StyleReference *reference,
                                          StyleComparison *out) {
    if (!chain || !reference || !out) {
        return STYLE_REFERENCE_ERR_ARGUMENT;
    }
    memset(out, 0, sizeof *out);
    if (!analysis_chain_score(chain)) {
        return STYLE_REFERENCE_ERR_ARGUMENT;
    }
    StyleReferenceResult result = STYLE_REFERENCE_ERR_ALLOC;
    StylePiece piece = {0};
    StyleProfile *profile = style_profile_create();
    StyleStatistic *statistics = NULL;
    size_t count = 0;
    if (profile && chain_style_piece(chain, &piece) && style_profile_add_piece(profile, &piece) == STYLE_PROFILE_OK &&
        style_profile_statistics(profile, &statistics, &count) == STYLE_PROFILE_OK) {
        result = style_reference_distance(reference, statistics, count, out);
    }
    free(statistics);
    style_piece_destroy(&piece);
    style_profile_destroy(profile);
    return result;
}
