#include "realization/movement.h"

#include <stdlib.h>
#include <string.h>

#define TEXT_MAX 64

struct Movement {
    char id[REALIZATION_ID_MAX];
    char *title;
    MeterMap *meter_map;
    TempoMap *tempo_map;
    bool has_home;
    Region home;
    char ensemble[REALIZATION_ID_MAX];
    bool has_ensemble;
    MovementKeySignature *keys;
    size_t key_count, key_capacity;
    MovementMarker *markers;
    size_t marker_count, marker_capacity;
    TimeSpan *played;
    size_t played_count, played_capacity;
    MovementPart *compound;
    size_t compound_count, compound_capacity;
    Line **lines;
    size_t line_count, line_capacity;
    ScoringAssignment *assignments;
    size_t assignment_count, assignment_capacity;
    PerformerPart **parts;
    size_t part_count, part_capacity;
};

/* Makes room for one more of `size` bytes in an array. */
static bool grow(void **array, size_t count, size_t *capacity, size_t size) {
    if (count < *capacity) {
        return true;
    }
    size_t wanted = *capacity == 0 ? 8 : *capacity * 2;
    void *grown = realloc(*array, wanted * size);
    if (grown == NULL) {
        return false;
    }
    *array = grown;
    *capacity = wanted;
    return true;
}

static bool time_ok(MusicalTime t) {
    return t.value.den > 0 && t.value.num >= 0;
}

RealizationResult movement_create(const char *id, const char *title, Movement **out) {
    if (id == NULL || out == NULL) {
        return REALIZATION_ERR_NULL;
    }
    if (!realization_id_valid(id)) {
        return REALIZATION_ERR_BAD_ID;
    }
    Movement *movement = calloc(1, sizeof(*movement));
    if (movement == NULL) {
        return REALIZATION_ERR_ALLOC;
    }
    strcpy(movement->id, id);
    if (title != NULL) {
        size_t size = strlen(title) + 1;
        movement->title = malloc(size);
        if (movement->title == NULL) {
            free(movement);
            return REALIZATION_ERR_ALLOC;
        }
        memcpy(movement->title, title, size);
    }
    *out = movement;
    return REALIZATION_OK;
}

void movement_destroy(Movement *movement) {
    if (movement == NULL) {
        return;
    }
    meter_map_destroy(movement->meter_map);
    tempo_map_destroy(movement->tempo_map);
    for (size_t i = 0; i < movement->line_count; i++) {
        line_destroy(movement->lines[i]);
    }
    for (size_t i = 0; i < movement->part_count; i++) {
        performer_part_destroy(movement->parts[i]);
    }
    free(movement->title);
    free(movement->keys);
    free(movement->markers);
    free(movement->played);
    free(movement->compound);
    free(movement->lines);
    free(movement->assignments);
    free(movement->parts);
    free(movement);
}

RealizationResult movement_set_meter_map(Movement *movement, MeterMap *map) {
    if (movement == NULL) {
        return REALIZATION_ERR_NULL;
    }
    if (movement->meter_map != map) {
        meter_map_destroy(movement->meter_map);
    }
    movement->meter_map = map;
    return REALIZATION_OK;
}

RealizationResult movement_set_tempo_map(Movement *movement, TempoMap *map) {
    if (movement == NULL) {
        return REALIZATION_ERR_NULL;
    }
    if (movement->tempo_map != map) {
        tempo_map_destroy(movement->tempo_map);
    }
    movement->tempo_map = map;
    return REALIZATION_OK;
}

RealizationResult movement_set_home(Movement *movement, Region home) {
    if (movement == NULL) {
        return REALIZATION_ERR_NULL;
    }
    movement->home = home;
    movement->has_home = true;
    return REALIZATION_OK;
}

RealizationResult movement_set_ensemble(Movement *movement, const char *ensemble_id) {
    if (movement == NULL) {
        return REALIZATION_ERR_NULL;
    }
    if (ensemble_id == NULL) {
        movement->has_ensemble = false;
        movement->ensemble[0] = '\0';
        return REALIZATION_OK;
    }
    if (!realization_id_valid(ensemble_id)) {
        return REALIZATION_ERR_BAD_ID;
    }
    strcpy(movement->ensemble, ensemble_id);
    movement->has_ensemble = true;
    return REALIZATION_OK;
}

