#include "plan/plan_json.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cJSON.h>
#include <common/json.h>
#include <model/pitch/pitch_arithmetic.h>
#include <theory/harmony/harmonic_function.h>
#include <theory/tonality/tonal_key.h>
#include "plan/plan_constraint.h"
#include "plan/plan_lanes.h"
#include "plan/plan_state.h"
#include "plan/section_json.h"
#include "plan/plan_constraint_kinds.h"
#include "plan/plan_internal.h"
#include "plan/plan_json_internal.h"
#include "plan/plan_common.h"

#define PLAN_FORMAT_NAME "ursatz-plan"

/* The name formats 2 to 5 carried: they are read, not written. */
#define PLAN_FORMAT_NAME_LEGACY "ursatz-composer-plan"

const char *const plan_json_NODE_KINDS[PLAN_JSON_NODE_KIND_COUNT] = {"form", "section", "phrase", "subphrase", "idea", "motive"};

const char *const plan_json_STATE_NAMES[PLAN_JSON_STATE_COUNT] = {"constrained", "given", "realized"};

/* The "mode" of a key in plan formats 1 and 2, which are only read. */
const char *const plan_json_MODE_NAMES[PLAN_JSON_MODE_COUNT] = {"major", "minor"};

/* A key span's anchor, in PlanKeyAnchor order. */
const char *const plan_json_KEY_ANCHOR_NAMES[PLAN_JSON_KEY_ANCHOR_COUNT] = {"home", "previous"};

const char *const plan_json_ARCHETYPE_NAMES[PLAN_JSON_ARCHETYPE_COUNT] = {"sentence", "period", "hybrid"};

/* The parts of a game loop, in PiecePart order (piece.h). */
const char *const plan_json_PIECE_PART_NAMES[PLAN_JSON_PIECE_PART_COUNT] = {"intro", "body", "turnaround", "ending"};

/* The accompaniment patterns by name, in PlanAccompanimentPattern order (a setting return and an accompaniment constraint). */
const char *const JSON_ACCOMPANIMENT_NAMES[JSON_ACCOMPANIMENT_COUNT] = {"any", "block", "arpeggiated", "alberti", "oom-pah", "motive", "entry"};

/* ------------------------------------------------------------------ writing */

bool plan_json_add(cJSON *object, const char *key, cJSON *item) {
    if (!item) {
        return false;
    }
    if (!cJSON_AddItemToObject(object, key, item)) {
        cJSON_Delete(item);
        return false;
    }
    return true;
}

bool plan_json_append(cJSON *array, cJSON *item) {
    if (!item) {
        return false;
    }
    if (!cJSON_AddItemToArray(array, item)) {
        cJSON_Delete(item);
        return false;
    }
    return true;
}

cJSON *json_number(long value) {
    return cJSON_CreateNumber((double)value);
}

