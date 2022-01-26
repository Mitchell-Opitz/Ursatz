#include "century_score.h"

#include "standard-counterpoint/counterpoint_pitch.h"

bool sixteenth_century_score_valid(const SixteenthCenturyScore *score) {
    if (score == NULL || score->voices == NULL || score->voice_count == 0 ||
        score->voice_count > SIXTEENTH_CENTURY_MAX_VOICES || score->slots_per_tactus < 2 ||
        score->slots_per_tactus % 2 != 0) {
        return false;
    }
    size_t length = score->voices[0].count;
    if (length == 0 || length > SIXTEENTH_CENTURY_MAX_SLOTS) {
        return false;
    }
    for (size_t v = 0; v < score->voice_count; v++) {
        const StandardCounterpointSlotLine *voice = &score->voices[v];
        if (voice->slots == NULL || voice->count != length) {
            return false;
        }
        for (size_t s = 0; s < length; s++) {
            const StandardCounterpointSlot *slot = &voice->slots[s];
            if (slot->kind == STANDARD_COUNTERPOINT_SLOT_REST) {
                if (slot->pitch != NULL) {
                    return false;
                }
                continue;
            }
            if (slot->pitch == NULL) {
                return false;
            }
            if (slot->kind == STANDARD_COUNTERPOINT_SLOT_TIE) {
                if (s == 0 || voice->slots[s - 1].kind == STANDARD_COUNTERPOINT_SLOT_REST ||
                    !standard_counterpoint_same_pitch(voice->slots[s - 1].pitch, slot->pitch)) {
                    return false;
                }
            }
        }
    }
    return true;
}

size_t sixteenth_century_score_slot_count(const SixteenthCenturyScore *score) {
    return sixteenth_century_score_valid(score) ? score->voices[0].count : 0;
}

SixteenthCenturyPosition sixteenth_century_position_at(const SixteenthCenturyScore *score, size_t slot) {
    size_t tactus = score->slots_per_tactus;
    if (slot % tactus != 0) {
        return SIXTEENTH_CENTURY_POSITION_SUB;
    }
    return (slot / tactus) % 2 == 0 ? SIXTEENTH_CENTURY_POSITION_STRONG : SIXTEENTH_CENTURY_POSITION_WEAK;
}

const PitchSpelling *sixteenth_century_pitch_at(const SixteenthCenturyScore *score, size_t voice, size_t slot) {
    if (!sixteenth_century_score_valid(score) || voice >= score->voice_count || slot >= score->voices[voice].count) {
        return NULL;
    }
    const StandardCounterpointSlot *s = &score->voices[voice].slots[slot];
    return s->kind == STANDARD_COUNTERPOINT_SLOT_REST ? NULL : s->pitch;
}

bool sixteenth_century_attacks_at(const SixteenthCenturyScore *score, size_t voice, size_t slot) {
    if (!sixteenth_century_score_valid(score) || voice >= score->voice_count || slot >= score->voices[voice].count) {
        return false;
    }
    return score->voices[voice].slots[slot].kind == STANDARD_COUNTERPOINT_SLOT_ATTACK;
}

bool sixteenth_century_voice_notes(const SixteenthCenturyScore *score, size_t voice,
                                   SixteenthCenturyNote *out, size_t capacity, size_t *count) {
    if (out == NULL || count == NULL || !sixteenth_century_score_valid(score) || voice >= score->voice_count) {
        return false;
    }
    const StandardCounterpointSlotLine *line = &score->voices[voice];
    size_t n = 0;
    for (size_t s = 0; s < line->count; s++) {
        const StandardCounterpointSlot *slot = &line->slots[s];
        if (slot->kind == STANDARD_COUNTERPOINT_SLOT_ATTACK) {
            if (n >= capacity) {
                return false;
            }
            out[n].start = s;
            out[n].length = 1;
            out[n].pitch = slot->pitch;
            n++;
        } else if (slot->kind == STANDARD_COUNTERPOINT_SLOT_TIE) {
            out[n - 1].length++;
        }
    }
    *count = n;
    return true;
}
