#include "realization/line.h"

#include <stdlib.h>
#include <string.h>

#define LINE_NAME_MAX 64

struct Line {
    char id[REALIZATION_ID_MAX];
    LineRole role;
    char name[LINE_NAME_MAX];
    LineNote *notes;
    size_t note_count;
    size_t note_capacity;
    LineRelation *relations;
    size_t relation_count;
    size_t relation_capacity;
};

static const char *const ROLE_NAMES[] = {"melody", "counter-melody", "harmony-line", "bass",    "chords",  "pad",
                                         "accompaniment", "drums", "pedal", "subject", "answer", "countersubject"};
#define ROLE_COUNT (sizeof(ROLE_NAMES) / sizeof(ROLE_NAMES[0]))

const char *line_role_name(LineRole role) {
    return (int)role >= 0 && (size_t)role < ROLE_COUNT ? ROLE_NAMES[role] : NULL;
}

bool line_role_from_name(const char *name, LineRole *out) {
    if (name == NULL || out == NULL) {
        return false;
    }
    for (size_t i = 0; i < ROLE_COUNT; i++) {
        if (strcmp(name, ROLE_NAMES[i]) == 0) {
            *out = (LineRole)i;
            return true;
        }
    }
    return false;
}

RealizationResult line_create(const char *id, LineRole role, const char *name, Line **out) {
    if (id == NULL || out == NULL) {
        return REALIZATION_ERR_NULL;
    }
    if (!realization_id_valid(id)) {
        return REALIZATION_ERR_BAD_ID;
    }
    if (line_role_name(role) == NULL || (name != NULL && strlen(name) > LINE_NAME_MAX - 1)) {
        return REALIZATION_ERR_BAD_VALUE;
    }
    Line *line = calloc(1, sizeof(*line));
    if (line == NULL) {
        return REALIZATION_ERR_ALLOC;
    }
    strcpy(line->id, id);
    line->role = role;
    if (name != NULL) {
        strcpy(line->name, name);
    }
    *out = line;
    return REALIZATION_OK;
}

void line_destroy(Line *line) {
    if (line == NULL) {
        return;
    }
    free(line->notes);
    free(line->relations);
    free(line);
}

static bool derivation_valid(const NoteDerivation *derivation) {
    if (derivation->source_count > NOTE_DERIVATION_SOURCES_MAX) {
        return false;
    }
    for (size_t i = 0; i < derivation->source_count; i++) {
        if (!realization_id_valid(derivation->sources[i])) {
            return false;
        }
    }
    return true;
}

RealizationResult line_add_note(Line *line, const LineNote *note) {
    if (line == NULL || note == NULL) {
        return REALIZATION_ERR_NULL;
    }
    if (!realization_id_valid(note->id)) {
        return REALIZATION_ERR_BAD_ID;
    }
    if (line_note_named(line, note->id) != NULL) {
        return REALIZATION_ERR_DUPLICATE_ID;
    }
    if (note->onset.value.num < 0 || note->duration.value.num <= 0 || note->onset.value.den <= 0 || note->duration.value.den <= 0 ||
        note->midi < 0 || note->midi > 127 || !derivation_valid(&note->derivation)) {
        return REALIZATION_ERR_BAD_VALUE;
    }
    if (note->spelled && ((int)note->letter < (int)PITCH_LETTER_A || (int)note->letter > (int)PITCH_LETTER_G ||
                          note->accidental < -2 || note->accidental > 2)) {
        return REALIZATION_ERR_BAD_VALUE;
    }
    if (line->note_count == line->note_capacity) {
        size_t capacity = line->note_capacity == 0 ? 16 : line->note_capacity * 2;
        LineNote *grown = realloc(line->notes, capacity * sizeof(LineNote));
        if (grown == NULL) {
            return REALIZATION_ERR_ALLOC;
        }
        line->notes = grown;
        line->note_capacity = capacity;
    }
    line->notes[line->note_count++] = *note;
    return REALIZATION_OK;
}

RealizationResult line_add_relation(Line *line, const LineRelation *relation) {
    if (line == NULL || relation == NULL) {
        return REALIZATION_ERR_NULL;
    }
    if (!realization_id_valid(relation->target)) {
        return REALIZATION_ERR_BAD_ID;
    }
    if (strcmp(relation->target, line->id) == 0 || (int)relation->kind < (int)LINE_RELATION_PARALLEL ||
        (int)relation->kind > (int)LINE_RELATION_CALL_RESPONSE || relation->delay.value.num < 0 || relation->delay.value.den <= 0 ||
        (relation->has_span && !time_span_holds(relation->span))) {
        return REALIZATION_ERR_BAD_VALUE;
    }
    if (line->relation_count == line->relation_capacity) {
        size_t capacity = line->relation_capacity == 0 ? 4 : line->relation_capacity * 2;
        LineRelation *grown = realloc(line->relations, capacity * sizeof(LineRelation));
        if (grown == NULL) {
            return REALIZATION_ERR_ALLOC;
        }
        line->relations = grown;
        line->relation_capacity = capacity;
    }
    line->relations[line->relation_count++] = *relation;
    return REALIZATION_OK;
}

const char *line_id(const Line *line) {
    return line != NULL ? line->id : NULL;
}

LineRole line_role(const Line *line) {
    return line != NULL ? line->role : LINE_ROLE_MELODY;
}

const char *line_name(const Line *line) {
    return line != NULL ? line->name : NULL;
}

size_t line_note_count(const Line *line) {
    return line != NULL ? line->note_count : 0;
}

const LineNote *line_note_at(const Line *line, size_t index) {
    return line != NULL && index < line->note_count ? &line->notes[index] : NULL;
}

const LineNote *line_note_named(const Line *line, const char *note_id) {
    if (line == NULL || note_id == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < line->note_count; i++) {
        if (strcmp(line->notes[i].id, note_id) == 0) {
            return &line->notes[i];
        }
    }
    return NULL;
}

size_t line_relation_count(const Line *line) {
    return line != NULL ? line->relation_count : 0;
}

const LineRelation *line_relation_at(const Line *line, size_t index) {
    return line != NULL && index < line->relation_count ? &line->relations[index] : NULL;
}