cJSON *plan_json_span_json(PlanSpan span) {
    Rational end;
    if (plan_span_end(span, &end) != PLAN_OK) {
        return NULL;
    }
    cJSON *object = cJSON_CreateObject();
    if (object && !(plan_json_add(object, "start", common_json_rational_object(span.start.num, span.start.den)) && plan_json_add(object, "end", common_json_rational_object(end.num, end.den)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* A motive ref's relation step as its text (motive_relation_step_text); a ref with no step has no member. */
#define STEP_TEXT_MAX 160

/* A key's "tonic", spelled as the library spells it (F#, Bb), and its "material", the id of the key's pitch material. */
bool json_key_members(cJSON *object, const SpelledKey *key) {
    char tonic[8];
    return pitch_spelling_name(key->tonic_letter, key->tonic_accidental, tonic, sizeof(tonic)) &&
           plan_json_add(object, "tonic", cJSON_CreateString(tonic)) &&
           plan_json_add(object, "material", cJSON_CreateString(plan_key_material_id(key)));
}

/* The checks a form runs, by name, in FormCheck bit order. */
const char *const plan_json_FORM_CHECK_NAMES[PLAN_JSON_FORM_CHECK_COUNT] = {"sections", "returns", "keys", "cadences", "seam"};

static cJSON *meter_json(const Plan *plan) {
    cJSON *spans = plan_json_lane_array_json(plan, 1u << PLAN_LANE_METER);
    if (!spans) {
        return NULL;
    }
    if (cJSON_GetArraySize(spans) == 0) {
        cJSON_Delete(spans);
        return cJSON_CreateNull();
    }
    cJSON *object = cJSON_CreateObject();
    if (!object || !plan_json_add(object, "spans", spans)) {
        if (!object) {
            cJSON_Delete(spans);
        }
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* Who wrote the file (the stamp the caller gives) and, for a plan loaded from an analysis, the Analyzer. */
static cJSON *metadata_json(const Plan *plan, const PlanProducer *producer) {
    cJSON *object = cJSON_CreateObject();
    const char *analyzer = plan_analyzer_version(plan);
    if (object && !(plan_json_add(object, "producer", cJSON_CreateString(producer->name)) &&
                    plan_json_add(object, "producer_version", cJSON_CreateString(producer->version)) &&
                    plan_json_add(object, "producer_key", cJSON_CreateString(producer->producer_key)) &&
                    plan_json_add(object, "composition_key", cJSON_CreateString(producer->composition_key)) &&
                    plan_json_add(object, "knowledge_hash", cJSON_CreateString(producer->knowledge_hash)) &&
                    (analyzer[0] == '\0' || plan_json_add(object, "analyzer_version", cJSON_CreateString(analyzer))))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The reading and the realization the plan was made from (R-635). */
static cJSON *derived_from_json(const PlanDerivation *derivation) {
    cJSON *object = cJSON_CreateObject();
    if (object && !(plan_json_add(object, "reading", cJSON_CreateString(derivation->reading)) &&
                    plan_json_add(object, "realization", cJSON_CreateString(derivation->realization)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *document_json(const Plan *plan, const PlanProducer *producer) {
    cJSON *document = cJSON_CreateObject();
    if (!document) {
        return NULL;
    }
    const PlanDerivation *derivation = plan_derived_from(plan);
    bool ok = plan_json_add(document, "format", cJSON_CreateString(PLAN_FORMAT_NAME)) &&
              plan_json_add(document, "format_version", json_number(URSATZ_PLAN_FORMAT_VERSION)) &&
              plan_json_add(document, "next_note_id", json_number((double)plan_next_note_id(plan))) &&
              plan_json_add(document, "metadata", metadata_json(plan, producer)) &&
              (!derivation || plan_json_add(document, "derived_from", derived_from_json(derivation))) &&
              plan_json_add(document, "meter", meter_json(plan)) &&
              plan_json_add(document, "regions", plan_json_lane_array_json(plan, 1u << PLAN_LANE_REGION)) &&
              plan_json_add(document, "chords", plan_json_lane_array_json(plan, 1u << PLAN_LANE_HARMONY)) &&
              plan_json_add(document, "form", plan_node_count(plan) > 0 ? plan_json_node_json(plan, plan_root(plan)) : cJSON_CreateNull()) &&
              plan_json_add(document, "melody", plan_json_lane_array_json(plan, 1u << PLAN_LANE_MELODY)) &&
              plan_json_add(document, "texture", plan_json_lane_array_json(plan, (1u << PLAN_LANE_BASS) | (1u << PLAN_LANE_INNER_VOICE)));
    if (!ok) {
        cJSON_Delete(document);
        return NULL;
    }
    return document;
}

void json_set_error(PlanJsonError *error, long offset, const char *path, const char *format, ...) {
    if (!error) {
        return;
    }
    va_list args;
    va_start(args, format);
    vsnprintf(error->message, sizeof(error->message), format, args);
    va_end(args);
    snprintf(error->path, sizeof(error->path), "%s", path);
    error->offset = offset;
}

char *plan_to_json(const Plan *plan, const PlanProducer *producer, PlanJsonError *error) {
    if (!plan) {
        json_set_error(error, -1, "", "plan is NULL");
        return NULL;
    }
    if (!producer) {
        json_set_error(error, -1, "", "producer is NULL");
        return NULL;
    }
    cJSON *document = document_json(plan, producer);
    char *printed = document ? cJSON_Print(document) : NULL;
    cJSON_Delete(document);
    char *text = printed ? common_json_indent(printed) : NULL;
    cJSON_free(printed);
    if (!text) {
        json_set_error(error, -1, "", "out of memory");
    }
    return text;
}

/* ------------------------------------------------------------------ reading */

bool json_fail(JsonReader *reader, PlanResult result, const char *path, const char *format, ...) {
    if (!reader->json.failed) {
        char message[256];
        va_list args;
        va_start(args, format);
        vsnprintf(message, sizeof(message), format, args);
        va_end(args);
        common_json_refuse(&reader->json, (int)result, path, "%s", message);
        if (reader->error) {
            reader->error->offset = -1;
        }
    }
    return false;
}

bool json_bad_format(JsonReader *reader, const char *path, const char *message) {
    return json_fail(reader, PLAN_ERR_FORMAT, path, "%s", message);
}

void json_child_path(char *out, const char *parent, const char *key) {
    common_json_child_path(out, PLAN_JSON_PATH_MAX, parent, key);
}

void json_index_path(char *out, const char *parent, size_t index) {
    common_json_index_path(out, PLAN_JSON_PATH_MAX, parent, index);
}

/* The plan_json_member of object named key, or NULL after reporting it missing (when required) or of the wrong type. */
const cJSON *plan_json_member(JsonReader *reader, const cJSON *object, const char *path, const char *key, bool required) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    if (!item && required) {
        char at[PLAN_JSON_PATH_MAX];
        json_child_path(at, path, key);
        json_bad_format(reader, at, reader->json.missing_text);
    }
    return item;
}

bool json_read_long(JsonReader *reader, const cJSON *item, const char *path, long min, long max, long *out) {
    long long value;
    if (!common_json_read_whole(&reader->json, item, path, min, max, &value)) {
        if (reader->error) {
            reader->error->offset = -1;
        }
        return false;
    }
    *out = (long)value;
    return true;
}

bool json_read_int_member(JsonReader *reader, const cJSON *object, const char *path, const char *key, long min,
                            long max, long *out) {
    const cJSON *item = plan_json_member(reader, object, path, key, true);
    if (!item) {
        return false;
    }
    char at[PLAN_JSON_PATH_MAX];
    json_child_path(at, path, key);
    return json_read_long(reader, item, at, min, max, out);
}

bool json_read_enum(JsonReader *reader, const cJSON *item, const char *path, const char *const *names, size_t count,
                      int *out) {
    if (!cJSON_IsString(item)) {
        return json_bad_format(reader, path, "expected a string");
    }
    for (size_t i = 0; i < count; i++) {
        if (strcmp(item->valuestring, names[i]) == 0) {
            *out = (int)i;
            return true;
        }
    }
    return json_fail(reader, PLAN_ERR_FORMAT, path, "unknown value '%.40s'", item->valuestring);
}

bool json_read_enum_member(JsonReader *reader, const cJSON *object, const char *path, const char *key,
                             const char *const *names, size_t count, int *out) {
    const cJSON *item = plan_json_member(reader, object, path, key, true);
    if (!item) {
        return false;
    }
    char at[PLAN_JSON_PATH_MAX];
    json_child_path(at, path, key);
    return json_read_enum(reader, item, at, names, count, out);
}

static bool json_read_rational(JsonReader *reader, const cJSON *item, const char *path, Rational *out) {
    long long value_num;
    long long value_den;
    /* A time in range is read by the common codec; any other value falls through to the checks that say what is wrong. */
    if (common_json_rational_read(item, COMMON_RATIONAL_OBJECT, &value_num, &value_den) && value_num >= -TIME_LIMIT &&
        value_num <= TIME_LIMIT && value_den <= TIME_LIMIT) {
        return rational_make(value_num, value_den, out) == RATIONAL_OK;
    }
    if (!cJSON_IsObject(item)) {
        return json_bad_format(reader, path, "expected an object with num and den");
    }
    long num;
    long den;
    if (!json_read_int_member(reader, item, path, "num", -TIME_LIMIT, TIME_LIMIT, &num) ||
        !json_read_int_member(reader, item, path, "den", 1, TIME_LIMIT, &den)) {
        return false;
    }
    return rational_make(num, (long long)den, out) == RATIONAL_OK;
}

bool plan_json_read_rational_member(JsonReader *reader, const cJSON *object, const char *path, const char *key,
                                 Rational *out) {
    const cJSON *item = plan_json_member(reader, object, path, key, true);
    if (!item) {
        return false;
    }
    char at[PLAN_JSON_PATH_MAX];
    json_child_path(at, path, key);
    return json_read_rational(reader, item, at, out);
}

/* Reads {"start", "end"} into a start and a length. */
bool json_read_span(JsonReader *reader, const cJSON *object, const char *path, PlanSpan *out) {
    const cJSON *item = plan_json_member(reader, object, path, "span", true);
    if (!item) {
        return false;
    }
    char at[PLAN_JSON_PATH_MAX];
    json_child_path(at, path, "span");
    if (!cJSON_IsObject(item)) {
        return json_bad_format(reader, at, "expected an object with start and end");
    }
    Rational end;
    if (!plan_json_read_rational_member(reader, item, at, "start", &out->start) ||
        !plan_json_read_rational_member(reader, item, at, "end", &end)) {
        return false;
    }
    Rational negative_start = {-out->start.num, out->start.den};
    if (rational_add(end, negative_start, &out->length) != RATIONAL_OK || out->length.num <= 0) {
        return json_bad_format(reader, at, "end must be after start");
    }
    return true;
}

/* An optional boolean member; absent is false. */
bool json_read_optional_bool(JsonReader *reader, const cJSON *object, const char *path, const char *key, bool *out) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    *out = false;
    if (!item) {
        return true;
    }
    if (!cJSON_IsBool(item)) {
        char at[PLAN_JSON_PATH_MAX];
        json_child_path(at, path, key);
        return json_bad_format(reader, at, "expected true or false");
    }
    *out = cJSON_IsTrue(item);
    return true;
}

static bool apply_states(JsonReader *reader, Plan *plan, const cJSON *document) {
    /* Nodes: walk the form tree; the ids were validated while building. */
    size_t node_count = plan_node_count(plan);
    ParsedNode *table = node_count ? calloc(node_count, sizeof(ParsedNode)) : NULL;
    if (node_count && !table) {
        return json_fail(reader, PLAN_ERR_ALLOC, "form", "out of memory");
    }
    bool ok = true;
    if (node_count) {
        ok = plan_json_collect_nodes(reader, cJSON_GetObjectItemCaseSensitive(document, "form"), "form", PLAN_NODE_NONE, table,
                           node_count);
    }
    for (size_t i = 0; ok && i < node_count; i++) {
        const cJSON *composer = cJSON_GetObjectItemCaseSensitive(table[i].json, "composer");
        char composer_path[PLAN_JSON_PATH_MAX];
        json_child_path(composer_path, table[i].path, "composer");
        ok = plan_json_read_constraints(reader, plan, composer, composer_path, i + 1, plan_json_add_node_constraint) &&
             (reader->version < 5 || plan_node_kind(plan, (NodeId)(i + 1)) != PLAN_NODE_SECTION ||
              plan_json_read_section_member(reader, plan, composer, composer_path, i + 1)) &&
             (reader->version < 5 || (NodeId)(i + 1) != plan_root(plan) || plan_json_read_form_record(reader, plan, composer, composer_path));
    }
    /* A plan before format 5 knows its form by the name of its form node: its record and the closed sections come from the built-in
       form of that name, before any node is Given. */
    if (ok && reader->version < 5 && reader->hooks && reader->hooks->form_record_migrate &&
        reader->hooks->form_record_migrate(plan, reader->hooks->data) != PLAN_OK) {
        ok = json_fail(reader, PLAN_ERR_ALLOC, "form", "out of memory");
    }
    for (size_t i = 0; ok && i < node_count; i++) {
        const cJSON *composer = cJSON_GetObjectItemCaseSensitive(table[i].json, "composer");
        char composer_path[PLAN_JSON_PATH_MAX];
        json_child_path(composer_path, table[i].path, "composer");
        int state;
        ok = json_read_enum_member(reader, composer, composer_path, "state", plan_json_STATE_NAMES, COUNT_OF(plan_json_STATE_NAMES), &state);
        if (ok) {
            plan_node_set_state(plan, (NodeId)(i + 1), (PlanState)state);
        }
    }
    free(table);
    if (!ok) {
        return false;
    }

    const char *const SECTIONS[] = {"meter", plan_json_regions_section_name(reader), "chords", "melody", "texture"};
    for (size_t s = 0; s < COUNT_OF(SECTIONS); s++) {
        const cJSON *section = cJSON_GetObjectItemCaseSensitive(document, SECTIONS[s]);
        if (section && strcmp(SECTIONS[s], "meter") == 0) {
            section = cJSON_GetObjectItemCaseSensitive(section, "spans");
        }
        const cJSON *entry;
        cJSON_ArrayForEach(entry, section) {
            const cJSON *composer = cJSON_GetObjectItemCaseSensitive(entry, "composer");
            const cJSON *id = cJSON_GetObjectItemCaseSensitive(composer, "id");
            const cJSON *state_item = cJSON_GetObjectItemCaseSensitive(composer, "state");
            char at[PLAN_JSON_PATH_MAX];
            snprintf(at, sizeof(at), "%s[%d].composer.state", SECTIONS[s], 0);
            int state;
            if (!json_read_enum(reader, state_item, at, plan_json_STATE_NAMES, COUNT_OF(plan_json_STATE_NAMES), &state)) {
                return false;
            }
            plan_lane_span_set_state(plan, (LaneSpanId)id->valueint, (PlanState)state);
        }
    }
    return true;
}

/*
 * Version 2 records metadata.analyzer_version only for a plan that carries one (an earlier Composer loaded it from an
 * Analyzer result). A version 1 file wrote the
 * Composer's own version under that key (B-A-10), so it says nothing about an analysis and is ignored.
 */
static bool read_metadata(JsonReader *reader, const cJSON *document, Plan *plan) {
    const cJSON *metadata = cJSON_GetObjectItemCaseSensitive(document, "metadata");
    if (reader->version == 1 || !metadata) {
        return true;
    }
    if (!cJSON_IsObject(metadata)) {
        return json_bad_format(reader, "metadata", "expected an object");
    }
    const cJSON *analyzer = cJSON_GetObjectItemCaseSensitive(metadata, "analyzer_version");
    if (!analyzer) {
        return true;
    }
    if (!cJSON_IsString(analyzer)) {
        return json_bad_format(reader, "metadata.analyzer_version", "expected a string");
    }
    if (plan_set_analyzer_version(plan, analyzer->valuestring) != PLAN_OK) {
        return json_bad_format(reader, "metadata.analyzer_version", "expected a version of fewer than 32 characters");
    }
    return true;
}

/* Format 6: "derived_from", the reading and the realization the plan was made from, kept. The metadata's producer members are read by nobody here. */
static bool read_derived_from(JsonReader *reader, const cJSON *document, Plan *plan) {
    const cJSON *object = reader->version >= 6 ? cJSON_GetObjectItemCaseSensitive(document, "derived_from") : NULL;
    if (!object || cJSON_IsNull(object)) {
        return true;
    }
    if (!cJSON_IsObject(object)) {
        return json_bad_format(reader, "derived_from", "expected an object");
    }
    const cJSON *reading = plan_json_member(reader, object, "derived_from", "reading", true);
    const cJSON *realization = reading ? plan_json_member(reader, object, "derived_from", "realization", true) : NULL;
    if (!reading || !realization) {
        return false;
    }
    if (!cJSON_IsString(reading)) {
        return json_bad_format(reader, "derived_from.reading", "expected a string");
    }
    if (!cJSON_IsString(realization)) {
        return json_bad_format(reader, "derived_from.realization", "expected a string");
    }
    PlanDerivation derivation;
    memset(&derivation, 0, sizeof(derivation));
    if (strlen(reading->valuestring) >= sizeof(derivation.reading) || strlen(realization->valuestring) >= sizeof(derivation.realization)) {
        return json_bad_format(reader, "derived_from", "the reading or the realization is too long");
    }
    snprintf(derivation.reading, sizeof(derivation.reading), "%s", reading->valuestring);
    snprintf(derivation.realization, sizeof(derivation.realization), "%s", realization->valuestring);
    if (plan_set_derived_from(plan, &derivation) != PLAN_OK) {
        return json_bad_format(reader, "derived_from", "expected a reading name and a realization digest of 16 lowercase hexadecimal digits");
    }
    return true;
}

/* Everything after the version: the metadata, the form, the lane spans and, from version 4, the next note id. */
static PlanResult read_document_body(JsonReader *reader, const cJSON *document, Plan *plan) {
    if (!read_metadata(reader, document, plan) || !read_derived_from(reader, document, plan)) {
        return json_result(reader);
    }
    const cJSON *form = plan_json_member(reader, document, "", "form", true);
    if (!form) {
        return json_result(reader);
    }
    if (!cJSON_IsNull(form) && !plan_json_build_nodes(reader, plan, form)) {
        return json_result(reader);
    }
    if (!plan_json_build_lane_spans(reader, plan, document)) {
        return json_result(reader);
    }
    if (reader->version >= 4) {
        long next;
        if (!json_read_int_member(reader, document, "", "next_note_id", 1, 0x7fffffffffffffffL, &next)) {
            return json_result(reader);
        }
        if ((size_t)next <= reader->note_ids->max) {
            json_bad_format(reader, "next_note_id", "must be above every note id");
            return json_result(reader);
        }
        plan_note_next_id_set(plan, (PlanNoteId)next);
    }
    if (!apply_states(reader, plan, document)) {
        return json_result(reader);
    }
    return PLAN_OK;
}

static PlanResult read_document(const cJSON *document, Plan *plan, const CommonPracticeMaterials *materials,
                                const KnowledgeSnapshot *knowledge, const PlanReadHooks *hooks, PlanJsonError *error) {
    JsonReader reader = {error, {0}, 0, materials, knowledge, NULL, hooks};
    common_json_reader_init(&reader.json, error ? error->path : NULL, PLAN_JSON_PATH_MAX, error ? error->message : NULL,
                            error ? sizeof(error->message) : 0, PLAN_ERR_FORMAT, "missing required field");
    if (!cJSON_IsObject(document)) {
        json_bad_format(&reader, "", "expected a JSON object");
        return json_result(&reader);
    }
    long version;
    if (cJSON_GetObjectItemCaseSensitive(document, "format")) {
        const cJSON *format = cJSON_GetObjectItemCaseSensitive(document, "format");
        /* "ursatz-plan" is format 6; "ursatz-composer-plan" was the name of formats 2 to 5. */
        bool current = cJSON_IsString(format) && strcmp(format->valuestring, PLAN_FORMAT_NAME) == 0;
        if (!current && !(cJSON_IsString(format) && strcmp(format->valuestring, PLAN_FORMAT_NAME_LEGACY) == 0)) {
            json_bad_format(&reader, "format", "expected \"" PLAN_FORMAT_NAME "\"");
            return json_result(&reader);
        }
        if (!json_read_int_member(&reader, document, "", "format_version", current ? 6 : 2, current ? URSATZ_PLAN_FORMAT_VERSION : 5,
                                  &version)) {
            return json_result(&reader);
        }
    } else if (!json_read_int_member(&reader, document, "", "schema_version", 1, PLAN_JSON_SCHEMA_VERSION, &version)) {
        return json_result(&reader);
    }
    reader.version = (int)version;
    struct NoteIdSet note_ids = {NULL, 0, 0, 0};
    if (reader.version >= 4) {
        reader.note_ids = &note_ids;
    }
    PlanResult result = read_document_body(&reader, document, plan);
    free(note_ids.slots);
    return result;
}

PlanResult plan_from_json_resolved(const KnowledgeSnapshot *knowledge, const char *text, const PlanReadHooks *hooks, Plan **out,
                                   PlanJsonError *error) {
    PlanResult result = plan_from_json(knowledge, text, hooks, out, error);
    if (result != PLAN_OK) {
        return result;
    }
    result = plan_resolve_key_spans(*out, knowledge, hooks, error);
    if (result != PLAN_OK) {
        plan_destroy(*out);
        *out = NULL;
    }
    return result;
}

PlanResult plan_from_json(const KnowledgeSnapshot *knowledge, const char *text, const PlanReadHooks *hooks, Plan **out,
                          PlanJsonError *error) {
    if (!text || !out) {
        json_set_error(error, -1, "", "text or out is NULL");
        return PLAN_ERR_NULL;
    }
    *out = NULL;
    CommonPracticeMaterials materials;
    if (!common_practice_materials_resolve(knowledge, &materials, NULL, 0)) {
        json_set_error(error, -1, "", "a knowledge snapshot is required");
        return PLAN_ERR_NULL;
    }
    const char *end = NULL;
    cJSON *document = cJSON_ParseWithLengthOpts(text, strlen(text) + 1, &end, true);
    if (!document) {
        json_set_error(error, end ? (long)(end - text) : -1, "", "not valid JSON");
        return PLAN_ERR_PARSE;
    }
    Plan *plan = plan_create();
    if (!plan) {
        cJSON_Delete(document);
        json_set_error(error, -1, "", "out of memory");
        return PLAN_ERR_ALLOC;
    }
    PlanResult result = read_document(document, plan, &materials, knowledge, hooks, error);
    cJSON_Delete(document);
    if (result != PLAN_OK) {
        plan_destroy(plan);
        return result;
    }
    *out = plan;
    return PLAN_OK;
}
