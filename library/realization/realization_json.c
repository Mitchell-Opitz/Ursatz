#include "realization/realization_json.h"

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

#define FORMAT_NAME "ursatz-realization"
#define MICROSECONDS_PER_MINUTE 60000000LL

static const char *const RELATION_NAMES[] = {"parallel", "imitation", "answers", "call-response"};
static const char *const TRANSFORM_NAMES[] = {"none", "octave", "interval"};
static const char *const KEY_MODE_NAMES[] = {"major", "minor", "unknown"};
#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

/* ------------------------------------------------------------------------------------------------ writing */

/* The first failure is kept; once one is, every later builder still runs but its items are dropped, so no step needs its own cleanup. */
typedef struct {
    RealizationJsonError *error;
    bool failed;
} Writer;

static void set_error(RealizationJsonError *error, const char *path, const char *format, ...) {
    if (!error) {
        return;
    }
    va_list args;
    va_start(args, format);
    vsnprintf(error->message, sizeof(error->message), format, args);
    va_end(args);
    snprintf(error->path, sizeof(error->path), "%s", path);
}

static void write_fail(Writer *w, const char *message) {
    if (!w->failed) {
        w->failed = true;
        set_error(w->error, "", "%s", message);
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

/* cJSON prints with tabs; the project's JSON uses a 2-space indent and "key": value. */
static char *indent_with_spaces(const char *printed) {
    size_t tabs = 0;
    size_t length = strlen(printed);
    for (size_t i = 0; i < length; i++) {
        tabs += printed[i] == '\t';
    }
    char *out = malloc(length + tabs + 2);
    if (!out) {
        return NULL;
    }
    char *cursor = out;
    for (size_t i = 0; i < length; i++) {
        if (printed[i] == '\t' && i > 0 && printed[i - 1] == ':') {
            *cursor++ = ' ';
        } else if (printed[i] == '\t') {
            *cursor++ = ' ';
            *cursor++ = ' ';
        } else {
            *cursor++ = printed[i];
        }
    }
    *cursor++ = '\n';
    *cursor = '\0';
    return out;
}

static char *write_work(const Work *work, bool compact, RealizationJsonError *error) {
    if (!work) {
        set_error(error, "", "no work");
        return NULL;
    }
    Writer writer = {error, false};
    cJSON *document = document_item(&writer, work);
    char *text = NULL;
    if (!writer.failed) {
        char *printed = compact ? cJSON_PrintUnformatted(document) : cJSON_Print(document);
        if (printed) {
            text = compact ? printed : indent_with_spaces(printed);
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

/* ------------------------------------------------------------------------------------------------ reading */

typedef struct {
    RealizationJsonError *error;
    bool failed;
    RealizationResult result;
    const KnowledgeSnapshot *knowledge;
} Reader;

static bool refuse(Reader *r, RealizationResult result, const char *path, const char *format, ...) {
    if (!r->failed) {
        r->failed = true;
        r->result = result;
        if (r->error) {
            va_list args;
            va_start(args, format);
            vsnprintf(r->error->message, sizeof(r->error->message), format, args);
            va_end(args);
            snprintf(r->error->path, sizeof(r->error->path), "%s", path);
        }
    }
    return false;
}

static bool bad(Reader *r, const char *path, const char *message) {
    return refuse(r, REALIZATION_ERR_BAD_VALUE, path, "%s", message);
}

/* What the group's own check said, as text at the member's path. */
static bool group_refused(Reader *r, RealizationResult result, const char *path, const char *what) {
    const char *reason = "the value is refused";
    switch (result) {
    case REALIZATION_ERR_BAD_ID:
        reason = "not a valid id";
        break;
    case REALIZATION_ERR_DUPLICATE_ID:
        reason = "the id is used twice";
        break;
    case REALIZATION_ERR_BAD_VALUE:
        reason = "a value is out of range";
        break;
    case REALIZATION_ERR_UNKNOWN_REFERENCE:
        reason = "it names something that is not there";
        break;
    case REALIZATION_ERR_NOT_BUILT:
        reason = "it is not built";
        break;
    case REALIZATION_ERR_ALLOC:
        return refuse(r, result, path, "out of memory");
    default:
        break;
    }
    return refuse(r, result, path, "%s: %s", what, reason);
}

static void child_member(char *out, const char *parent, const char *key) {
    if (parent[0] == '\0') {
        snprintf(out, REALIZATION_JSON_PATH_MAX, "%s", key);
    } else {
        snprintf(out, REALIZATION_JSON_PATH_MAX, "%s.%s", parent, key);
    }
}

static void child_index(char *out, const char *parent, size_t index) {
    snprintf(out, REALIZATION_JSON_PATH_MAX, "%s[%zu]", parent, index);
}

static const cJSON *member(Reader *r, const cJSON *object, const char *path, const char *key) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    if (!item) {
        char at[REALIZATION_JSON_PATH_MAX];
        child_member(at, path, key);
        bad(r, at, "missing required member");
    }
    return item;
}

static bool read_object(Reader *r, const cJSON *item, const char *path) {
    return cJSON_IsObject(item) || bad(r, path, "expected an object");
}

static const cJSON *read_array(Reader *r, const cJSON *object, const char *path, const char *key, char *at) {
    const cJSON *item = member(r, object, path, key);
    if (!item) {
        return NULL;
    }
    child_member(at, path, key);
    if (!cJSON_IsArray(item)) {
        bad(r, at, "expected an array");
        return NULL;
    }
    return item;
}

static bool read_long(Reader *r, const cJSON *item, const char *path, long long min, long long max, long long *out) {
    if (!cJSON_IsNumber(item)) {
        return bad(r, path, "expected a number");
    }
    double value = item->valuedouble;
    if (value < (double)min || value > (double)max || (double)(long long)value != value) {
        return refuse(r, REALIZATION_ERR_BAD_VALUE, path, "expected an integer from %lld to %lld", min, max);
    }
    *out = (long long)value;
    return true;
}

static bool read_int_member(Reader *r, const cJSON *object, const char *path, const char *key, long long min, long long max, int *out) {
    const cJSON *item = member(r, object, path, key);
    char at[REALIZATION_JSON_PATH_MAX];
    long long value;
    child_member(at, path, key);
    if (!item || !read_long(r, item, at, min, max, &value)) {
        return false;
    }
    *out = (int)value;
    return true;
}

static bool read_bool_member(Reader *r, const cJSON *object, const char *path, const char *key, bool *out) {
    const cJSON *item = member(r, object, path, key);
    if (!item) {
        return false;
    }
    if (!cJSON_IsBool(item)) {
        char at[REALIZATION_JSON_PATH_MAX];
        child_member(at, path, key);
        return bad(r, at, "expected true or false");
    }
    *out = cJSON_IsTrue(item);
    return true;
}

/* A text member; *out is NULL for a JSON null when allow_null. */
static bool read_text_member(Reader *r, const cJSON *object, const char *path, const char *key, bool allow_null, const char **out) {
    const cJSON *item = member(r, object, path, key);
    if (!item) {
        return false;
    }
    char at[REALIZATION_JSON_PATH_MAX];
    child_member(at, path, key);
    if (allow_null && cJSON_IsNull(item)) {
        *out = NULL;
        return true;
    }
    if (!cJSON_IsString(item) || !item->valuestring) {
        return bad(r, at, allow_null ? "expected text or null" : "expected text");
    }
    *out = item->valuestring;
    return true;
}

static bool read_rational(Reader *r, const cJSON *item, const char *path, Rational *out) {
    long long num;
    long long den;
    /* A time in range is read by the common codec; any other value falls through to the checks that say what is wrong. */
    if (common_json_rational_read(item, COMMON_RATIONAL_OBJECT, &num, &den) && num >= -1000000000LL && num <= 1000000000LL &&
        den <= 1000000000LL) {
        return rational_make(num, den, out) == RATIONAL_OK || bad(r, path, "not a time");
    }
    if (!cJSON_IsObject(item)) {
        return bad(r, path, "expected a time {\"num\", \"den\"}");
    }
    char at[REALIZATION_JSON_PATH_MAX];
    const cJSON *n = member(r, item, path, "num");
    child_member(at, path, "num");
    if (!n || !read_long(r, n, at, -1000000000LL, 1000000000LL, &num)) {
        return false;
    }
    const cJSON *d = member(r, item, path, "den");
    child_member(at, path, "den");
    if (!d || !read_long(r, d, at, 1, 1000000000LL, &den)) {
        return false;
    }
    return rational_make(num, den, out) == RATIONAL_OK || bad(r, path, "not a time");
}

static bool read_time_member(Reader *r, const cJSON *object, const char *path, const char *key, bool allow_null, bool *present, MusicalTime *out) {
    const cJSON *item = member(r, object, path, key);
    if (!item) {
        return false;
    }
    char at[REALIZATION_JSON_PATH_MAX];
    child_member(at, path, key);
    if (allow_null && cJSON_IsNull(item)) {
        *present = false;
        return true;
    }
    Rational value;
    if (!read_rational(r, item, at, &value)) {
        return false;
    }
    if (present) {
        *present = true;
    }
    if (value.num < 0) {
        return bad(r, at, "a time is not below 0");
    }
    *out = musical_time_of(value);
    return true;
}

static bool read_duration_member(Reader *r, const cJSON *object, const char *path, const char *key, Duration *out) {
    const cJSON *item = member(r, object, path, key);
    char at[REALIZATION_JSON_PATH_MAX];
    Rational value;
    child_member(at, path, key);
    if (!item || !read_rational(r, item, at, &value)) {
        return false;
    }
    return duration_of(value, out) == DURATION_OK || bad(r, at, "a duration is not below 0");
}

static bool read_name(Reader *r, const cJSON *object, const char *path, const char *key, const char *const *names, size_t count, int *out) {
    const char *text;
    if (!read_text_member(r, object, path, key, false, &text)) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        if (strcmp(text, names[i]) == 0) {
            *out = (int)i;
            return true;
        }
    }
    char at[REALIZATION_JSON_PATH_MAX];
    child_member(at, path, key);
    return refuse(r, REALIZATION_ERR_BAD_VALUE, at, "unknown name '%.40s'", text);
}

static bool read_spelling_member(Reader *r, const cJSON *object, const char *path, bool *spelled, PitchLetter *letter, int *accidental) {
    const char *text;
    if (!read_text_member(r, object, path, "spelling", true, &text)) {
        return false;
    }
    *spelled = text != NULL;
    if (text && !pitch_spelling_parse_name(text, letter, accidental, NULL)) {
        char at[REALIZATION_JSON_PATH_MAX];
        child_member(at, path, "spelling");
        return bad(r, at, "expected a note name like \"F#\" or \"Bb\"");
    }
    return true;
}

static bool read_derivation_member(Reader *r, const cJSON *object, const char *path, NoteDerivation *out) {
    memset(out, 0, sizeof(*out));
    const cJSON *item = member(r, object, path, "derivation");
    if (!item) {
        return false;
    }
    char at[REALIZATION_JSON_PATH_MAX];
    child_member(at, path, "derivation");
    if (cJSON_IsNull(item)) {
        return true;
    }
    if (!read_object(r, item, at)) {
        return false;
    }
    char list_at[REALIZATION_JSON_PATH_MAX];
    const cJSON *sources = read_array(r, item, at, "sources", list_at);
    if (!sources) {
        return false;
    }
    size_t index = 0;
    const cJSON *source;
    cJSON_ArrayForEach(source, sources) {
        char source_at[REALIZATION_JSON_PATH_MAX];
        child_index(source_at, list_at, index++);
        if (!cJSON_IsString(source) || !source->valuestring) {
            return bad(r, source_at, "expected a note id");
        }
        RealizationResult added = note_derivation_add_source(out, source->valuestring);
        if (added != REALIZATION_OK) {
            return group_refused(r, added, source_at, "source");
        }
    }
    const char *operation;
    const char *run;
    if (!read_text_member(r, item, at, "operation", false, &operation) || !read_text_member(r, item, at, "run", false, &run)) {
        return false;
    }
    if (operation[0] != '\0' && note_derivation_set_operation(out, operation) != REALIZATION_OK) {
        child_member(list_at, at, "operation");
        return bad(r, list_at, "expected lowercase words joined by hyphens, at most 31 characters");
    }
    if (run[0] != '\0' && note_derivation_set_run(out, run) != REALIZATION_OK) {
        child_member(list_at, at, "run");
        return bad(r, list_at, "expected printable text of at most 63 characters");
    }
    return true;
}

static bool copy_id(Reader *r, const char *text, const char *path, char out[REALIZATION_ID_MAX]) {
    if (!realization_id_valid(text)) {
        return refuse(r, REALIZATION_ERR_BAD_ID, path, "not a valid id: '%.40s'", text);
    }
    snprintf(out, REALIZATION_ID_MAX, "%s", text);
    return true;
}

static bool read_id_member(Reader *r, const cJSON *object, const char *path, const char *key, char out[REALIZATION_ID_MAX]) {
    const char *text;
    char at[REALIZATION_JSON_PATH_MAX];
    if (!read_text_member(r, object, path, key, false, &text)) {
        return false;
    }
    child_member(at, path, key);
    return copy_id(r, text, at, out);
}

/* "N/M" with M a power of two from 1 to 16. */
static bool parse_signature(const char *text, unsigned *top, unsigned *bottom) {
    char tail;
    return sscanf(text, "%u/%u%c", top, bottom, &tail) == 2 && *top >= 1 && *top <= TIME_SIGNATURE_MAX_TOP_NUMBER &&
           (*bottom == 1 || *bottom == 2 || *bottom == 4 || *bottom == 8 || *bottom == 16);
}

static bool read_meter_map(Reader *r, const cJSON *object, const char *path, const char *key, const char *id_text, MeterMap **out) {
    *out = NULL;
    char list_at[REALIZATION_JSON_PATH_MAX];
    const cJSON *list = read_array(r, object, path, key, list_at);
    size_t count = list ? (size_t)cJSON_GetArraySize(list) : 0;
    if (!list) {
        return false;
    }
    if (count == 0) {
        return true;
    }
    TimeSignature **signatures = calloc(count, sizeof(TimeSignature *));
    MeterMapChangePoint *points = calloc(count, sizeof(MeterMapChangePoint));
    bool ok = signatures && points;
    if (!ok) {
        refuse(r, REALIZATION_ERR_ALLOC, list_at, "out of memory");
    }
    size_t index = 0;
    const cJSON *item;
    cJSON_ArrayForEach(item, list) {
        char at[REALIZATION_JSON_PATH_MAX];
        child_index(at, list_at, index);
        const char *text;
        unsigned top;
        unsigned bottom;
        if (!ok || !read_object(r, item, at) || !read_time_member(r, item, at, "position", false, NULL, &points[index].start) ||
            !read_text_member(r, item, at, "time_signature", false, &text)) {
            ok = false;
            break;
        }
        char sig_at[REALIZATION_JSON_PATH_MAX];
        child_member(sig_at, at, "time_signature");
        if (!parse_signature(text, &top, &bottom)) {
            ok = bad(r, sig_at, "expected a time signature like \"3/4\"");
            break;
        }
        int exponent = 0;
        while ((1u << exponent) < bottom) {
            exponent++;
        }
        NoteValue *unit = NULL;
        TimeSignatureResult made;
        if (note_value_create(exponent, &unit) != NOTE_VALUE_OK) {
            ok = refuse(r, REALIZATION_ERR_ALLOC, sig_at, "out of memory");
            break;
        }
        const cJSON *groups = cJSON_GetObjectItemCaseSensitive(item, "groups");
        if (groups && !cJSON_IsNull(groups)) {
            unsigned values[TIME_SIGNATURE_MAX_GROUPS];
            size_t group_count = 0;
            const cJSON *group;
            char groups_at[REALIZATION_JSON_PATH_MAX];
            child_member(groups_at, at, "groups");
            if (!cJSON_IsArray(groups) || cJSON_GetArraySize(groups) < 1 || (size_t)cJSON_GetArraySize(groups) > TIME_SIGNATURE_MAX_GROUPS) {
                note_value_destroy(unit);
                ok = bad(r, groups_at, "expected 1 to 16 groups");
                break;
            }
            cJSON_ArrayForEach(group, groups) {
                long long value;
                char group_at[REALIZATION_JSON_PATH_MAX];
                child_index(group_at, groups_at, group_count);
                if (!read_long(r, group, group_at, 1, TIME_SIGNATURE_MAX_GROUP_UNITS, &value)) {
                    ok = false;
                    break;
                }
                values[group_count++] = (unsigned)value;
            }
            if (!ok) {
                note_value_destroy(unit);
                break;
            }
            made = time_signature_create_grouped(values, group_count, unit, &signatures[index]);
        } else {
            made = time_signature_create(top, unit, &signatures[index]);
        }
        note_value_destroy(unit);
        if (made != TIME_SIGNATURE_OK) {
            ok = bad(r, sig_at, "not a time signature");
            break;
        }
        points[index].time_signature = signatures[index];
        index++;
    }
    MeterMap *map = NULL;
    EntityID *id = NULL;
    if (ok) {
        if (entity_id_create(id_text, &id) != ENTITY_ID_OK) {
            ok = refuse(r, REALIZATION_ERR_ALLOC, list_at, "out of memory");
        } else {
            MeterMapResult made = meter_map_create(id, points, count, &map);
            if (made != METER_MAP_OK) {
                entity_id_destroy(id);
                ok = made == METER_MAP_ERR_ALLOC ? refuse(r, REALIZATION_ERR_ALLOC, list_at, "out of memory")
                                                 : bad(r, list_at, "the meter changes must start from 0 and ascend");
            }
        }
    }
    for (size_t i = 0; signatures && i < count; i++) {
        time_signature_destroy(signatures[i]);
    }
    free(signatures);
    free(points);
    *out = ok ? map : NULL;
    return ok;
}

static bool read_tempo_map(Reader *r, const cJSON *object, const char *path, const char *id_text, TempoMap **out) {
    *out = NULL;
    char list_at[REALIZATION_JSON_PATH_MAX];
    const cJSON *list = read_array(r, object, path, "tempo", list_at);
    if (!list) {
        return false;
    }
    size_t count = (size_t)cJSON_GetArraySize(list);
    if (count == 0) {
        return true;
    }
    TempoMapChangePoint *points = calloc(count, sizeof(TempoMapChangePoint));
    if (!points) {
        return refuse(r, REALIZATION_ERR_ALLOC, list_at, "out of memory");
    }
    bool ok = true;
    size_t index = 0;
    const cJSON *item;
    cJSON_ArrayForEach(item, list) {
        char at[REALIZATION_JSON_PATH_MAX];
        char quarters_at[REALIZATION_JSON_PATH_MAX];
        Rational quarters;
        child_index(at, list_at, index);
        child_member(quarters_at, at, "quarters_per_minute");
        const cJSON *tempo;
        if (!read_object(r, item, at) || !read_time_member(r, item, at, "position", false, NULL, &points[index].start) ||
            !(tempo = member(r, item, at, "quarters_per_minute")) || !read_rational(r, tempo, quarters_at, &quarters)) {
            ok = false;
            break;
        }
        /* microseconds per quarter = 60,000,000 / quarters per minute, which has to be a whole number of at least 1 */
        long long scaled = quarters.num > 0 ? MICROSECONDS_PER_MINUTE * quarters.den : 0;
        if (quarters.num <= 0 || scaled % quarters.num != 0 || scaled / quarters.num < 1) {
            ok = bad(r, quarters_at, "the tempo is not a whole number of microseconds per quarter note");
            break;
        }
        points[index].microseconds_per_quarter = (unsigned long)(scaled / quarters.num);
        points[index].kind = TEMPO_MAP_CHANGE_ABSOLUTE;
        index++;
    }
    if (ok) {
        EntityID *id = NULL;
        TempoMap *map = NULL;
        if (entity_id_create(id_text, &id) != ENTITY_ID_OK) {
            ok = refuse(r, REALIZATION_ERR_ALLOC, list_at, "out of memory");
        } else if (tempo_map_create(id, points, count, &map) != TEMPO_MAP_OK) {
            entity_id_destroy(id);
            ok = bad(r, list_at, "the tempo changes must start from 0 and ascend");
        } else {
            *out = map;
        }
    }
    free(points);
    return ok;
}

static bool read_region(Reader *r, const cJSON *item, const char *path, Region *out) {
    if (!read_object(r, item, path)) {
        return false;
    }
    const char *kind;
    const char *material;
    int center;
    char at[REALIZATION_JSON_PATH_MAX];
    if (!read_text_member(r, item, path, "kind", false, &kind)) {
        return false;
    }
    if (strcmp(kind, "tonal") != 0) {
        child_member(at, path, "kind");
        return refuse(r, REALIZATION_ERR_BAD_VALUE, at, "region kind '%.40s' is not built", kind);
    }
    if (!read_int_member(r, item, path, "center", 0, 11, &center) || !read_text_member(r, item, path, "material", false, &material)) {
        return false;
    }
    const PitchMaterial *found = knowledge_snapshot_material(r->knowledge, material);
    if (!found) {
        child_member(at, path, "material");
        return refuse(r, REALIZATION_ERR_UNKNOWN_REFERENCE, at, "no material has the id '%.60s'", material);
    }
    *out = region_tonal(center, found);
    return true;
}

static bool read_span_members(Reader *r, const cJSON *object, const char *path, TimeSpan *out) {
    MusicalTime start;
    MusicalTime end;
    if (!read_time_member(r, object, path, "start", false, NULL, &start) || !read_time_member(r, object, path, "end", false, NULL, &end)) {
        return false;
    }
    return time_span_make(start, end, out) == TIME_SPAN_OK || bad(r, path, "the span ends before it starts");
}

static bool read_line(Reader *r, const cJSON *item, const char *path, Movement *movement) {
    if (!read_object(r, item, path)) {
        return false;
    }
    char id[REALIZATION_ID_MAX];
    const char *name;
    int role;
    char at[REALIZATION_JSON_PATH_MAX];
    if (!read_id_member(r, item, path, "id", id) || !read_text_member(r, item, path, "name", true, &name)) {
        return false;
    }
    const cJSON *role_item = member(r, item, path, "role");
    child_member(at, path, "role");
    if (!role_item || !cJSON_IsString(role_item) || !line_role_from_name(role_item->valuestring, (LineRole *)&role)) {
        return role_item ? bad(r, at, "unknown line role") : false;
    }
    Line *line = NULL;
    RealizationResult made = line_create(id, (LineRole)role, name, &line);
    if (made != REALIZATION_OK) {
        child_member(at, path, name ? "name" : "role");
        return group_refused(r, made, at, "line");
    }
    bool ok = true;
    char list_at[REALIZATION_JSON_PATH_MAX];
    const cJSON *relations = read_array(r, item, path, "relations", list_at);
    size_t index = 0;
    const cJSON *relation;
    ok = relations != NULL;
    cJSON_ArrayForEach(relation, relations) {
        char relation_at[REALIZATION_JSON_PATH_MAX];
        LineRelation value;
        int kind;
        const char *target;
        child_index(relation_at, list_at, index++);
        memset(&value, 0, sizeof(value));
        MusicalTime start;
        MusicalTime end;
        bool has_start = false;
        bool has_end = false;
        if (!read_object(r, relation, relation_at) || !read_name(r, relation, relation_at, "kind", RELATION_NAMES, COUNT_OF(RELATION_NAMES), &kind) ||
            !read_text_member(r, relation, relation_at, "target", false, &target) ||
            !read_int_member(r, relation, relation_at, "interval", -1000, 1000, &value.interval) ||
            !read_duration_member(r, relation, relation_at, "delay", &value.delay) ||
            !read_time_member(r, relation, relation_at, "start", true, &has_start, &start) ||
            !read_time_member(r, relation, relation_at, "end", true, &has_end, &end)) {
            ok = false;
            break;
        }
        value.kind = (LineRelationKind)kind;
        child_member(at, relation_at, "target");
        if (!copy_id(r, target, at, value.target)) {
            ok = false;
            break;
        }
        if (has_start != has_end) {
            ok = bad(r, relation_at, "a relation names both ends of its span or neither");
            break;
        }
        value.has_span = has_start;
        if (has_start && time_span_make(start, end, &value.span) != TIME_SPAN_OK) {
            ok = bad(r, relation_at, "the span ends before it starts");
            break;
        }
        RealizationResult added = line_add_relation(line, &value);
        if (added != REALIZATION_OK) {
            ok = group_refused(r, added, relation_at, "relation");
            break;
        }
    }
    const cJSON *notes = ok ? read_array(r, item, path, "notes", list_at) : NULL;
    ok = ok && notes != NULL;
    index = 0;
    const cJSON *note;
    cJSON_ArrayForEach(note, notes) {
        char note_at[REALIZATION_JSON_PATH_MAX];
        LineNote value;
        child_index(note_at, list_at, index++);
        memset(&value, 0, sizeof(value));
        if (!ok || !read_object(r, note, note_at) || !read_id_member(r, note, note_at, "id", value.id) ||
            !read_time_member(r, note, note_at, "onset", false, NULL, &value.onset) ||
            !read_duration_member(r, note, note_at, "duration", &value.duration) || !read_int_member(r, note, note_at, "midi", 0, 127, &value.midi) ||
            !read_spelling_member(r, note, note_at, &value.spelled, &value.letter, &value.accidental) ||
            !read_derivation_member(r, note, note_at, &value.derivation)) {
            ok = false;
            break;
        }
        RealizationResult added = line_add_note(line, &value);
        if (added != REALIZATION_OK) {
            child_member(at, note_at, added == REALIZATION_ERR_BAD_ID || added == REALIZATION_ERR_DUPLICATE_ID ? "id" : "onset");
            ok = group_refused(r, added, at, "note");
            break;
        }
    }
    if (ok) {
        RealizationResult added = movement_add_line(movement, line);
        if (added != REALIZATION_OK) {
            child_member(at, path, "id");
            ok = group_refused(r, added, at, "line");
        }
    }
    if (!ok) {
        line_destroy(line);
    }
    return ok;
}

static bool read_part(Reader *r, const cJSON *item, const char *path, Movement *movement) {
    if (!read_object(r, item, path)) {
        return false;
    }
    char id[REALIZATION_ID_MAX];
    char instrument[REALIZATION_ID_MAX];
    const char *name;
    int number;
    int players;
    int staves;
    int program;
    char at[REALIZATION_JSON_PATH_MAX];
    if (!read_id_member(r, item, path, "id", id) || !read_id_member(r, item, path, "instrument", instrument) ||
        !read_text_member(r, item, path, "name", false, &name) || !read_int_member(r, item, path, "number", 1, 1000, &number) ||
        !read_int_member(r, item, path, "players", 1, 1000, &players) || !read_int_member(r, item, path, "staves", 1, 4, &staves) ||
        !read_int_member(r, item, path, "program", 0, 127, &program)) {
        return false;
    }
    PerformerPart *part = NULL;
    RealizationResult made = performer_part_create(id, instrument, name, number, players, staves, program, &part);
    if (made != REALIZATION_OK) {
        child_member(at, path, "name");
        return group_refused(r, made, at, "part");
    }
    bool ok = true;
    char list_at[REALIZATION_JSON_PATH_MAX];
    MeterMap *local = NULL;
    char map_id[REALIZATION_ID_MAX + 16];
    snprintf(map_id, sizeof(map_id), "%s-meter", id);
    ok = read_meter_map(r, item, path, "local_meter", map_id, &local);
    if (ok && local) {
        RealizationResult set = performer_part_set_local_meter_map(part, local);
        if (set != REALIZATION_OK) {
            meter_map_destroy(local);
            child_member(at, path, "local_meter");
            ok = group_refused(r, set, at, "local meter");
        }
    }
    const cJSON *pedals = ok ? read_array(r, item, path, "pedals", list_at) : NULL;
    ok = ok && pedals != NULL;
    size_t index = 0;
    const cJSON *pedal;
    cJSON_ArrayForEach(pedal, pedals) {
        char pedal_at[REALIZATION_JSON_PATH_MAX];
        PedalMarking marking;
        const char *kind;
        child_index(pedal_at, list_at, index++);
        if (!ok || !read_object(r, pedal, pedal_at) || !read_text_member(r, pedal, pedal_at, "kind", false, &kind) ||
            !read_span_members(r, pedal, pedal_at, &marking.span)) {
            ok = false;
            break;
        }
        if (strcmp(kind, "sustain") != 0) {
            child_member(at, pedal_at, "kind");
            ok = bad(r, at, "expected \"sustain\"");
            break;
        }
        marking.kind = PEDAL_SUSTAIN;
        RealizationResult added = performer_part_add_pedal(part, &marking);
        if (added != REALIZATION_OK) {
            ok = group_refused(r, added, pedal_at, "pedal");
            break;
        }
    }
    const cJSON *notes = ok ? read_array(r, item, path, "notes", list_at) : NULL;
    ok = ok && notes != NULL;
    index = 0;
    const cJSON *note;
    cJSON_ArrayForEach(note, notes) {
        char note_at[REALIZATION_JSON_PATH_MAX];
        PerformerNote value;
        child_index(note_at, list_at, index++);
        memset(&value, 0, sizeof(value));
        if (!ok || !read_object(r, note, note_at) || !read_id_member(r, note, note_at, "id", value.id) ||
            !read_time_member(r, note, note_at, "onset", false, NULL, &value.onset) ||
            !read_duration_member(r, note, note_at, "duration", &value.duration) || !read_int_member(r, note, note_at, "midi", 0, 127, &value.midi) ||
            !read_spelling_member(r, note, note_at, &value.spelled, &value.letter, &value.accidental) ||
            !read_int_member(r, note, note_at, "staff", 1, 4, &value.staff) || !read_int_member(r, note, note_at, "voice", 1, 1000, &value.voice) ||
            !read_derivation_member(r, note, note_at, &value.derivation) || !read_bool_member(r, note, note_at, "edited", &value.edited) ||
            !read_bool_member(r, note, note_at, "detached", &value.detached) || !read_bool_member(r, note, note_at, "review", &value.review)) {
            ok = false;
            break;
        }
        RealizationResult added = performer_part_add_note(part, &value);
        if (added != REALIZATION_OK) {
            child_member(at, note_at, added == REALIZATION_ERR_BAD_ID || added == REALIZATION_ERR_DUPLICATE_ID ? "id" : "staff");
            ok = group_refused(r, added, at, "note");
            break;
        }
    }
    if (ok) {
        RealizationResult added = movement_add_part(movement, part);
        if (added != REALIZATION_OK) {
            child_member(at, path, "id");
            ok = group_refused(r, added, at, "part");
        }
    }
    if (!ok) {
        performer_part_destroy(part);
    }
    return ok;
}

static bool read_assignment(Reader *r, const cJSON *item, const char *path, Movement *movement) {
    if (!read_object(r, item, path)) {
        return false;
    }
    ScoringAssignment value;
    memset(&value, 0, sizeof(value));
    int kind;
    if (!read_id_member(r, item, path, "id", value.id) || !read_id_member(r, item, path, "line", value.line) ||
        !read_id_member(r, item, path, "part", value.part) || !read_bool_member(r, item, path, "whole", &value.whole)) {
        return false;
    }
    if (!value.whole && !read_span_members(r, item, path, &value.span)) {
        return false;
    }
    if (!read_int_member(r, item, path, "staff", 1, 4, &value.staff) || !read_int_member(r, item, path, "voice", 1, 1000, &value.voice) ||
        !read_bool_member(r, item, path, "divisi", &value.divisi)) {
        return false;
    }
    char at[REALIZATION_JSON_PATH_MAX];
    const cJSON *transform = member(r, item, path, "transform");
    child_member(at, path, "transform");
    if (!transform || !read_object(r, transform, at) || !read_name(r, transform, at, "kind", TRANSFORM_NAMES, COUNT_OF(TRANSFORM_NAMES), &kind) ||
        !read_int_member(r, transform, at, "octaves", -4, 4, &value.transform.octaves) ||
        !read_int_member(r, transform, at, "semitones", -127, 127, &value.transform.semitones) ||
        !read_int_member(r, transform, at, "steps", -127, 127, &value.transform.steps)) {
        return false;
    }
    value.transform.kind = (ScoringTransformKind)kind;
    child_member(at, path, "id");
    RealizationResult added = movement_add_assignment(movement, &value);
    if (added != REALIZATION_OK) {
        return group_refused(r, added, at, "assignment");
    }
    /* The line and the part are named by id: they are read by now. */
    if (!movement_line_named(movement, value.line)) {
        child_member(at, path, "line");
        return refuse(r, REALIZATION_ERR_UNKNOWN_REFERENCE, at, "the movement has no line '%.40s'", value.line);
    }
    if (!movement_part_named(movement, value.part)) {
        child_member(at, path, "part");
        return refuse(r, REALIZATION_ERR_UNKNOWN_REFERENCE, at, "the movement has no part '%.40s'", value.part);
    }
    return true;
}

static bool read_movement_body(Reader *r, const cJSON *item, const char *path, Movement *movement, const char *id) {
    char list_at[REALIZATION_JSON_PATH_MAX];
    char at[REALIZATION_JSON_PATH_MAX];
    char map_id[REALIZATION_ID_MAX + 16];
    MeterMap *meter = NULL;
    TempoMap *tempo = NULL;
    snprintf(map_id, sizeof(map_id), "%s-meter", id);
    if (!read_meter_map(r, item, path, "meter", map_id, &meter)) {
        return false;
    }
    if (meter && movement_set_meter_map(movement, meter) != REALIZATION_OK) {
        meter_map_destroy(meter);
        return refuse(r, REALIZATION_ERR_ALLOC, path, "out of memory");
    }
    snprintf(map_id, sizeof(map_id), "%s-tempo", id);
    if (!read_tempo_map(r, item, path, map_id, &tempo)) {
        return false;
    }
    if (tempo && movement_set_tempo_map(movement, tempo) != REALIZATION_OK) {
        tempo_map_destroy(tempo);
        return refuse(r, REALIZATION_ERR_ALLOC, path, "out of memory");
    }
    const cJSON *home = member(r, item, path, "home");
    child_member(at, path, "home");
    if (!home) {
        return false;
    }
    if (!cJSON_IsNull(home)) {
        Region region;
        if (!read_region(r, home, at, &region)) {
            return false;
        }
        RealizationResult set = movement_set_home(movement, region);
        if (set != REALIZATION_OK) {
            return group_refused(r, set, at, "home");
        }
    }
    const char *ensemble;
    if (!read_text_member(r, item, path, "ensemble", true, &ensemble)) {
        return false;
    }
    if (ensemble) {
        RealizationResult set = movement_set_ensemble(movement, ensemble);
        child_member(at, path, "ensemble");
        if (set != REALIZATION_OK) {
            return group_refused(r, set, at, "ensemble");
        }
    }
    const cJSON *list = read_array(r, item, path, "key_signatures", list_at);
    size_t index = 0;
    const cJSON *entry;
    if (!list) {
        return false;
    }
    cJSON_ArrayForEach(entry, list) {
        MovementKeySignature key;
        int mode;
        char entry_at[REALIZATION_JSON_PATH_MAX];
        child_index(entry_at, list_at, index++);
        memset(&key, 0, sizeof(key));
        if (!read_object(r, entry, entry_at) || !read_time_member(r, entry, entry_at, "position", false, NULL, &key.position) ||
            !read_int_member(r, entry, entry_at, "sharps_flats", -7, 7, &key.sharps_flats) ||
            !read_name(r, entry, entry_at, "mode", KEY_MODE_NAMES, COUNT_OF(KEY_MODE_NAMES), &mode)) {
            return false;
        }
        key.mode = (MovementKeyMode)mode;
        RealizationResult added = movement_add_key_signature(movement, &key);
        if (added != REALIZATION_OK) {
            return group_refused(r, added, entry_at, "key signature");
        }
    }
    list = read_array(r, item, path, "markers", list_at);
    if (!list) {
        return false;
    }
    index = 0;
    cJSON_ArrayForEach(entry, list) {
        MovementMarker marker;
        const char *text;
        char entry_at[REALIZATION_JSON_PATH_MAX];
        child_index(entry_at, list_at, index++);
        memset(&marker, 0, sizeof(marker));
        if (!read_object(r, entry, entry_at) || !read_time_member(r, entry, entry_at, "position", false, NULL, &marker.position) ||
            !read_text_member(r, entry, entry_at, "text", false, &text)) {
            return false;
        }
        if (strlen(text) >= sizeof(marker.text)) {
            child_member(at, entry_at, "text");
            return bad(r, at, "a marker is at most 63 characters");
        }
        snprintf(marker.text, sizeof(marker.text), "%s", text);
        RealizationResult added = movement_add_marker(movement, &marker);
        if (added != REALIZATION_OK) {
            return group_refused(r, added, entry_at, "marker");
        }
    }
    list = read_array(r, item, path, "played", list_at);
    if (!list) {
        return false;
    }
    index = 0;
    cJSON_ArrayForEach(entry, list) {
        TimeSpan span;
        char entry_at[REALIZATION_JSON_PATH_MAX];
        child_index(entry_at, list_at, index++);
        if (!read_object(r, entry, entry_at) || !read_span_members(r, entry, entry_at, &span)) {
            return false;
        }
        RealizationResult added = movement_add_played_span(movement, span);
        if (added != REALIZATION_OK) {
            return group_refused(r, added, entry_at, "played span");
        }
    }
    list = read_array(r, item, path, "compound_parts", list_at);
    if (!list) {
        return false;
    }
    index = 0;
    cJSON_ArrayForEach(entry, list) {
        MovementPart part;
        const char *name;
        char entry_at[REALIZATION_JSON_PATH_MAX];
        child_index(entry_at, list_at, index++);
        memset(&part, 0, sizeof(part));
        if (!read_object(r, entry, entry_at) || !read_id_member(r, entry, entry_at, "id", part.id) ||
            !read_text_member(r, entry, entry_at, "name", false, &name) || !read_span_members(r, entry, entry_at, &part.span)) {
            return false;
        }
        if (strlen(name) >= sizeof(part.name)) {
            child_member(at, entry_at, "name");
            return bad(r, at, "a name is at most 63 characters");
        }
        snprintf(part.name, sizeof(part.name), "%s", name);
        const cJSON *part_home = member(r, entry, entry_at, "home");
        if (!part_home) {
            return false;
        }
        if (!cJSON_IsNull(part_home)) {
            child_member(at, entry_at, "home");
            part.has_home = true;
            if (!read_region(r, part_home, at, &part.home)) {
                return false;
            }
        }
        RealizationResult added = movement_add_compound_part(movement, &part);
        if (added != REALIZATION_OK) {
            return group_refused(r, added, entry_at, "compound part");
        }
    }
    list = read_array(r, item, path, "lines", list_at);
    if (!list) {
        return false;
    }
    index = 0;
    cJSON_ArrayForEach(entry, list) {
        char entry_at[REALIZATION_JSON_PATH_MAX];
        child_index(entry_at, list_at, index++);
        if (!read_line(r, entry, entry_at, movement)) {
            return false;
        }
    }
    list = read_array(r, item, path, "parts", list_at);
    if (!list) {
        return false;
    }
    index = 0;
    cJSON_ArrayForEach(entry, list) {
        char entry_at[REALIZATION_JSON_PATH_MAX];
        child_index(entry_at, list_at, index++);
        if (!read_part(r, entry, entry_at, movement)) {
            return false;
        }
    }
    list = read_array(r, item, path, "assignments", list_at);
    if (!list) {
        return false;
    }
    index = 0;
    cJSON_ArrayForEach(entry, list) {
        char entry_at[REALIZATION_JSON_PATH_MAX];
        child_index(entry_at, list_at, index++);
        if (!read_assignment(r, entry, entry_at, movement)) {
            return false;
        }
    }
    return true;
}

static bool read_work(Reader *r, const cJSON *document, Work **out) {
    const cJSON *item = member(r, document, "", "work");
    if (!item || !read_object(r, item, "work")) {
        return false;
    }
    char id[REALIZATION_ID_MAX];
    const char *title;
    const char *composer;
    const char *ensemble;
    char at[REALIZATION_JSON_PATH_MAX];
    if (!read_id_member(r, item, "work", "id", id) || !read_text_member(r, item, "work", "title", true, &title) ||
        !read_text_member(r, item, "work", "composer", true, &composer) || !read_text_member(r, item, "work", "ensemble", true, &ensemble)) {
        return false;
    }
    Work *work = NULL;
    RealizationResult made = work_create(id, title, composer, &work);
    if (made != REALIZATION_OK) {
        return group_refused(r, made, "work", "work");
    }
    bool ok = true;
    if (ensemble) {
        RealizationResult set = work_set_ensemble(work, ensemble);
        child_member(at, "work", "ensemble");
        ok = set == REALIZATION_OK || group_refused(r, set, at, "ensemble");
    }
    char list_at[REALIZATION_JSON_PATH_MAX];
    const cJSON *list = ok ? read_array(r, item, "work", "material", list_at) : NULL;
    ok = ok && list != NULL;
    size_t index = 0;
    const cJSON *entry;
    cJSON_ArrayForEach(entry, list) {
        char entry_at[REALIZATION_JSON_PATH_MAX];
        child_index(entry_at, list_at, index++);
        if (!cJSON_IsString(entry) || !entry->valuestring) {
            ok = bad(r, entry_at, "expected a material reference like \"form:binary\"");
            break;
        }
        RealizationResult added = work_add_material(work, entry->valuestring);
        if (added != REALIZATION_OK) {
            ok = group_refused(r, added, entry_at, "material");
            break;
        }
    }
    list = ok ? read_array(r, item, "work", "movements", list_at) : NULL;
    ok = ok && list != NULL;
    index = 0;
    cJSON_ArrayForEach(entry, list) {
        char entry_at[REALIZATION_JSON_PATH_MAX];
        char movement_id[REALIZATION_ID_MAX];
        const char *movement_title;
        child_index(entry_at, list_at, index++);
        if (!ok || !read_object(r, entry, entry_at) || !read_id_member(r, entry, entry_at, "id", movement_id) ||
            !read_text_member(r, entry, entry_at, "title", true, &movement_title)) {
            ok = false;
            break;
        }
        Movement *movement = NULL;
        RealizationResult started = movement_create(movement_id, movement_title, &movement);
        if (started != REALIZATION_OK) {
            ok = group_refused(r, started, entry_at, "movement");
            break;
        }
        if (!read_movement_body(r, entry, entry_at, movement, movement_id)) {
            movement_destroy(movement);
            ok = false;
            break;
        }
        RealizationResult added = work_add_movement(work, movement);
        if (added != REALIZATION_OK) {
            movement_destroy(movement);
            child_member(at, entry_at, "id");
            ok = group_refused(r, added, at, "movement");
            break;
        }
    }
    if (!ok) {
        work_destroy(work);
        return false;
    }
    *out = work;
    return true;
}

RealizationResult realization_work_from_json(const KnowledgeSnapshot *knowledge, const char *text, Work **out, RealizationJsonError *error) {
    if (out) {
        *out = NULL;
    }
    if (!knowledge || !text || !out) {
        set_error(error, "", "a knowledge snapshot, a text and an out are required");
        return REALIZATION_ERR_NULL;
    }
    Reader reader = {error, false, REALIZATION_OK, knowledge};
    const char *end = NULL;
    cJSON *document = cJSON_ParseWithLengthOpts(text, strlen(text) + 1, &end, true);
    if (!document) {
        set_error(error, "", "not valid JSON (byte %ld)", end ? (long)(end - text) : 0L);
        return REALIZATION_ERR_BAD_VALUE;
    }
    if (!cJSON_IsObject(document)) {
        bad(&reader, "", "expected a JSON object");
    } else {
        const cJSON *format = cJSON_GetObjectItemCaseSensitive(document, "format");
        const cJSON *version = cJSON_GetObjectItemCaseSensitive(document, "format_version");
        if (!cJSON_IsString(format) || strcmp(format->valuestring, FORMAT_NAME) != 0) {
            bad(&reader, "format", "expected \"" FORMAT_NAME "\"");
        } else if (!cJSON_IsNumber(version) || version->valuedouble != (double)URSATZ_REALIZATION_FORMAT_VERSION) {
            bad(&reader, "format_version", "expected the format version 1");
        } else {
            read_work(&reader, document, out);
        }
    }
    cJSON_Delete(document);
    return reader.failed ? reader.result : REALIZATION_OK;
}
