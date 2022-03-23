#include "fugue_exposition.h"

#include "standard-counterpoint/counterpoint_pitch.h"

static bool voices_valid(const FugueExposition *exposition) {
    if (exposition->voices == NULL || exposition->voice_count < 2 || exposition->voice_count > FUGUE_MAX_VOICES) {
        return false;
    }
    size_t length = exposition->voices[0].count;
    if (length == 0 || length > FUGUE_MAX_SLOTS) {
        return false;
    }
    for (size_t v = 0; v < exposition->voice_count; v++) {
        const StandardCounterpointSlotLine *voice = &exposition->voices[v];
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
            if (slot->kind == STANDARD_COUNTERPOINT_SLOT_TIE &&
                (s == 0 || voice->slots[s - 1].kind == STANDARD_COUNTERPOINT_SLOT_REST ||
                 !standard_counterpoint_same_pitch(voice->slots[s - 1].pitch, slot->pitch))) {
                return false;
            }
        }
    }
    return true;
}

/* The notes of `voice`, without the checks of a valid exposition. */
static bool collect_notes(const StandardCounterpointSlotLine *line, FugueNote *out, size_t capacity,
                          size_t *count) {
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

bool fugue_exposition_valid(const FugueExposition *exposition) {
    if (exposition == NULL || !voices_valid(exposition) || exposition->statements == NULL ||
        exposition->statement_count == 0 || exposition->statement_count > FUGUE_MAX_STATEMENTS ||
        exposition->slots_per_bar == 0) {
        return false;
    }
    size_t previous_start = 0;
    for (size_t i = 0; i < exposition->statement_count; i++) {
        const FugueStatement *statement = &exposition->statements[i];
        if (!fugue_role_valid(statement->role) || statement->voice >= exposition->voice_count ||
            statement->note_count == 0 || statement->start >= exposition->voices[0].count ||
            (i > 0 && statement->start < previous_start)) {
            return false;
        }
        previous_start = statement->start;

        FugueNote notes[FUGUE_MAX_NOTES];
        size_t count;
        if (!collect_notes(&exposition->voices[statement->voice], notes, FUGUE_MAX_NOTES, &count)) {
            return false;
        }
        size_t first = count;
        for (size_t n = 0; n < count; n++) {
            if (notes[n].start == statement->start) {
                first = n;
                break;
            }
        }
        if (first == count || first + statement->note_count > count) {
            return false;
        }
    }
    return true;
}

size_t fugue_exposition_slot_count(const FugueExposition *exposition) {
    return fugue_exposition_valid(exposition) ? exposition->voices[0].count : 0;
}

FugueLimits fugue_exposition_limits(const FugueExposition *exposition) {
    if (exposition != NULL && exposition->limits != NULL) {
        return *exposition->limits;
    }
    return fugue_default_limits();
}

const PitchSpelling *fugue_pitch_at(const FugueExposition *exposition, size_t voice, size_t slot) {
    if (!fugue_exposition_valid(exposition) || voice >= exposition->voice_count ||
        slot >= exposition->voices[voice].count) {
        return NULL;
    }
    const StandardCounterpointSlot *s = &exposition->voices[voice].slots[slot];
    return s->kind == STANDARD_COUNTERPOINT_SLOT_REST ? NULL : s->pitch;
}

bool fugue_attacks_at(const FugueExposition *exposition, size_t voice, size_t slot) {
    if (!fugue_exposition_valid(exposition) || voice >= exposition->voice_count ||
        slot >= exposition->voices[voice].count) {
        return false;
    }
    return exposition->voices[voice].slots[slot].kind == STANDARD_COUNTERPOINT_SLOT_ATTACK;
}

bool fugue_voice_notes(const FugueExposition *exposition, size_t voice, FugueNote *out, size_t capacity,
                       size_t *count) {
    if (out == NULL || count == NULL || !fugue_exposition_valid(exposition) || voice >= exposition->voice_count) {
        return false;
    }
    return collect_notes(&exposition->voices[voice], out, capacity, count);
}

bool fugue_statement_notes(const FugueExposition *exposition, size_t index, FugueNote *out, size_t capacity,
                           size_t *count) {
    if (out == NULL || count == NULL || !fugue_exposition_valid(exposition) ||
        index >= exposition->statement_count) {
        return false;
    }
    const FugueStatement *statement = &exposition->statements[index];
    FugueNote notes[FUGUE_MAX_NOTES];
    size_t total;
    if (statement->note_count > capacity ||
        !collect_notes(&exposition->voices[statement->voice], notes, FUGUE_MAX_NOTES, &total)) {
        return false;
    }
    for (size_t n = 0; n < total; n++) {
        if (notes[n].start == statement->start) {
            for (size_t i = 0; i < statement->note_count; i++) {
                out[i] = notes[n + i];
            }
            *count = statement->note_count;
            return true;
        }
    }
    return false;
}

bool fugue_voice_part(const FugueExposition *exposition, size_t voice, VoicePart *out) {
    if (out == NULL || exposition == NULL || exposition->voice_count != 4 || voice >= 4) {
        return false;
    }
    *out = (VoicePart)voice;
    return true;
}