RealizationResult movement_add_key_signature(Movement *movement, const MovementKeySignature *key) {
    if (movement == NULL || key == NULL) {
        return REALIZATION_ERR_NULL;
    }
    if (!time_ok(key->position) || key->sharps_flats < -7 || key->sharps_flats > 7 || (int)key->mode < (int)MOVEMENT_KEY_MAJOR ||
        (int)key->mode > (int)MOVEMENT_KEY_UNKNOWN ||
        (movement->key_count > 0 && musical_time_cmp(key->position, movement->keys[movement->key_count - 1].position) < 0)) {
        return REALIZATION_ERR_BAD_VALUE;
    }
    if (!grow((void **)&movement->keys, movement->key_count, &movement->key_capacity, sizeof(*movement->keys))) {
        return REALIZATION_ERR_ALLOC;
    }
    movement->keys[movement->key_count++] = *key;
    return REALIZATION_OK;
}

RealizationResult movement_add_marker(Movement *movement, const MovementMarker *marker) {
    if (movement == NULL || marker == NULL) {
        return REALIZATION_ERR_NULL;
    }
    size_t length = strnlen(marker->text, TEXT_MAX);
    if (!time_ok(marker->position) || length == 0 || length > TEXT_MAX - 1) {
        return REALIZATION_ERR_BAD_VALUE;
    }
    if (!grow((void **)&movement->markers, movement->marker_count, &movement->marker_capacity, sizeof(*movement->markers))) {
        return REALIZATION_ERR_ALLOC;
    }
    movement->markers[movement->marker_count++] = *marker;
    return REALIZATION_OK;
}

RealizationResult movement_add_played_span(Movement *movement, TimeSpan span) {
    if (movement == NULL) {
        return REALIZATION_ERR_NULL;
    }
    if (!time_ok(span.start) || !time_span_holds(span)) {
        return REALIZATION_ERR_BAD_VALUE;
    }
    if (!grow((void **)&movement->played, movement->played_count, &movement->played_capacity, sizeof(*movement->played))) {
        return REALIZATION_ERR_ALLOC;
    }
    movement->played[movement->played_count++] = span;
    return REALIZATION_OK;
}

RealizationResult movement_add_compound_part(Movement *movement, const MovementPart *part) {
    if (movement == NULL || part == NULL) {
        return REALIZATION_ERR_NULL;
    }
    if (!realization_id_valid(part->id)) {
        return REALIZATION_ERR_BAD_ID;
    }
    for (size_t i = 0; i < movement->compound_count; i++) {
        if (strcmp(movement->compound[i].id, part->id) == 0) {
            return REALIZATION_ERR_DUPLICATE_ID;
        }
    }
    if (!time_ok(part->span.start) || !time_span_holds(part->span) || memchr(part->name, '\0', sizeof(part->name)) == NULL) {
        return REALIZATION_ERR_BAD_VALUE;
    }
    if (!grow((void **)&movement->compound, movement->compound_count, &movement->compound_capacity, sizeof(*movement->compound))) {
        return REALIZATION_ERR_ALLOC;
    }
    movement->compound[movement->compound_count++] = *part;
    return REALIZATION_OK;
}

RealizationResult movement_add_line(Movement *movement, Line *line) {
    if (movement == NULL || line == NULL) {
        return REALIZATION_ERR_NULL;
    }
    if (movement_line_named(movement, line_id(line)) != NULL) {
        return REALIZATION_ERR_DUPLICATE_ID;
    }
    if (!grow((void **)&movement->lines, movement->line_count, &movement->line_capacity, sizeof(*movement->lines))) {
        return REALIZATION_ERR_ALLOC;
    }
    movement->lines[movement->line_count++] = line;
    return REALIZATION_OK;
}

RealizationResult movement_add_part(Movement *movement, PerformerPart *part) {
    if (movement == NULL || part == NULL) {
        return REALIZATION_ERR_NULL;
    }
    if (movement_part_named(movement, performer_part_id(part)) != NULL) {
        return REALIZATION_ERR_DUPLICATE_ID;
    }
    if (!grow((void **)&movement->parts, movement->part_count, &movement->part_capacity, sizeof(*movement->parts))) {
        return REALIZATION_ERR_ALLOC;
    }
    movement->parts[movement->part_count++] = part;
    return REALIZATION_OK;
}

