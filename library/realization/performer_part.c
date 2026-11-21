#include "realization/performer_part.h"

#include <stdlib.h>
#include <string.h>

#include "realization/performer_part_internal.h"

RealizationResult performer_part_create(const char *id, const char *instrument_id, const char *name, int number, int players,
                                        int staff_count, int midi_program, PerformerPart **out) {
    if (id == NULL || instrument_id == NULL || name == NULL || out == NULL) {
        return REALIZATION_ERR_NULL;
    }
    if (!realization_id_valid(id) || !realization_id_valid(instrument_id)) {
        return REALIZATION_ERR_BAD_ID;
    }
    size_t name_length = strlen(name);
    if (name_length == 0 || name_length > sizeof(((PerformerPart *)0)->name) - 1 || number < 1 || players < 1 || staff_count < 1 ||
        staff_count > 4 || midi_program < 0 || midi_program > 127) {
        return REALIZATION_ERR_BAD_VALUE;
    }
    PerformerPart *part = calloc(1, sizeof(*part));
    if (part == NULL) {
        return REALIZATION_ERR_ALLOC;
    }
    strcpy(part->id, id);
    strcpy(part->instrument, instrument_id);
    strcpy(part->name, name);
    part->number = number;
    part->players = players;
    part->staff_count = staff_count;
    part->midi_program = midi_program;
    *out = part;
    return REALIZATION_OK;
}

void performer_part_destroy(PerformerPart *part) {
    if (part == NULL) {
        return;
    }
    meter_map_destroy(part->local_meter_map);
    free(part->notes);
    free(part->pedals);
    free(part);
}

static const PerformerNote *note_named(const PerformerPart *part, const char *note_id) {
    for (size_t i = 0; i < part->note_count; i++) {
        if (strcmp(part->notes[i].id, note_id) == 0) {
            return &part->notes[i];
        }
    }
    return NULL;
}

RealizationResult performer_part_add_note(PerformerPart *part, const PerformerNote *note) {
    if (part == NULL || note == NULL) {
        return REALIZATION_ERR_NULL;
    }
    if (!realization_id_valid(note->id)) {
        return REALIZATION_ERR_BAD_ID;
    }
    if (note_named(part, note->id) != NULL) {
        return REALIZATION_ERR_DUPLICATE_ID;
    }
    if (note->staff < 1 || note->staff > part->staff_count || note->voice < 1 || note->midi < 0 || note->midi > 127 ||
        note->onset.value.num < 0 || note->onset.value.den <= 0 || note->duration.value.num <= 0 ||
        note->duration.value.den <= 0 || note->derivation.source_count > NOTE_DERIVATION_SOURCES_MAX) {
        return REALIZATION_ERR_BAD_VALUE;
    }
    for (size_t i = 0; i < note->derivation.source_count; i++) {
        if (!realization_id_valid(note->derivation.sources[i])) {
            return REALIZATION_ERR_BAD_VALUE;
        }
    }
    if (note->spelled && ((int)note->letter < (int)PITCH_LETTER_A || (int)note->letter > (int)PITCH_LETTER_G ||
                          note->accidental < -2 || note->accidental > 2)) {
        return REALIZATION_ERR_BAD_VALUE;
    }
    if (part->note_count == part->note_capacity) {
        size_t capacity = part->note_capacity == 0 ? 16 : part->note_capacity * 2;
        PerformerNote *grown = realloc(part->notes, capacity * sizeof(PerformerNote));
        if (grown == NULL) {
            return REALIZATION_ERR_ALLOC;
        }
        part->notes = grown;
        part->note_capacity = capacity;
    }
    part->notes[part->note_count++] = *note;
    return REALIZATION_OK;
}

RealizationResult performer_part_add_pedal(PerformerPart *part, const PedalMarking *pedal) {
    if (part == NULL || pedal == NULL) {
        return REALIZATION_ERR_NULL;
    }
    if (pedal->kind != PEDAL_SUSTAIN || !time_span_holds(pedal->span) || pedal->span.start.value.num < 0) {
        return REALIZATION_ERR_BAD_VALUE;
    }
    size_t at = part->pedal_count;
    for (size_t i = 0; i < part->pedal_count; i++) {
        const TimeSpan *other = &part->pedals[i].span;
        if (musical_time_cmp(pedal->span.start, other->end) < 0 && musical_time_cmp(other->start, pedal->span.end) < 0) {
            return REALIZATION_ERR_BAD_VALUE;
        }
        if (at == part->pedal_count && musical_time_cmp(pedal->span.start, other->start) < 0) {
            at = i;
        }
    }
    if (part->pedal_count == part->pedal_capacity) {
        size_t capacity = part->pedal_capacity == 0 ? 4 : part->pedal_capacity * 2;
        PedalMarking *grown = realloc(part->pedals, capacity * sizeof(PedalMarking));
        if (grown == NULL) {
            return REALIZATION_ERR_ALLOC;
        }
        part->pedals = grown;
        part->pedal_capacity = capacity;
    }
    memmove(&part->pedals[at + 1], &part->pedals[at], (part->pedal_count - at) * sizeof(PedalMarking));
    part->pedals[at] = *pedal;
    part->pedal_count++;
    return REALIZATION_OK;
}

RealizationResult performer_part_set_local_meter_map(PerformerPart *part, MeterMap *map) {
    if (part == NULL) {
        return REALIZATION_ERR_NULL;
    }
    if (part->local_meter_map != map) {
        meter_map_destroy(part->local_meter_map);
    }
    part->local_meter_map = map;
    return REALIZATION_OK;
}

const char *performer_part_id(const PerformerPart *part) {
    return part != NULL ? part->id : NULL;
}

const char *performer_part_instrument(const PerformerPart *part) {
    return part != NULL ? part->instrument : NULL;
}

const char *performer_part_name(const PerformerPart *part) {
    return part != NULL ? part->name : NULL;
}

int performer_part_number(const PerformerPart *part) {
    return part != NULL ? part->number : 0;
}

int performer_part_players(const PerformerPart *part) {
    return part != NULL ? part->players : 0;
}

int performer_part_staff_count(const PerformerPart *part) {
    return part != NULL ? part->staff_count : 0;
}

int performer_part_midi_program(const PerformerPart *part) {
    return part != NULL ? part->midi_program : 0;
}

size_t performer_part_note_count(const PerformerPart *part) {
    return part != NULL ? part->note_count : 0;
}

const PerformerNote *performer_part_note_at(const PerformerPart *part, size_t index) {
    return part != NULL && index < part->note_count ? &part->notes[index] : NULL;
}

size_t performer_part_pedal_count(const PerformerPart *part) {
    return part != NULL ? part->pedal_count : 0;
}

const PedalMarking *performer_part_pedal_at(const PerformerPart *part, size_t index) {
    return part != NULL && index < part->pedal_count ? &part->pedals[index] : NULL;
}

const MeterMap *performer_part_local_meter_map(const PerformerPart *part) {
    return part != NULL ? part->local_meter_map : NULL;
}
