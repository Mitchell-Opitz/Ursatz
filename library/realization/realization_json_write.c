#include "realization/realization_json.h"
#include "realization/realization_json_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cJSON.h>
#include <common/hash.h>
#include <common/json.h>
#include "model/identity/entity_id.h"
#include "model/pitch/pitch_arithmetic.h"
#include "model/time/meter_map.h"
#include "model/time/note_value.h"
#include "model/time/tempo_map.h"
#include "model/time/time_signature.h"

void realization_json_set_error(RealizationJsonError *error, const char *path, const char *format, ...) {
    if (!error) {
        return;
    }
    va_list args;
    va_start(args, format);
    vsnprintf(error->message, sizeof(error->message), format, args);
    va_end(args);
    snprintf(error->path, sizeof(error->path), "%s", path);
}

/* ------------------------------------------------------------------------------------------------ writing */

/* The first failure is kept; once one is, every later builder still runs but its items are dropped, so no step needs its own cleanup. */
typedef struct {
    RealizationJsonError *error;
    bool failed;
} Writer;

static void write_fail(Writer *w, const char *message) {
    if (!w->failed) {
        w->failed = true;
        realization_json_set_error(w->error, "", "%s", message);
    }
}

static void put(Writer *w, cJSON *object, const char *key, cJSON *item) {
    if (!item) {
        write_fail(w, "out of memory");
        return;
    }
    if (!object || !cJSON_AddItemToObject(object, key, item)) {
        cJSON_Delete(item);
        write_fail(w, "out of memory");
    }
}

static void push(Writer *w, cJSON *array, cJSON *item) {
    if (!item) {
        write_fail(w, "out of memory");
        return;
    }
    if (!array || !cJSON_AddItemToArray(array, item)) {
        cJSON_Delete(item);
        write_fail(w, "out of memory");
    }
}

static cJSON *number_item(long long value) {
    return cJSON_CreateNumber((double)value);
}

static cJSON *text_item(const char *text) {
    return text ? cJSON_CreateString(text) : cJSON_CreateNull();
}

static cJSON *rational_item(Rational value) {
    return common_json_rational_object(value.num, value.den);
}

static cJSON *spelling_item(Writer *w, bool spelled, PitchLetter letter, int accidental) {
    if (!spelled) {
        return cJSON_CreateNull();
    }
    char name[8];
    if (!pitch_spelling_name(letter, accidental, name, sizeof(name))) {
        write_fail(w, "a note has a spelling that has no name");
        return NULL;
    }
    return cJSON_CreateString(name);
}

static cJSON *derivation_item(Writer *w, const NoteDerivation *derivation) {
    if (!note_derivation_present(derivation)) {
        return cJSON_CreateNull();
    }
    cJSON *object = cJSON_CreateObject();
    cJSON *sources = cJSON_CreateArray();
    for (size_t i = 0; i < derivation->source_count; i++) {
        push(w, sources, cJSON_CreateString(derivation->sources[i]));
    }
    put(w, object, "sources", sources);
    put(w, object, "operation", cJSON_CreateString(derivation->operation));
    put(w, object, "run", cJSON_CreateString(derivation->run));
    return object;
}

/* The groups of a time signature when they are not the ones its top and bottom numbers give. */
static bool groups_are_default(const TimeSignature *signature) {
    NoteValue *unit = NULL;
    TimeSignature *standard = NULL;
    int exponent = time_signature_bottom_unit_exponent(signature);
    bool same = false;
    if (note_value_create(exponent, &unit) == NOTE_VALUE_OK &&
        time_signature_create(time_signature_top_number(signature), unit, &standard) == TIME_SIGNATURE_OK) {
        same = time_signature_equals(signature, standard);
    }
    time_signature_destroy(standard);
    note_value_destroy(unit);
    return same;
}

/* The regions of a meter map as {"position", "time_signature", "groups"?}; a defaulted region (the 4/4 a map makes before a first change) is not written. */
static cJSON *meter_item(Writer *w, const MeterMap *map) {
    cJSON *array = cJSON_CreateArray();
    for (size_t i = 0; map && i < meter_map_region_count(map); i++) {
        const MeterMapRegion *region = meter_map_region(map, i);
        if (meter_map_region_is_defaulted(region)) {
            continue;
        }
        const TimeSignature *signature = meter_map_region_time_signature(region);
        char text[24];
        snprintf(text, sizeof(text), "%u/%u", time_signature_top_number(signature), time_signature_bottom_number(signature));
        cJSON *object = cJSON_CreateObject();
        put(w, object, "position", rational_item(meter_map_region_start(region).value));
        put(w, object, "time_signature", cJSON_CreateString(text));
        if (!groups_are_default(signature)) {
            cJSON *groups = cJSON_CreateArray();
            for (size_t g = 0; g < time_signature_group_count(signature); g++) {
                push(w, groups, number_item(time_signature_group_at(signature, g)));
            }
            put(w, object, "groups", groups);
        }
        push(w, array, object);
    }
    return array;
}

