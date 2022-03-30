#include "ground_piece.h"

#include "standard-counterpoint/counterpoint_pitch.h"

static bool voices_valid(const GroundBassPiece *piece) {
    if (piece->voices == NULL || piece->voice_count < 2 || piece->voice_count > GROUND_MAX_VOICES) {
        return false;
    }
    size_t length = piece->voices[0].count;
    if (length == 0 || length > GROUND_MAX_SLOTS) {
        return false;
    }
    for (size_t v = 0; v < piece->voice_count; v++) {
        const StandardCounterpointSlotLine *voice = &piece->voices[v];
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

/* The notes of a line, without the checks of a valid piece. */
static bool collect_notes(const StandardCounterpointSlotLine *line, GroundNote *out, size_t capacity,
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

bool ground_piece_valid(const GroundBassPiece *piece) {
    if (piece == NULL || !voices_valid(piece) || piece->bass_voice >= piece->voice_count ||
        piece->slots_per_iteration == 0 || piece->slots_per_bar == 0 || piece->beats_per_bar == 0 ||
        !ostinato_type_valid(piece->type) || piece->statement_count > GROUND_MAX_STATEMENTS ||
        (piece->statement_count > 0 && piece->statements == NULL)) {
        return false;
    }
    size_t previous_start = 0;
    for (size_t i = 0; i < piece->statement_count; i++) {
        const GroundStatement *statement = &piece->statements[i];
        if (!imitative_role_valid(statement->role) || statement->voice >= piece->voice_count ||
            statement->note_count == 0 || statement->start >= piece->voices[0].count ||
            (i > 0 && statement->start < previous_start)) {
            return false;
        }
        previous_start = statement->start;

        GroundNote notes[GROUND_MAX_NOTES];
        size_t count;
        if (!collect_notes(&piece->voices[statement->voice], notes, GROUND_MAX_NOTES, &count)) {
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

size_t ground_piece_slot_count(const GroundBassPiece *piece) {
    return ground_piece_valid(piece) ? piece->voices[0].count : 0;
}

size_t ground_piece_iteration_count(const GroundBassPiece *piece) {
    return ground_piece_valid(piece) ? piece->voices[0].count / piece->slots_per_iteration : 0;
}

GroundLimits ground_piece_limits(const GroundBassPiece *piece) {
    if (piece != NULL && piece->limits != NULL) {
        return *piece->limits;
    }
    return ground_default_limits();
}

const PitchSpelling *ground_pitch_at(const GroundBassPiece *piece, size_t voice, size_t slot) {
    if (!ground_piece_valid(piece) || voice >= piece->voice_count || slot >= piece->voices[voice].count) {
        return NULL;
    }
    const StandardCounterpointSlot *s = &piece->voices[voice].slots[slot];
    return s->kind == STANDARD_COUNTERPOINT_SLOT_REST ? NULL : s->pitch;
}

bool ground_attacks_at(const GroundBassPiece *piece, size_t voice, size_t slot) {
    if (!ground_piece_valid(piece) || voice >= piece->voice_count || slot >= piece->voices[voice].count) {
        return false;
    }
    return piece->voices[voice].slots[slot].kind == STANDARD_COUNTERPOINT_SLOT_ATTACK;
}

bool ground_voice_notes(const GroundBassPiece *piece, size_t voice, GroundNote *out, size_t capacity,
                        size_t *count) {
    if (out == NULL || count == NULL || !ground_piece_valid(piece) || voice >= piece->voice_count) {
        return false;
    }
    return collect_notes(&piece->voices[voice], out, capacity, count);
}

bool ground_statement_notes(const GroundBassPiece *piece, size_t index, GroundNote *out, size_t capacity,
                            size_t *count) {
    if (out == NULL || count == NULL || !ground_piece_valid(piece) || index >= piece->statement_count) {
        return false;
    }
    const GroundStatement *statement = &piece->statements[index];
    GroundNote notes[GROUND_MAX_NOTES];
    size_t total;
    if (statement->note_count > capacity ||
        !collect_notes(&piece->voices[statement->voice], notes, GROUND_MAX_NOTES, &total)) {
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