RealizationResult movement_add_assignment(Movement *movement, const ScoringAssignment *assignment) {
    if (movement == NULL || assignment == NULL) {
        return REALIZATION_ERR_NULL;
    }
    RealizationResult checked = scoring_assignment_check(assignment);
    if (checked != REALIZATION_OK) {
        return checked;
    }
    for (size_t i = 0; i < movement->assignment_count; i++) {
        if (strcmp(movement->assignments[i].id, assignment->id) == 0) {
            return REALIZATION_ERR_DUPLICATE_ID;
        }
    }
    if (!grow((void **)&movement->assignments, movement->assignment_count, &movement->assignment_capacity,
              sizeof(*movement->assignments))) {
        return REALIZATION_ERR_ALLOC;
    }
    movement->assignments[movement->assignment_count++] = *assignment;
    return REALIZATION_OK;
}

RealizationResult movement_score(Movement *movement, const char *run) {
    if (movement == NULL) {
        return REALIZATION_ERR_NULL;
    }
    return realization_score(movement->lines, movement->line_count, movement->assignments, movement->assignment_count,
                             movement->parts, movement->part_count, run);
}

const char *movement_id(const Movement *movement) {
    return movement != NULL ? movement->id : NULL;
}

const char *movement_title(const Movement *movement) {
    return movement != NULL ? movement->title : NULL;
}

const MeterMap *movement_meter_map(const Movement *movement) {
    return movement != NULL ? movement->meter_map : NULL;
}

const TempoMap *movement_tempo_map(const Movement *movement) {
    return movement != NULL ? movement->tempo_map : NULL;
}

bool movement_home(const Movement *movement, Region *out) {
    if (movement == NULL || !movement->has_home) {
        return false;
    }
    if (out != NULL) {
        *out = movement->home;
    }
    return true;
}

const char *movement_ensemble(const Movement *movement) {
    return movement != NULL && movement->has_ensemble ? movement->ensemble : NULL;
}

size_t movement_key_signature_count(const Movement *movement) {
    return movement != NULL ? movement->key_count : 0;
}

const MovementKeySignature *movement_key_signature_at(const Movement *movement, size_t index) {
    return movement != NULL && index < movement->key_count ? &movement->keys[index] : NULL;
}

size_t movement_marker_count(const Movement *movement) {
    return movement != NULL ? movement->marker_count : 0;
}

const MovementMarker *movement_marker_at(const Movement *movement, size_t index) {
    return movement != NULL && index < movement->marker_count ? &movement->markers[index] : NULL;
}

size_t movement_played_span_count(const Movement *movement) {
    return movement != NULL ? movement->played_count : 0;
}

const TimeSpan *movement_played_span_at(const Movement *movement, size_t index) {
    return movement != NULL && index < movement->played_count ? &movement->played[index] : NULL;
}

size_t movement_compound_part_count(const Movement *movement) {
    return movement != NULL ? movement->compound_count : 0;
}

const MovementPart *movement_compound_part_at(const Movement *movement, size_t index) {
    return movement != NULL && index < movement->compound_count ? &movement->compound[index] : NULL;
}

size_t movement_line_count(const Movement *movement) {
    return movement != NULL ? movement->line_count : 0;
}

Line *movement_line_at(const Movement *movement, size_t index) {
    return movement != NULL && index < movement->line_count ? movement->lines[index] : NULL;
}

Line *movement_line_named(const Movement *movement, const char *id) {
    for (size_t i = 0; movement != NULL && id != NULL && i < movement->line_count; i++) {
        if (strcmp(line_id(movement->lines[i]), id) == 0) {
            return movement->lines[i];
        }
    }
    return NULL;
}

size_t movement_assignment_count(const Movement *movement) {
    return movement != NULL ? movement->assignment_count : 0;
}

const ScoringAssignment *movement_assignment_at(const Movement *movement, size_t index) {
    return movement != NULL && index < movement->assignment_count ? &movement->assignments[index] : NULL;
}

size_t movement_part_count(const Movement *movement) {
    return movement != NULL ? movement->part_count : 0;
}

PerformerPart *movement_part_at(const Movement *movement, size_t index) {
    return movement != NULL && index < movement->part_count ? movement->parts[index] : NULL;
}

PerformerPart *movement_part_named(const Movement *movement, const char *id) {
    for (size_t i = 0; movement != NULL && id != NULL && i < movement->part_count; i++) {
        if (strcmp(performer_part_id(movement->parts[i]), id) == 0) {
            return movement->parts[i];
        }
    }
    return NULL;
}