/* The tempo map as {"position", "quarters_per_minute"}: a rational, 60,000,000 over the microseconds of a quarter note. */
static cJSON *tempo_item(Writer *w, const TempoMap *map) {
    cJSON *array = cJSON_CreateArray();
    for (size_t i = 0; map && i < tempo_map_region_count(map); i++) {
        const TempoMapRegion *region = tempo_map_region(map, i);
        if (tempo_map_region_is_defaulted(region)) {
            continue;
        }
        Rational quarters;
        if (rational_make(MICROSECONDS_PER_MINUTE, (long long)tempo_map_region_microseconds_per_quarter(region), &quarters) != RATIONAL_OK) {
            write_fail(w, "a tempo has no quarter notes per minute");
            continue;
        }
        cJSON *object = cJSON_CreateObject();
        put(w, object, "position", rational_item(tempo_map_region_start(region).value));
        put(w, object, "quarters_per_minute", rational_item(quarters));
        push(w, array, object);
    }
    return array;
}

static cJSON *region_item(Writer *w, const Region *region) {
    if (region->kind != REGION_TONAL_MODAL || !region->material) {
        write_fail(w, "a home region that is not tonal cannot be written");
        return NULL;
    }
    cJSON *object = cJSON_CreateObject();
    put(w, object, "kind", cJSON_CreateString("tonal"));
    put(w, object, "center", number_item(region->center));
    put(w, object, "material", cJSON_CreateString(region->material->id));
    return object;
}

static cJSON *relation_item(Writer *w, const LineRelation *relation) {
    cJSON *object = cJSON_CreateObject();
    put(w, object, "kind", cJSON_CreateString(RELATION_NAMES[relation->kind]));
    put(w, object, "target", cJSON_CreateString(relation->target));
    put(w, object, "interval", number_item(relation->interval));
    put(w, object, "delay", rational_item(relation->delay.value));
    put(w, object, "start", relation->has_span ? rational_item(relation->span.start.value) : cJSON_CreateNull());
    put(w, object, "end", relation->has_span ? rational_item(relation->span.end.value) : cJSON_CreateNull());
    return object;
}

static cJSON *line_item(Writer *w, const Line *line) {
    cJSON *object = cJSON_CreateObject();
    put(w, object, "id", cJSON_CreateString(line_id(line)));
    put(w, object, "role", cJSON_CreateString(line_role_name(line_role(line))));
    put(w, object, "name", text_item(line_name(line)[0] ? line_name(line) : NULL));
    cJSON *relations = cJSON_CreateArray();
    for (size_t i = 0; i < line_relation_count(line); i++) {
        push(w, relations, relation_item(w, line_relation_at(line, i)));
    }
    put(w, object, "relations", relations);
    cJSON *notes = cJSON_CreateArray();
    for (size_t i = 0; i < line_note_count(line); i++) {
        const LineNote *note = line_note_at(line, i);
        cJSON *item = cJSON_CreateObject();
        put(w, item, "id", cJSON_CreateString(note->id));
        put(w, item, "onset", rational_item(note->onset.value));
        put(w, item, "duration", rational_item(note->duration.value));
        put(w, item, "midi", number_item(note->midi));
        put(w, item, "spelling", spelling_item(w, note->spelled, note->letter, note->accidental));
        put(w, item, "derivation", derivation_item(w, &note->derivation));
        push(w, notes, item);
    }
    put(w, object, "notes", notes);
    return object;
}

static cJSON *assignment_item(Writer *w, const ScoringAssignment *assignment) {
    cJSON *object = cJSON_CreateObject();
    put(w, object, "id", cJSON_CreateString(assignment->id));
    put(w, object, "line", cJSON_CreateString(assignment->line));
    put(w, object, "part", cJSON_CreateString(assignment->part));
    put(w, object, "whole", cJSON_CreateBool(assignment->whole));
    put(w, object, "start", assignment->whole ? cJSON_CreateNull() : rational_item(assignment->span.start.value));
    put(w, object, "end", assignment->whole ? cJSON_CreateNull() : rational_item(assignment->span.end.value));
    put(w, object, "staff", number_item(assignment->staff));
    put(w, object, "voice", number_item(assignment->voice));
    cJSON *transform = cJSON_CreateObject();
    put(w, transform, "kind", cJSON_CreateString(TRANSFORM_NAMES[assignment->transform.kind]));
    put(w, transform, "octaves", number_item(assignment->transform.octaves));
    put(w, transform, "semitones", number_item(assignment->transform.semitones));
    put(w, transform, "steps", number_item(assignment->transform.steps));
    put(w, object, "transform", transform);
    put(w, object, "divisi", cJSON_CreateBool(assignment->divisi));
    return object;
}

static cJSON *part_item(Writer *w, const PerformerPart *part) {
    cJSON *object = cJSON_CreateObject();
    put(w, object, "id", cJSON_CreateString(performer_part_id(part)));
    put(w, object, "instrument", cJSON_CreateString(performer_part_instrument(part)));
    put(w, object, "name", cJSON_CreateString(performer_part_name(part)));
    put(w, object, "number", number_item(performer_part_number(part)));
    put(w, object, "players", number_item(performer_part_players(part)));
    put(w, object, "staves", number_item(performer_part_staff_count(part)));
    put(w, object, "program", number_item(performer_part_midi_program(part)));
    put(w, object, "local_meter", meter_item(w, performer_part_local_meter_map(part)));
    cJSON *pedals = cJSON_CreateArray();
    for (size_t i = 0; i < performer_part_pedal_count(part); i++) {
        const PedalMarking *pedal = performer_part_pedal_at(part, i);
        cJSON *item = cJSON_CreateObject();
        put(w, item, "kind", cJSON_CreateString("sustain"));
        put(w, item, "start", rational_item(pedal->span.start.value));
        put(w, item, "end", rational_item(pedal->span.end.value));
        push(w, pedals, item);
    }
    put(w, object, "pedals", pedals);
    cJSON *notes = cJSON_CreateArray();
    for (size_t i = 0; i < performer_part_note_count(part); i++) {
        const PerformerNote *note = performer_part_note_at(part, i);
        cJSON *item = cJSON_CreateObject();
        put(w, item, "id", cJSON_CreateString(note->id));
        put(w, item, "onset", rational_item(note->onset.value));
        put(w, item, "duration", rational_item(note->duration.value));
        put(w, item, "midi", number_item(note->midi));
        put(w, item, "spelling", spelling_item(w, note->spelled, note->letter, note->accidental));
        put(w, item, "staff", number_item(note->staff));
        put(w, item, "voice", number_item(note->voice));
        put(w, item, "derivation", derivation_item(w, &note->derivation));
        put(w, item, "edited", cJSON_CreateBool(note->edited));
        put(w, item, "detached", cJSON_CreateBool(note->detached));
        put(w, item, "review", cJSON_CreateBool(note->review));
        push(w, notes, item);
    }
    put(w, object, "notes", notes);
    return object;
}

static cJSON *movement_item(Writer *w, const Movement *movement) {
    cJSON *object = cJSON_CreateObject();
    put(w, object, "id", cJSON_CreateString(movement_id(movement)));
    put(w, object, "title", text_item(movement_title(movement)));
    put(w, object, "meter", meter_item(w, movement_meter_map(movement)));
    put(w, object, "tempo", tempo_item(w, movement_tempo_map(movement)));
    Region home;
    put(w, object, "home", movement_home(movement, &home) ? region_item(w, &home) : cJSON_CreateNull());
    put(w, object, "ensemble", text_item(movement_ensemble(movement)));
    cJSON *keys = cJSON_CreateArray();
    for (size_t i = 0; i < movement_key_signature_count(movement); i++) {
        const MovementKeySignature *key = movement_key_signature_at(movement, i);
        cJSON *item = cJSON_CreateObject();
        put(w, item, "position", rational_item(key->position.value));
        put(w, item, "sharps_flats", number_item(key->sharps_flats));
        put(w, item, "mode", cJSON_CreateString(KEY_MODE_NAMES[key->mode]));
        push(w, keys, item);
    }
    put(w, object, "key_signatures", keys);
    cJSON *markers = cJSON_CreateArray();
    for (size_t i = 0; i < movement_marker_count(movement); i++) {
        const MovementMarker *marker = movement_marker_at(movement, i);
        cJSON *item = cJSON_CreateObject();
        put(w, item, "position", rational_item(marker->position.value));
        put(w, item, "text", cJSON_CreateString(marker->text));
        push(w, markers, item);
    }
    put(w, object, "markers", markers);
    cJSON *played = cJSON_CreateArray();
    for (size_t i = 0; i < movement_played_span_count(movement); i++) {
        const TimeSpan *span = movement_played_span_at(movement, i);
        cJSON *item = cJSON_CreateObject();
        put(w, item, "start", rational_item(span->start.value));
        put(w, item, "end", rational_item(span->end.value));
        push(w, played, item);
    }
    put(w, object, "played", played);
    cJSON *parts = cJSON_CreateArray();
    for (size_t i = 0; i < movement_compound_part_count(movement); i++) {
        const MovementPart *part = movement_compound_part_at(movement, i);
        cJSON *item = cJSON_CreateObject();
        put(w, item, "id", cJSON_CreateString(part->id));
        put(w, item, "name", cJSON_CreateString(part->name));
        put(w, item, "start", rational_item(part->span.start.value));
        put(w, item, "end", rational_item(part->span.end.value));
        put(w, item, "home", part->has_home ? region_item(w, &part->home) : cJSON_CreateNull());
        push(w, parts, item);
    }
    put(w, object, "compound_parts", parts);
    cJSON *lines = cJSON_CreateArray();
    for (size_t i = 0; i < movement_line_count(movement); i++) {
        push(w, lines, line_item(w, movement_line_at(movement, i)));
    }
    put(w, object, "lines", lines);
    cJSON *assignments = cJSON_CreateArray();
    for (size_t i = 0; i < movement_assignment_count(movement); i++) {
        push(w, assignments, assignment_item(w, movement_assignment_at(movement, i)));
    }
    put(w, object, "assignments", assignments);
    cJSON *performers = cJSON_CreateArray();
    for (size_t i = 0; i < movement_part_count(movement); i++) {
        push(w, performers, part_item(w, movement_part_at(movement, i)));
    }
    put(w, object, "parts", performers);
    return object;
}

static cJSON *document_item(Writer *w, const Work *work) {
    cJSON *document = cJSON_CreateObject();
    put(w, document, "format", cJSON_CreateString(FORMAT_NAME));
    put(w, document, "format_version", number_item(URSATZ_REALIZATION_FORMAT_VERSION));
    cJSON *object = cJSON_CreateObject();
    put(w, object, "id", cJSON_CreateString(work_id(work)));
    put(w, object, "title", text_item(work_title(work)));
    put(w, object, "composer", text_item(work_composer(work)));
    put(w, object, "ensemble", text_item(work_ensemble(work)));
    cJSON *material = cJSON_CreateArray();
    for (size_t i = 0; i < work_material_count(work); i++) {
        push(w, material, cJSON_CreateString(work_material_at(work, i)));
    }
    put(w, object, "material", material);
    cJSON *movements = cJSON_CreateArray();
    for (size_t i = 0; i < work_movement_count(work); i++) {
        push(w, movements, movement_item(w, work_movement_at(work, i)));
    }
    put(w, object, "movements", movements);
    put(w, document, "work", object);
    return document;
}

static char *write_work(const Work *work, bool compact, RealizationJsonError *error) {
    if (!work) {
        realization_json_set_error(error, "", "no work");
        return NULL;
    }
    Writer writer = {error, false};
    cJSON *document = document_item(&writer, work);
    char *text = NULL;
    if (!writer.failed) {
        char *printed = compact ? cJSON_PrintUnformatted(document) : cJSON_Print(document);
        if (printed) {
            text = compact ? printed : common_json_indent(printed);
            if (!compact) {
                cJSON_free(printed);
            }
        }
        if (!text) {
            write_fail(&writer, "out of memory");
        }
    }
    cJSON_Delete(document);
    return text;
}

char *realization_work_to_json(const Work *work, RealizationJsonError *error) {
    return write_work(work, false, error);
}

char *realization_work_to_compact_json(const Work *work, RealizationJsonError *error) {
    char *text = write_work(work, true, error);
    if (!text) {
        return NULL;
    }
    /* cJSON allocates with malloc by default: the caller frees it with free. */
    return text;
}

bool realization_digest(const Work *work, char out[17]) {
    char *text = realization_work_to_compact_json(work, NULL);
    if (!text || !out) {
        free(text);
        return false;
    }
    common_fnv1a64_hex(common_fnv1a64(text, strlen(text), COMMON_FNV1A64_OFFSET), out);
    free(text);
    return true;
}
