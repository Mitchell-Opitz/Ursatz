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
/* The materials a file of format 1 or 2 means by "major" and "minor". */
#define MATERIAL_ID_MAJOR "omt.material.major"
#define MATERIAL_ID_MINOR "omt.material.natural-minor"

static const char *const NODE_KINDS[] = {"form", "section", "phrase", "subphrase", "idea", "motive"};
static const char *const STATE_NAMES[] = {"constrained", "given", "realized"};
/* The "mode" of a key in plan formats 1 and 2, which are only read. */
static const char *const MODE_NAMES[] = {"major", "minor"};
/* A key span's anchor, in PlanKeyAnchor order. */
static const char *const KEY_ANCHOR_NAMES[] = {"home", "previous"};
/* A version 1 file wrote a chord with no function as "unknown". */
#define LEGACY_FUNCTION_NONE "unknown"
static const char *const ARCHETYPE_NAMES[] = {"sentence", "period", "hybrid"};
/* The parts of a game loop, in PiecePart order (piece.h). */
static const char *const PIECE_PART_NAMES[] = {"intro", "body", "turnaround", "ending"};
/* The accompaniment patterns by name, in PlanAccompanimentPattern order (a setting return and an accompaniment constraint). */
const char *const JSON_ACCOMPANIMENT_NAMES[JSON_ACCOMPANIMENT_COUNT] = {"any", "block", "arpeggiated", "alberti", "oom-pah", "motive"};

#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

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

static cJSON *span_json(PlanSpan span) {
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

/* A region's payload: a tonal region (a key), with the relation it is named by beside it (R-597); a span not yet resolved is only the relation. */
#define REGION_KIND_TONAL "tonal"
static const char *key_anchor_text(const PlanKeyRelation *relation) {
    return relation->anchor == PLAN_KEY_ANCHOR_SECTION ? relation->anchor_section : KEY_ANCHOR_NAMES[relation->anchor];
}

static bool key_payload_json(cJSON *object, const SpelledKey *key, const PlanKeyRelation *relation) {
    if (relation && !relation->resolved) {
        return plan_json_add(object, "relation", cJSON_CreateString(relation->relation)) &&
               plan_json_add(object, "anchor", cJSON_CreateString(key_anchor_text(relation)));
    }
    return plan_json_add(object, "kind", cJSON_CreateString(REGION_KIND_TONAL)) && json_key_members(object, key) &&
           (!relation || (plan_json_add(object, "relation", cJSON_CreateString(relation->relation)) &&
                          plan_json_add(object, "anchor", cJSON_CreateString(key_anchor_text(relation)))));
}

static cJSON *constraint_json(const Plan *plan, const PlanConstraint *constraint) {
    const ConstraintKindRow *row = constraint_kind_row(constraint->kind);
    cJSON *object = row ? cJSON_CreateObject() : NULL;
    if (!object) {
        return NULL;
    }
    if (!plan_json_add(object, "kind", cJSON_CreateString(row->name)) || !row->write(object, plan, constraint)) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *node_constraints_json(const Plan *plan, NodeId id) {
    cJSON *array = cJSON_CreateArray();
    for (size_t i = 0; array && i < plan_node_constraint_count(plan, id); i++) {
        if (!plan_json_append(array, constraint_json(plan, plan_node_constraint_at(plan, id, i)))) {
            cJSON_Delete(array);
            return NULL;
        }
    }
    return array;
}

static cJSON *span_constraints_json(const Plan *plan, LaneSpanId id) {
    cJSON *array = cJSON_CreateArray();
    for (size_t i = 0; array && i < plan_lane_span_constraint_count(plan, id); i++) {
        if (!plan_json_append(array, constraint_json(plan, plan_lane_span_constraint_at(plan, id, i)))) {
            cJSON_Delete(array);
            return NULL;
        }
    }
    return array;
}

/* The checks a form runs, by name, in FormCheck bit order. */
static const char *const FORM_CHECK_NAMES[] = {"sections", "returns", "keys", "cadences", "seam"};

static cJSON *form_record_json(const PlanFormRecord *record) {
    cJSON *object = cJSON_CreateObject();
    cJSON *checks = cJSON_CreateArray();
    bool ok = object && checks;
    for (size_t i = 0; ok && i < COUNT_OF(FORM_CHECK_NAMES); i++) {
        ok = (record->checks & (1u << i)) == 0 || plan_json_append(checks, cJSON_CreateString(FORM_CHECK_NAMES[i]));
    }
    ok = ok && plan_json_add(object, "name", cJSON_CreateString(record->name)) &&
         plan_json_add(object, "version", json_number((long)record->version)) &&
         plan_json_add(object, "hash", cJSON_CreateString(record->hash)) && plan_json_add(object, "checks", checks);
    if (ok) {
        checks = NULL;
        ok = plan_json_add(object, "closing", cJSON_CreateBool(record->closing));
    }
    if (!ok) {
        cJSON_Delete(object);
        cJSON_Delete(checks);
        return NULL;
    }
    return object;
}

static cJSON *node_json(const Plan *plan, NodeId id) {
    cJSON *object = cJSON_CreateObject();
    cJSON *composer = cJSON_CreateObject();
    cJSON *children = cJSON_CreateArray();
    const char *name = plan_node_name(plan, id);
    bool ok = object && composer && children;
    ok = ok && plan_json_add(composer, "id", json_number((long)plan_node_rank(plan, id))) &&
         plan_json_add(composer, "state", cJSON_CreateString(STATE_NAMES[plan_node_state(plan, id)])) &&
         plan_json_add(composer, "constraints", node_constraints_json(plan, id));
    if (ok && plan_node_kind(plan, id) == PLAN_NODE_SECTION) {
        ok = plan_json_add(composer, "section", section_to_json(plan_node_section(plan, id), SECTION_USE_PLAN));
    }
    if (ok && id == plan_root(plan) && plan_form_record(plan)) {
        ok = plan_json_add(composer, "form", form_record_json(plan_form_record(plan)));
    }
    ok = ok && plan_json_add(object, "kind", cJSON_CreateString(NODE_KINDS[plan_node_kind(plan, id)])) &&
         plan_json_add(object, "label", name ? cJSON_CreateString(name) : cJSON_CreateNull()) &&
         plan_json_add(object, "span", span_json(plan_node_span(plan, id)));
    for (size_t i = 0; ok && i < plan_node_child_count(plan, id); i++) {
        ok = plan_json_append(children, node_json(plan, plan_node_child_at(plan, id, i)));
    }
    ok = ok && plan_json_add(object, "children", children);
    if (ok) {
        children = NULL;
        ok = plan_json_add(object, "composer", composer);
        composer = NULL;
    }
    if (!ok) {
        cJSON_Delete(object);
        cJSON_Delete(composer);
        cJSON_Delete(children);
        return NULL;
    }
    return object;
}

static void plan_key_label(const SpelledKey *key, char *out, size_t size) {
    char tonic[8] = "?";
    pitch_spelling_name(key->tonic_letter, key->tonic_accidental, tonic, sizeof(tonic));
    snprintf(out, size, "%s %s", tonic, plan_key_word(key));
}

static bool lane_label(const PlanLanePayload *payload, PlanLaneKind lane, char *out, size_t size) {
    switch (lane) {
    case PLAN_LANE_METER:
        snprintf(out, size, "%d/%d", payload->meter.beats_per_bar, payload->meter.beat_unit);
        return true;
    case PLAN_LANE_REGION:
        plan_key_label(&payload->region, out, size);
        return true;
    case PLAN_LANE_HARMONY:
        snprintf(out, size, "%s", payload->harmony.roman_numeral);
        return true;
    case PLAN_LANE_BASS:
    case PLAN_LANE_MELODY:
    case PLAN_LANE_INNER_VOICE:
        snprintf(out, size, "%s", plan_lane_names[lane]);
        return true;
    }
    return false;
}

/* How a note was made from its source, by the name the file gives it (the index is the PlanNoteOperation). */
static const char *const plan_note_operation_names[] = {"", "copy", "motive", "return"};
#define PLAN_NOTE_OPERATION_COUNT (sizeof(plan_note_operation_names) / sizeof(plan_note_operation_names[0]))

static cJSON *notes_json(const PlanNotes *notes) {
    cJSON *array = cJSON_CreateArray();
    for (size_t i = 0; array && i < notes->note_count; i++) {
        const PlanNote *note = &notes->notes[i];
        cJSON *object = cJSON_CreateObject();
        char spelling[8];
        bool spelt = !note->spelled || pitch_spelling_name(note->letter, note->accidental, spelling, sizeof(spelling));
        if (!(object && spelt && plan_json_add(object, "midi_pitch", json_number(note->midi)) &&
              (!note->spelled || plan_json_add(object, "spelling", cJSON_CreateString(spelling))) &&
              plan_json_add(object, "offset", common_json_rational_object(note->offset.num, note->offset.den)) &&
              plan_json_add(object, "duration", common_json_rational_object(note->duration.num, note->duration.den)) &&
              plan_json_add(object, "id", json_number((double)note->id)) &&
              (note->source == PLAN_NOTE_NONE ||
               (plan_json_add(object, "source", json_number((double)note->source)) &&
                plan_json_add(object, "operation", cJSON_CreateString(plan_note_operation_names[note->operation])))) &&
              plan_json_append(array, object))) {
            if (object && !cJSON_GetObjectItem(object, "duration")) {
                cJSON_Delete(object);
            }
            cJSON_Delete(array);
            return NULL;
        }
    }
    return array;
}

static cJSON *payload_json(PlanLaneKind lane, const PlanLanePayload *payload, const PlanKeyRelation *relation) {
    cJSON *object = cJSON_CreateObject();
    if (!object) {
        return NULL;
    }
    bool ok = true;
    switch (lane) {
    case PLAN_LANE_METER:
        ok = plan_json_add(object, "beats_per_bar", json_number(payload->meter.beats_per_bar)) &&
             plan_json_add(object, "beat_unit", json_number(payload->meter.beat_unit)) &&
             plan_json_add(object, "bars_per_hyperbar", json_number(payload->meter.bars_per_hyperbar));
        break;
    case PLAN_LANE_REGION:
        ok = key_payload_json(object, &payload->region, relation);
        break;
    case PLAN_LANE_HARMONY:
        ok = plan_json_add(object, "roman_numeral", cJSON_CreateString(payload->harmony.roman_numeral));
        if (ok && payload->harmony.has_function) {
            const char *name = harmonic_function_name(payload->harmony.function);
            ok = name && plan_json_add(object, "function", cJSON_CreateString(name));
        }
        break;
    case PLAN_LANE_BASS:
    case PLAN_LANE_MELODY:
    case PLAN_LANE_INNER_VOICE:
        ok = plan_json_add(object, "notes", notes_json(&payload->notes));
        break;
    }
    if (!ok) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The label of a key span: the key's, or for a span not yet resolved the relation and its anchor. */
static void key_span_label(const Plan *plan, LaneSpanId id, char *out, size_t size) {
    const PlanKeyRelation *relation = plan_lane_span_key_relation(plan, id);
    if (relation && !relation->resolved) {
        snprintf(out, size, "%s from %s", relation->relation, KEY_ANCHOR_NAMES[relation->anchor]);
    } else {
        plan_key_label(&plan_lane_span_payload(plan, id)->region, out, size);
    }
}

static cJSON *lane_entry_json(const Plan *plan, LaneSpanId id) {
    PlanLaneKind lane = plan_lane_span_lane(plan, id);
    PlanSpan span = plan_lane_span_span(plan, id);
    const PlanLanePayload *payload = plan_lane_span_payload(plan, id);
    char label[64];
    lane_label(payload, lane, label, sizeof(label));
    if (lane == PLAN_LANE_REGION) {
        key_span_label(plan, id, label, sizeof(label));
    }

    cJSON *object = cJSON_CreateObject();
    cJSON *composer = cJSON_CreateObject();
    bool ok = object && composer;
    ok = ok && plan_json_add(composer, "id", json_number((long)plan_lane_span_rank(plan, id))) &&
         plan_json_add(composer, "owner", json_number((long)plan_node_rank(plan, plan_lane_span_owner(plan, id)))) &&
         plan_json_add(composer, "lane", cJSON_CreateString(plan_lane_names[lane])) &&
         plan_json_add(composer, "voice", json_number(plan_lane_span_voice(plan, id))) &&
         plan_json_add(composer, "state", cJSON_CreateString(STATE_NAMES[plan_lane_span_state(plan, id)])) &&
         plan_json_add(composer, "constraints", span_constraints_json(plan, id)) &&
         plan_json_add(composer, "payload", payload_json(lane, payload, plan_lane_span_key_relation(plan, id)));
    ok = ok && plan_json_add(object, "span", span_json(span)) && plan_json_add(object, "label", cJSON_CreateString(label));
    if (ok) {
        ok = plan_json_add(object, "composer", composer);
        composer = NULL;
    }
    if (!ok) {
        cJSON_Delete(object);
        cJSON_Delete(composer);
        return NULL;
    }
    return object;
}

/* The claims for every lane span whose lane is in mask, in id order. */
static cJSON *lane_array_json(const Plan *plan, unsigned mask) {
    cJSON *array = cJSON_CreateArray();
    for (LaneSpanId id = 1; array && id <= plan_lane_span_count(plan); id++) {
        if (plan_has_lane_span(plan, id) && (mask & (1u << plan_lane_span_lane(plan, id))) && !plan_json_append(array, lane_entry_json(plan, id))) {
            cJSON_Delete(array);
            return NULL;
        }
    }
    return array;
}

static cJSON *meter_json(const Plan *plan) {
    cJSON *spans = lane_array_json(plan, 1u << PLAN_LANE_METER);
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
              plan_json_add(document, "regions", lane_array_json(plan, 1u << PLAN_LANE_REGION)) &&
              plan_json_add(document, "chords", lane_array_json(plan, 1u << PLAN_LANE_HARMONY)) &&
              plan_json_add(document, "form", plan_node_count(plan) > 0 ? node_json(plan, plan_root(plan)) : cJSON_CreateNull()) &&
              plan_json_add(document, "melody", lane_array_json(plan, 1u << PLAN_LANE_MELODY)) &&
              plan_json_add(document, "texture", lane_array_json(plan, (1u << PLAN_LANE_BASS) | (1u << PLAN_LANE_INNER_VOICE)));
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

/* A return constraint's variation, by the name the one table holds. */
static bool read_return_variation(JsonReader *reader, const cJSON *object, const char *path,
                                  SectionReturnVariation *out) {
    const cJSON *item = plan_json_member(reader, object, path, "variation", true);
    if (!item) {
        return false;
    }
    char at[PLAN_JSON_PATH_MAX];
    json_child_path(at, path, "variation");
    if (!cJSON_IsString(item)) {
        return json_bad_format(reader, at, "expected a string");
    }
    if (!section_return_variation_find(item->valuestring, out)) {
        return json_fail(reader, PLAN_ERR_FORMAT, at, "unknown value '%.40s'", item->valuestring);
    }
    return true;
}

/* A return's setting (R-518): required for a setting return, refused for any other. */
/* A return as plan formats 1 to 4 wrote it: the section it returns by id, how it varies it, the setting and whether it crosses modes. */
typedef struct {
    NodeId section;
    SectionReturnVariation variation;
    PlanAccompanimentPattern setting;
    bool across_modes;
} LegacyReturn;

static bool read_return_setting(JsonReader *reader, const cJSON *object, const char *path, LegacyReturn *out) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, "setting");
    char at[PLAN_JSON_PATH_MAX];
    json_child_path(at, path, "setting");
    if (out->variation != SECTION_RETURN_SETTING) {
        return item ? json_bad_format(reader, at, "only a setting return has a setting") : true;
    }
    int value;
    if (!item) {
        return json_bad_format(reader, at, "a setting return names its setting");
    }
    if (!json_read_enum_member(reader, object, path, "setting", JSON_ACCOMPANIMENT_NAMES, COUNT_OF(JSON_ACCOMPANIMENT_NAMES), &value)) {
        return false;
    }
    if (value < (int)PLAN_ACCOMPANIMENT_BLOCK || value > (int)PLAN_ACCOMPANIMENT_OOM_PAH) {
        return json_fail(reader, PLAN_ERR_FORMAT, at, "unknown value '%.40s'", JSON_ACCOMPANIMENT_NAMES[value]);
    }
    out->setting = (PlanAccompanimentPattern)value;
    return true;
}

/* A theme pairing's beginning or ending by its plan name (the section tables'). */
static bool read_theme_half(JsonReader *reader, const cJSON *object, const char *path, const char *key, bool beginning,
                            int *out) {
    const cJSON *item = plan_json_member(reader, object, path, key, true);
    if (!item) {
        return false;
    }
    char at[PLAN_JSON_PATH_MAX];
    json_child_path(at, path, key);
    if (!cJSON_IsString(item)) {
        return json_bad_format(reader, at, "expected a string");
    }
    SectionBeginning found_beginning;
    SectionEnding found_ending;
    if (beginning ? !section_beginning_find(item->valuestring, &found_beginning) : !section_ending_find(item->valuestring, &found_ending)) {
        return json_fail(reader, PLAN_ERR_FORMAT, at, "unknown value '%.40s'", item->valuestring);
    }
    *out = beginning ? (int)found_beginning : (int)found_ending;
    return true;
}

/* An idea by the name the section tables give it; `at` is its path. */
static bool read_idea_name(JsonReader *reader, const cJSON *item, const char *at, int *out) {
    if (!cJSON_IsString(item)) {
        return json_bad_format(reader, at, "expected an idea name");
    }
    SectionIdea idea;
    if (!section_idea_find(item->valuestring, &idea)) {
        return json_fail(reader, PLAN_ERR_FORMAT, at, "unknown idea '%.40s'", item->valuestring);
    }
    *out = (int)idea;
    return true;
}

/* A construction constraint's ideas, extensions and reason. */
static bool read_construction(JsonReader *reader, const cJSON *item, const char *path, SectionConstruction *out) {
    char at[PLAN_JSON_PATH_MAX];
    int value;
    long number;
    const cJSON *ideas = plan_json_member(reader, item, path, "ideas", true);
    if (!ideas) {
        return false;
    }
    json_child_path(at, path, "ideas");
    if (!cJSON_IsObject(ideas)) {
        return json_bad_format(reader, at, "expected an object of idea names and bars");
    }
    for (const cJSON *member = ideas->child; member; member = member->next) {
        char member_at[PLAN_JSON_PATH_MAX];
        SectionIdea idea;
        json_child_path(member_at, at, member->string);
        if (!section_idea_find(member->string, &idea)) {
            return json_fail(reader, PLAN_ERR_FORMAT, member_at, "unknown idea '%.40s'", member->string);
        }
        if (!json_read_long(reader, member, member_at, 0, 16, &number)) {
            return false;
        }
        out->idea_bars[idea] = (unsigned char)number;
    }
    const cJSON *extensions = plan_json_member(reader, item, path, "extensions", true);
    if (!extensions) {
        return false;
    }
    json_child_path(at, path, "extensions");
    if (!cJSON_IsArray(extensions)) {
        return json_bad_format(reader, at, "expected an array");
    }
    if ((size_t)cJSON_GetArraySize(extensions) > SECTION_CONSTRUCTION_EXTENSIONS) {
        return json_fail(reader, PLAN_ERR_FORMAT, at, "at most %d extensions", SECTION_CONSTRUCTION_EXTENSIONS);
    }
    size_t index = 0;
    for (const cJSON *extension = extensions->child; extension; extension = extension->next, index++) {
        char extension_at[PLAN_JSON_PATH_MAX];
        json_index_path(extension_at, at, index);
        if (!cJSON_IsObject(extension)) {
            return json_bad_format(reader, extension_at, "expected an object");
        }
        SectionExtension *target = &out->extensions[index];
        const cJSON *idea;
        const cJSON *varied;
        const char *kind_names[] = {section_extension_kind_name(SECTION_EXTENSION_REPEAT),
                                    section_extension_kind_name(SECTION_EXTENSION_STRETCH),
                                    section_extension_kind_name(SECTION_EXTENSION_ADDITION),
                                    section_extension_kind_name(SECTION_EXTENSION_EVADED)};
        if (!json_read_enum_member(reader, extension, extension_at, "kind", kind_names, COUNT_OF(kind_names), &value)) {
            return false;
        }
        target->kind = (SectionExtensionKind)value;
        if (!(idea = plan_json_member(reader, extension, extension_at, "idea", true))) {
            return false;
        }
        char idea_at[PLAN_JSON_PATH_MAX];
        json_child_path(idea_at, extension_at, "idea");
        if (!read_idea_name(reader, idea, idea_at, &value)) {
            return false;
        }
        target->idea = (unsigned char)value;
        if (!json_read_int_member(reader, extension, extension_at, "bars", 0, 4, &number)) {
            return false;
        }
        target->bars = (unsigned char)number;
        if (!(varied = plan_json_member(reader, extension, extension_at, "varied", true))) {
            return false;
        }
        if (!cJSON_IsBool(varied)) {
            char varied_at[PLAN_JSON_PATH_MAX];
            json_child_path(varied_at, extension_at, "varied");
            return json_bad_format(reader, varied_at, "expected true or false");
        }
        target->varied = cJSON_IsTrue(varied);
        out->extension_count = index + 1;
    }
    const char *reason_names[] = {section_construction_reason_name(SECTION_CONSTRUCTION_GIVEN),
                                  section_construction_reason_name(SECTION_CONSTRUCTION_FROM_MOTIVE),
                                  section_construction_reason_name(SECTION_CONSTRUCTION_FROM_LENGTH)};
    if (!json_read_enum_member(reader, item, path, "reason", reason_names, COUNT_OF(reason_names), &value)) {
        return false;
    }
    out->reason = (SectionConstructionReason)value;
    return true;
}

#define TIME_LIMIT 1000000000L

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

static bool read_rational_member(JsonReader *reader, const cJSON *object, const char *path, const char *key,
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
    if (!read_rational_member(reader, item, at, "start", &out->start) ||
        !read_rational_member(reader, item, at, "end", &end)) {
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

/* The optional across_modes of a reharmonized return (R-524); true on any other variation is refused. */
static bool read_return_across_modes(JsonReader *reader, const cJSON *object, const char *path, LegacyReturn *out) {
    if (!json_read_optional_bool(reader, object, path, "across_modes", &out->across_modes)) {
        return false;
    }
    if (out->across_modes && out->variation != SECTION_RETURN_REHARMONIZE) {
        char at[PLAN_JSON_PATH_MAX];
        json_child_path(at, path, "across_modes");
        return json_bad_format(reader, at, "only a reharmonized return crosses modes");
    }
    return true;
}





/*
 * Plan formats 1 and 2 name a key's mode, "major" or "minor", where format 3 names a material (R-597). The migration reads them as
 * the materials the snapshot holds for those words: the major as omt.material.major, the minor as omt.material.natural-minor. A
 * file of format 1 or 2 has no relation beside a key either: a key span reads back with none.
 */
static bool read_legacy_material(JsonReader *reader, const cJSON *object, const char *path, const PitchMaterial **out) {
    int mode;
    if (!json_read_enum_member(reader, object, path, "mode", MODE_NAMES, COUNT_OF(MODE_NAMES), &mode)) {
        return false;
    }
    *out = knowledge_snapshot_material(reader->knowledge, mode == 1 ? MATERIAL_ID_MINOR : MATERIAL_ID_MAJOR);
    if (!*out) {
        char at[PLAN_JSON_PATH_MAX];
        json_child_path(at, path, "mode");
        return json_bad_format(reader, at, "the knowledge snapshot has no material for this mode");
    }
    return true;
}

/*
 * Version 1 files (R-260): a key is "tonic_pitch_class" and "mode", with no spelling. The tonic is spelled as the library
 * spells the key on that pitch class (standard_tonality_key_tonic_of_pitch_class_material: the fewest accidentals in its
 * signature, sharps on a tie), which is how every plan of version 1 was written.
 */
static bool read_key_v1(JsonReader *reader, const cJSON *object, const char *path, SpelledKey *out) {
    const PitchMaterial *material;
    long pitch_class;
    PitchLetter letter;
    int accidental;
    if (!read_legacy_material(reader, object, path, &material) ||
        !json_read_int_member(reader, object, path, "tonic_pitch_class", 0, 11, &pitch_class)) {
        return false;
    }
    if (!standard_tonality_key_tonic_of_pitch_class_material((int)pitch_class, material, &letter, &accidental)) {
        char at[PLAN_JSON_PATH_MAX];
        json_child_path(at, path, "tonic_pitch_class");
        return json_bad_format(reader, at, "no key on this pitch class");
    }
    SpelledKey key = {NULL, letter, accidental, material};
    *out = key;
    return true;
}

/* A tonic member: a spelled name ("C", "F#", "Bb"). */
static bool read_tonic_member(JsonReader *reader, const cJSON *object, const char *path, PitchLetter *letter, int *accidental) {
    const cJSON *tonic = plan_json_member(reader, object, path, "tonic", true);
    if (!tonic) {
        return false;
    }
    char at[PLAN_JSON_PATH_MAX];
    json_child_path(at, path, "tonic");
    if (!cJSON_IsString(tonic) || !pitch_spelling_parse_name(tonic->valuestring, letter, accidental, NULL)) {
        return json_bad_format(reader, at, "expected a tonic like \"C\", \"F#\" or \"Bb\"");
    }
    return true;
}

/* A version 3 key: "tonic" and "material", the id of a material the Composer composes in. */
static bool read_key_v3(JsonReader *reader, const cJSON *object, const char *path, SpelledKey *out) {
    const cJSON *id = plan_json_member(reader, object, path, "material", true);
    if (!id) {
        return false;
    }
    char at[PLAN_JSON_PATH_MAX];
    json_child_path(at, path, "material");
    if (!cJSON_IsString(id)) {
        return json_bad_format(reader, at, "expected a material id like \"omt.material.major\"");
    }
    const PitchMaterial *material = knowledge_snapshot_material(reader->knowledge, id->valuestring);
    if (!material) {
        return json_bad_format(reader, at, "no material has this id");
    }
    if (reader->hooks && reader->hooks->material_supported && !reader->hooks->material_supported(material, reader->hooks->data)) {
        return json_bad_format(reader, at, "the Composer composes in major and natural minor keys only");
    }
    PitchLetter letter;
    int accidental;
    if (!read_tonic_member(reader, object, path, &letter, &accidental)) {
        return false;
    }
    SpelledKey key = {NULL, letter, accidental, material};
    *out = key;
    return true;
}

/* A version 2 key: "tonic" and "mode". */
static bool read_key_v2(JsonReader *reader, const cJSON *object, const char *path, SpelledKey *out) {
    const PitchMaterial *material;
    PitchLetter letter;
    int accidental;
    if (!read_legacy_material(reader, object, path, &material) || !read_tonic_member(reader, object, path, &letter, &accidental)) {
        return false;
    }
    SpelledKey key = {NULL, letter, accidental, material};
    *out = key;
    return true;
}

/* A key's members, by the version of the file: see read_key_v1, read_key_v2 and read_key_v3. */
bool json_read_key_members(JsonReader *reader, const cJSON *object, const char *path, SpelledKey *out) {
    switch (reader->version) {
    case 1:
        return read_key_v1(reader, object, path, out);
    case 2:
        return read_key_v2(reader, object, path, out);
    default:
        return read_key_v3(reader, object, path, out);
    }
}

/* A key span's optional "relation" and "anchor" beside its key (version 3): both or neither. *relation is left empty for neither. */
static bool read_key_relation(JsonReader *reader, const cJSON *object, const char *path, PlanKeyRelation *relation) {
    memset(relation, 0, sizeof(*relation));
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(object, "relation");
    const cJSON *anchor = cJSON_GetObjectItemCaseSensitive(object, "anchor");
    if (!id && !anchor) {
        return true;
    }
    char at[PLAN_JSON_PATH_MAX];
    if (!id || !anchor) {
        json_child_path(at, path, id ? "anchor" : "relation");
        return json_bad_format(reader, at, "a key names its relation and its anchor together");
    }
    json_child_path(at, path, "relation");
    if (!cJSON_IsString(id) || id->valuestring[0] == '\0' || strlen(id->valuestring) >= PLAN_KEY_RELATION_MAX) {
        return json_bad_format(reader, at, "expected a relation id like \"omt.relation.dominant\"");
    }
    json_child_path(at, path, "anchor");
    if (!cJSON_IsString(anchor) || anchor->valuestring[0] == '\0' || strlen(anchor->valuestring) >= PLAN_KEY_ANCHOR_SECTION_MAX) {
        return json_bad_format(reader, at, "expected \"home\", \"previous\" or the name of an earlier section");
    }
    /* "home" and "previous" are the plan's own; any other name is a section (format 5). */
    int found = PLAN_KEY_ANCHOR_SECTION;
    for (int i = 0; i < (int)COUNT_OF(KEY_ANCHOR_NAMES); i++) {
        if (strcmp(anchor->valuestring, KEY_ANCHOR_NAMES[i]) == 0) {
            found = i;
        }
    }
    if (found == PLAN_KEY_ANCHOR_SECTION && reader->version < 5) {
        return json_bad_format(reader, at, "expected \"home\" or \"previous\"");
    }
    snprintf(relation->relation, PLAN_KEY_RELATION_MAX, "%s", id->valuestring);
    relation->anchor = (PlanKeyAnchor)found;
    if (found == PLAN_KEY_ANCHOR_SECTION) {
        snprintf(relation->anchor_section, PLAN_KEY_ANCHOR_SECTION_MAX, "%s", anchor->valuestring);
    }
    relation->resolved = true;
    return true;
}

/*
 * The payload of a key span: its key and, in version 3, the relation it is named by. A version 3 span with a "relation" and an
 * "anchor" and neither a "tonic" nor a "material" is named only by them: its key is the zero key until plan_resolve_key_spans.
 */
static bool read_key_span(JsonReader *reader, const cJSON *object, const char *path, SpelledKey *key, PlanKeyRelation *relation) {
    memset(relation, 0, sizeof(*relation));
    /* Format 6: a region names its kind; only the tonal one (a key) is built. */
    const cJSON *kind = reader->version >= 6 ? cJSON_GetObjectItemCaseSensitive(object, "kind") : NULL;
    if (kind) {
        char kind_path[PLAN_JSON_PATH_MAX];
        json_child_path(kind_path, path, "kind");
        if (!cJSON_IsString(kind)) {
            return json_bad_format(reader, kind_path, "expected a string");
        }
        if (strcmp(kind->valuestring, REGION_KIND_TONAL) != 0) {
            return json_fail(reader, PLAN_ERR_FORMAT, kind_path, "region kind '%.40s' is not built until P69", kind->valuestring);
        }
    }
    if (reader->version >= 3 && cJSON_GetObjectItemCaseSensitive(object, "relation") &&
        !cJSON_GetObjectItemCaseSensitive(object, "tonic") && !cJSON_GetObjectItemCaseSensitive(object, "material")) {
        SpelledKey zero;
        memset(&zero, 0, sizeof(zero));
        *key = zero;
        if (!read_key_relation(reader, object, path, relation)) {
            return false;
        }
        relation->resolved = false;
        return true;
    }
    return json_read_key_members(reader, object, path, key) &&
           (reader->version < 3 || read_key_relation(reader, object, path, relation));
}

/* A chord's "function": a harmonic_function_name, absent for none. Version 1 always wrote it, "unknown" for none. */
static bool read_function(JsonReader *reader, const cJSON *object, const char *path, PlanHarmony *harmony) {
    harmony->has_function = false;
    harmony->function = HARMONIC_FUNCTION_TONIC;
    const cJSON *item = plan_json_member(reader, object, path, "function", reader->version == 1);
    if (!item) {
        return !reader->json.failed;
    }
    char at[PLAN_JSON_PATH_MAX];
    json_child_path(at, path, "function");
    if (!cJSON_IsString(item)) {
        return json_bad_format(reader, at, "expected a string");
    }
    if (reader->version == 1 && strcmp(item->valuestring, LEGACY_FUNCTION_NONE) == 0) {
        return true;
    }
    for (int function = 0; function < (int)HARMONIC_FUNCTION_COUNT; function++) {
        if (strcmp(item->valuestring, harmonic_function_name((HarmonicFunction)function)) == 0) {
            harmony->has_function = true;
            harmony->function = (HarmonicFunction)function;
            return true;
        }
    }
    return json_fail(reader, PLAN_ERR_FORMAT, at, "unknown value '%.40s'", item->valuestring);
}

/* A figure constraint: every member required; change_at is a bar from 1 or null for none. */
static bool read_figure(JsonReader *reader, const cJSON *item, const char *path, SectionFigure *out) {
    const cJSON *kind = plan_json_member(reader, item, path, "figure", true);
    const cJSON *rhythm = kind ? plan_json_member(reader, item, path, "rhythm", true) : NULL;
    const cJSON *harmony = rhythm ? plan_json_member(reader, item, path, "harmony", true) : NULL;
    const cJSON *change_at = harmony ? plan_json_member(reader, item, path, "change_at", true) : NULL;
    const cJSON *climax = change_at ? plan_json_member(reader, item, path, "climax", true) : NULL;
    if (!climax) {
        return false;
    }
    char at[PLAN_JSON_PATH_MAX];
    json_child_path(at, path, "figure");
    if (!cJSON_IsString(kind)) {
        return json_bad_format(reader, at, "expected a string");
    }
    if (!section_figure_kind_find(kind->valuestring, &out->kind)) {
        return json_fail(reader, PLAN_ERR_FORMAT, at, "unknown value '%.40s'", kind->valuestring);
    }
    json_child_path(at, path, "rhythm");
    if (!cJSON_IsString(rhythm)) {
        return json_bad_format(reader, at, "expected a string");
    }
    if (!section_figure_rhythm_find(rhythm->valuestring, &out->rhythm)) {
        return json_fail(reader, PLAN_ERR_FORMAT, at, "unknown value '%.40s'", rhythm->valuestring);
    }
    json_child_path(at, path, "harmony");
    if (!cJSON_IsString(harmony)) {
        return json_bad_format(reader, at, "expected a string");
    }
    if (strcmp(harmony->valuestring, "kept") == 0) {
        out->reharmonize = false;
    } else if (strcmp(harmony->valuestring, "reharmonized") == 0) {
        out->reharmonize = true;
    } else {
        return json_fail(reader, PLAN_ERR_FORMAT, at, "unknown value '%.40s'", harmony->valuestring);
    }
    json_child_path(at, path, "change_at");
    out->change_at = 0;
    if (!cJSON_IsNull(change_at)) {
        long value;
        if (!json_read_long(reader, change_at, at, 1, TIME_LIMIT, &value)) {
            return false;
        }
        out->change_at = (unsigned)value;
    }
    json_child_path(at, path, "climax");
    if (!cJSON_IsBool(climax)) {
        return json_bad_format(reader, at, "expected true or false");
    }
    out->climax = cJSON_IsTrue(climax);
    return true;
}

/* A section function constraint: function, independent and lyric, all required. */
static bool read_section_function(JsonReader *reader, const cJSON *item, const char *path, SectionFunction *out) {
    const cJSON *function = plan_json_member(reader, item, path, "function", true);
    const cJSON *independent = function ? plan_json_member(reader, item, path, "independent", true) : NULL;
    const cJSON *lyric = independent ? plan_json_member(reader, item, path, "lyric", true) : NULL;
    if (!lyric) {
        return false;
    }
    char at[PLAN_JSON_PATH_MAX];
    json_child_path(at, path, "function");
    if (!cJSON_IsString(function)) {
        return json_bad_format(reader, at, "expected a string");
    }
    if (!section_function_kind_find(function->valuestring, &out->function)) {
        return json_fail(reader, PLAN_ERR_FORMAT, at, "unknown value '%.40s'", function->valuestring);
    }
    json_child_path(at, path, "independent");
    if (!cJSON_IsBool(independent)) {
        return json_bad_format(reader, at, "expected true or false");
    }
    out->independent = cJSON_IsTrue(independent);
    json_child_path(at, path, "lyric");
    if (!cJSON_IsBool(lyric)) {
        return json_bad_format(reader, at, "expected true or false");
    }
    out->lyric = cJSON_IsTrue(lyric);
    return true;
}

/*
 * The constraints plan formats 1 to 4 wrote on a section node, which are the members of its description from format 5 on (R-607):
 * archetype (derived from the pairing), material, theme_pairing, return, layers, section_letter, piece_part, free_phrase, hold,
 * seam, pedal, construction, connective, liquidation, figure and section_function; a section's cadence constraints are its cadence
 * and, second, its first cadence. section_from_old_constraints is the one place that reads them: apply_old_constraint takes
 * one into the description, and the reader sets the description once the node's constraints are read.
 */
static const char *const LEGACY_KINDS[] = {"archetype", "material", "theme_pairing", "return", "layers", "section_letter",
                                              "piece_part", "free_phrase", "hold", "seam", "pedal", "construction", "connective",
                                              "liquidation", "figure", "section_function"};
#define LEGACY_COUNT COUNT_OF(LEGACY_KINDS)

/* The index of the kind a constraint object names in LEGACY_KINDS, or -1 for any other kind. */
static int old_constraint_kind(const cJSON *item) {
    const cJSON *kind = cJSON_IsObject(item) ? cJSON_GetObjectItemCaseSensitive(item, "kind") : NULL;
    for (size_t i = 0; cJSON_IsString(kind) && i < LEGACY_COUNT; i++) {
        if (strcmp(kind->valuestring, LEGACY_KINDS[i]) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static const char *result_name(PlanResult result);

static bool apply_old_constraint(JsonReader *reader, const Plan *plan, const cJSON *item, const char *path, int legacy,
                                 SectionDescription *d) {
    int value;
    long number;
    char at[PLAN_JSON_PATH_MAX];
    switch (legacy) {
    case 0: /* archetype: the pairing says it */
        return json_read_enum_member(reader, item, path, "type", ARCHETYPE_NAMES, COUNT_OF(ARCHETYPE_NAMES), &value);
    case 1: /* material */
        d->has_role = true;
        d->role = SECTION_ROLE_MATERIAL;
        return true;
    case 2: { /* theme_pairing */
        int ending;
        if (!read_theme_half(reader, item, path, "beginning", true, &value) ||
            !read_theme_half(reader, item, path, "ending", false, &ending)) {
            return false;
        }
        d->has_pairing = true;
        d->pairing.beginning = (SectionBeginning)value;
        d->pairing.ending = (SectionEnding)ending;
        return true;
    }
    case 3: { /* return */
        LegacyReturn ret;
        memset(&ret, 0, sizeof(ret));
        if (!json_read_int_member(reader, item, path, "section", 1, TIME_LIMIT, &number) ||
            !read_return_variation(reader, item, path, &ret.variation)) {
            return false;
        }
        ret.section = (NodeId)number;
        if (!read_return_setting(reader, item, path, &ret) || !read_return_across_modes(reader, item, path, &ret)) {
            return false;
        }
        if (!plan_has_node(plan, ret.section) || plan_node_kind(plan, ret.section) != PLAN_NODE_SECTION) {
            return json_fail(reader, PLAN_ERR_FORMAT, path, "constraint %s", result_name(PLAN_ERR_BAD_PAYLOAD));
        }
        const char *name = plan_node_name(plan, ret.section);
        snprintf(d->returns, sizeof(d->returns), "%s", name ? name : "");
        d->variation = ret.variation;
        d->setting = (SectionSetting)ret.setting;
        d->mode = ret.across_modes ? SECTION_MODE_HOME : SECTION_MODE_NONE;
        return true;
    }
    case 4: { /* layers */
        const cJSON *lanes = plan_json_member(reader, item, path, "lanes", true);
        if (!lanes) {
            return false;
        }
        char lanes_path[PLAN_JSON_PATH_MAX];
        json_child_path(lanes_path, path, "lanes");
        if (!cJSON_IsArray(lanes)) {
            return json_bad_format(reader, lanes_path, "expected an array of lane names");
        }
        size_t index = 0;
        const cJSON *lane;
        d->layers = 0;
        cJSON_ArrayForEach(lane, lanes) {
            json_index_path(at, lanes_path, index++);
            if (!json_read_enum(reader, lane, at, plan_lane_names, PLAN_LANE_NAME_COUNT, &value)) {
                return false;
            }
            d->layers |= PLAN_LAYER_BIT(value);
        }
        return d->layers != 0 ? true : json_bad_format(reader, lanes_path, "a layer sounds at least one lane");
    }
    case 5: { /* section_letter */
        const cJSON *letter = plan_json_member(reader, item, path, "letter", true);
        if (!letter) {
            return false;
        }
        json_child_path(at, path, "letter");
        if (!cJSON_IsString(letter) || letter->valuestring[0] == '\0' || strlen(letter->valuestring) >= SECTION_LETTER_MAX) {
            return json_bad_format(reader, at, "expected one to three characters");
        }
        snprintf(d->letter, sizeof(d->letter), "%s", letter->valuestring);
        return true;
    }
    case 6: { /* piece_part */
        const cJSON *id = NULL;
        if (!json_read_enum_member(reader, item, path, "part", PIECE_PART_NAMES, COUNT_OF(PIECE_PART_NAMES), &value) ||
            !(id = plan_json_member(reader, item, path, "id", true))) {
            return false;
        }
        json_child_path(at, path, "id");
        if (!cJSON_IsString(id) || id->valuestring[0] == '\0' || strlen(id->valuestring) >= SECTION_ID_MAX) {
            return json_bad_format(reader, at, "expected one to fifteen characters");
        }
        d->has_role = true;
        d->role = (SectionRole)(SECTION_ROLE_INTRO + value);
        snprintf(d->id, sizeof(d->id), "%s", id->valuestring);
        return true;
    }
    case 7: /* free_phrase */
        d->free_phrase = true;
        return true;
    case 8: /* hold */
        d->leads_in = true;
        return true;
    case 9: /* seam */
        d->has_role = true;
        d->role = SECTION_ROLE_SEAM;
        return true;
    case 10: /* pedal */
        if (!json_read_int_member(reader, item, path, "degree", 1, 7, &number)) {
            return false;
        }
        if (number != 5) {
            return json_fail(reader, PLAN_ERR_FORMAT, path, "constraint %s", result_name(PLAN_ERR_BAD_PAYLOAD));
        }
        d->pedal = 5;
        return true;
    case 11: /* construction */
        d->has_construction = true;
        return read_construction(reader, item, path, &d->construction);
    case 12: { /* connective */
        const cJSON *written = plan_json_member(reader, item, path, "written", true);
        if (!written) {
            return false;
        }
        if (!cJSON_IsBool(written)) {
            json_child_path(at, path, "written");
            return json_bad_format(reader, at, "expected true or false");
        }
        d->connective = SECTION_CONNECTIVE_ALWAYS; /* the plan holds that it was asked for; sometimes was drawn at build */
        d->connective_written = cJSON_IsTrue(written);
        return true;
    }
    case 13: /* liquidation */
        d->has_role = true;
        d->role = SECTION_ROLE_LIQUIDATION;
        return true;
    case 14: /* figure */
        d->has_figure = true;
        return read_figure(reader, item, path, &d->figure);
    case 15: /* section_function */
        d->has_function = true;
        return read_section_function(reader, item, path, &d->function);
    }
    return false;
}

static bool read_constraint(JsonReader *reader, const cJSON *item, const char *path, PlanConstraint *out) {
    if (!cJSON_IsObject(item)) {
        return json_bad_format(reader, path, "expected an object");
    }
    memset(out, 0, sizeof(*out));
    const cJSON *named = plan_json_member(reader, item, path, "kind", true);
    char at[PLAN_JSON_PATH_MAX];
    json_child_path(at, path, "kind");
    if (!named) {
        return false;
    }
    if (!cJSON_IsString(named)) {
        return json_bad_format(reader, at, "expected a string");
    }
    /* Formats before 6 called the constraint on a region "key". */
    const char *kind_name = reader->version < 6 && strcmp(named->valuestring, "key") == 0 ? "region" : named->valuestring;
    const ConstraintKindRow *row = constraint_kind_named(kind_name);
    if (!row) {
        return json_fail(reader, PLAN_ERR_FORMAT, at, "unknown value '%.40s'", named->valuestring);
    }
    out->kind = row->kind;
    return row->read(reader, item, path, out);
}

static const char *result_name(PlanResult result) {
    switch (result) {
    case PLAN_ERR_BAD_TIME:
        return "invalid time";
    case PLAN_ERR_BAD_SPAN:
        return "span is empty or not inside its parent";
    case PLAN_ERR_BAD_KIND:
        return "this kind cannot go here";
    case PLAN_ERR_ROOT_EXISTS:
        return "a plan has one root";
    case PLAN_ERR_NOT_FOUND:
        return "refers to a missing node";
    case PLAN_ERR_LANE_OVERLAP:
        return "overlaps another span in the same lane";
    case PLAN_ERR_BAD_PAYLOAD:
        return "value is not valid here";
    case PLAN_ERR_GIVEN_LOCKED:
        return "the item is Given";
    default:
        return "could not be added";
    }
}

/* Reads composer.constraints and adds each one through add. */
typedef PlanResult (*AddConstraintFn)(Plan *plan, size_t id, const PlanConstraint *constraint);

static PlanResult add_node_constraint(Plan *plan, size_t id, const PlanConstraint *constraint) {
    return plan_node_add_constraint(plan, (NodeId)id, constraint);
}

static PlanResult add_span_constraint(Plan *plan, size_t id, const PlanConstraint *constraint) {
    return plan_lane_span_add_constraint(plan, (LaneSpanId)id, constraint);
}

static bool read_constraints(JsonReader *reader, Plan *plan, const cJSON *composer, const char *composer_path,
                             size_t id, AddConstraintFn add_constraint) {
    const cJSON *list = plan_json_member(reader, composer, composer_path, "constraints", true);
    if (!list) {
        return false;
    }
    char list_path[PLAN_JSON_PATH_MAX];
    json_child_path(list_path, composer_path, "constraints");
    if (!cJSON_IsArray(list)) {
        return json_bad_format(reader, list_path, "expected an array");
    }
    /* Formats 1 to 4 wrote a section's attributes as constraints: they are read into its description. */
    const bool old_section = add_constraint == add_node_constraint && reader->version < 5 &&
                             plan_node_kind(plan, (NodeId)id) == PLAN_NODE_SECTION;
    SectionDescription description;
    section_description_init(&description);
    size_t cadences = 0;
    size_t index = 0;
    const cJSON *item;
    cJSON_ArrayForEach(item, list) {
        char at[PLAN_JSON_PATH_MAX];
        json_index_path(at, list_path, index++);
        if (old_section) {
            const cJSON *kind = cJSON_IsObject(item) ? cJSON_GetObjectItemCaseSensitive(item, "kind") : NULL;
            int legacy = old_constraint_kind(item);
            if (legacy >= 0) {
                if (!apply_old_constraint(reader, plan, item, at, legacy, &description)) {
                    return false;
                }
                continue;
            }
            if (cJSON_IsString(kind) && strcmp(kind->valuestring, "cadence") == 0) {
                PlanConstraint cadence;
                if (!read_constraint(reader, item, at, &cadence)) {
                    return false;
                }
                /* The first is the section's, a second the weak one an antecedent ends in. */
                if (cadences == 0) {
                    description.has_cadence = true;
                    description.cadence = cadence.params.cadence;
                } else if (cadences == 1) {
                    description.has_first_cadence = true;
                    description.first_cadence = cadence.params.cadence;
                }
                cadences++;
                continue;
            }
        }
        PlanConstraint constraint;
        if (!read_constraint(reader, item, at, &constraint)) {
            return false;
        }
        if (add_constraint == add_node_constraint && constraint.kind == PLAN_CONSTRAINT_MOTIVE_TONES) {
            /* The labels are one to a note of the node's melody span. */
            LaneSpanId spans[16];
            size_t found = plan_node_lane_spans(plan, (NodeId)id, spans, 16);
            for (size_t k = 0; k < found && k < 16; k++) {
                size_t notes = plan_lane_span_payload(plan, spans[k])->notes.note_count;
                if (plan_lane_span_lane(plan, spans[k]) == PLAN_LANE_MELODY && notes != constraint.params.motive_tones.label_count) {
                    return json_fail(reader, PLAN_ERR_FORMAT, at, "motive_tones on %zu: %zu labels for %zu notes", id,
                                     constraint.params.motive_tones.label_count, notes);
                }
            }
        }
        PlanResult result = add_constraint(plan, id, &constraint);
        if (result == PLAN_ERR_ALLOC) {
            return json_fail(reader, PLAN_ERR_ALLOC, at, "out of memory");
        }
        if (result != PLAN_OK) {
            return json_fail(reader, PLAN_ERR_FORMAT, at, "constraint %s", result_name(result));
        }
    }
    if (old_section) {
        PlanResult result = plan_node_set_section(plan, (NodeId)id, &description);
        if (result != PLAN_OK) {
            return json_fail(reader, result == PLAN_ERR_ALLOC ? PLAN_ERR_ALLOC : PLAN_ERR_FORMAT, list_path, "section %s",
                             result_name(result));
        }
    }
    return true;
}

/* Format 5: a section node's "section" member is its description. */
static bool read_section_member(JsonReader *reader, Plan *plan, const cJSON *composer, const char *composer_path, size_t id) {
    const cJSON *item = plan_json_member(reader, composer, composer_path, "section", true);
    if (!item) {
        return false;
    }
    char at[PLAN_JSON_PATH_MAX];
    json_child_path(at, composer_path, "section");
    SectionDescription description;
    SectionError error;
    if (!section_from_json(item, SECTION_USE_PLAN, NULL, 0, &description, &error)) {
        char member[PLAN_JSON_PATH_MAX + 128];
        snprintf(member, sizeof(member), "%s%s", at, error.path);
        return json_fail(reader, PLAN_ERR_FORMAT, member, "%s", error.message);
    }
    PlanResult result = plan_node_set_section(plan, (NodeId)id, &description);
    if (result != PLAN_OK) {
        return json_fail(reader, result == PLAN_ERR_ALLOC ? PLAN_ERR_ALLOC : PLAN_ERR_FORMAT, at, "section %s", result_name(result));
    }
    return true;
}

/* Format 5: the form node's optional "form" member is the record of the form the plan was built from. */
static bool read_form_record(JsonReader *reader, Plan *plan, const cJSON *composer, const char *composer_path) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(composer, "form");
    if (!item) {
        return true;
    }
    char at[PLAN_JSON_PATH_MAX];
    json_child_path(at, composer_path, "form");
    if (!cJSON_IsObject(item)) {
        return json_bad_format(reader, at, "expected an object");
    }
    PlanFormRecord record;
    memset(&record, 0, sizeof(record));
    const cJSON *name = plan_json_member(reader, item, at, "name", true);
    const cJSON *hash = name ? plan_json_member(reader, item, at, "hash", true) : NULL;
    const cJSON *checks = hash ? plan_json_member(reader, item, at, "checks", true) : NULL;
    const cJSON *closing = checks ? plan_json_member(reader, item, at, "closing", true) : NULL;
    long version;
    char member[PLAN_JSON_PATH_MAX];
    if (!closing || !json_read_int_member(reader, item, at, "version", 1, 1000, &version)) {
        return false;
    }
    json_child_path(member, at, "name");
    if (!cJSON_IsString(name) || name->valuestring[0] == '\0' || strlen(name->valuestring) >= PLAN_FORM_NAME_MAX) {
        return json_bad_format(reader, member, "expected a form name of 1 to 31 characters");
    }
    json_child_path(member, at, "hash");
    if (!cJSON_IsString(hash) || strlen(hash->valuestring) > PLAN_FORM_HASH_LEN) {
        return json_bad_format(reader, member, "expected a hash of 16 lowercase hexadecimal digits or none");
    }
    json_child_path(member, at, "closing");
    if (!cJSON_IsBool(closing)) {
        return json_bad_format(reader, member, "expected true or false");
    }
    json_child_path(member, at, "checks");
    if (!cJSON_IsArray(checks)) {
        return json_bad_format(reader, member, "expected an array of check names");
    }
    size_t index = 0;
    const cJSON *check;
    cJSON_ArrayForEach(check, checks) {
        char entry[PLAN_JSON_PATH_MAX];
        int bit;
        json_index_path(entry, member, index++);
        if (!json_read_enum(reader, check, entry, FORM_CHECK_NAMES, COUNT_OF(FORM_CHECK_NAMES), &bit)) {
            return false;
        }
        if ((record.checks & (1u << bit)) != 0) {
            return json_bad_format(reader, entry, "a check is named twice");
        }
        record.checks |= 1u << bit;
    }
    snprintf(record.name, sizeof(record.name), "%s", name->valuestring);
    snprintf(record.hash, sizeof(record.hash), "%s", hash->valuestring);
    record.version = (unsigned)version;
    record.closing = cJSON_IsTrue(closing);
    PlanResult result = plan_set_form_record(plan, &record);
    if (result != PLAN_OK) {
        return json_fail(reader, result == PLAN_ERR_ALLOC ? PLAN_ERR_ALLOC : PLAN_ERR_FORMAT, at, "form %s", result_name(result));
    }
    return true;
}

typedef struct {
    const cJSON *json;
    NodeId parent;
    char path[PLAN_JSON_PATH_MAX];
} ParsedNode;

static bool count_nodes(JsonReader *reader, const cJSON *node, const char *path, size_t *count) {
    if (!cJSON_IsObject(node)) {
        return json_bad_format(reader, path, "expected an object");
    }
    (*count)++;
    const cJSON *children = plan_json_member(reader, node, path, "children", true);
    if (!children) {
        return false;
    }
    char list_path[PLAN_JSON_PATH_MAX];
    json_child_path(list_path, path, "children");
    if (!cJSON_IsArray(children)) {
        return json_bad_format(reader, list_path, "expected an array");
    }
    size_t index = 0;
    const cJSON *child;
    cJSON_ArrayForEach(child, children) {
        char at[PLAN_JSON_PATH_MAX];
        json_index_path(at, list_path, index++);
        if (!count_nodes(reader, child, at, count)) {
            return false;
        }
    }
    return true;
}

/* Files each node under its composer.id so nodes can be added in id order. */
static bool collect_nodes(JsonReader *reader, const cJSON *node, const char *path, NodeId parent, ParsedNode *table,
                          size_t count) {
    const cJSON *composer = plan_json_member(reader, node, path, "composer", true);
    if (!composer) {
        return false;
    }
    char composer_path[PLAN_JSON_PATH_MAX];
    json_child_path(composer_path, path, "composer");
    if (!cJSON_IsObject(composer)) {
        return json_bad_format(reader, composer_path, "expected an object");
    }
    long id;
    if (!json_read_int_member(reader, composer, composer_path, "id", 1, (long)count, &id)) {
        return false;
    }
    if (table[id - 1].json) {
        char id_path[PLAN_JSON_PATH_MAX];
        json_child_path(id_path, composer_path, "id");
        return json_bad_format(reader, id_path, "id is used twice");
    }
    table[id - 1].json = node;
    table[id - 1].parent = parent;
    snprintf(table[id - 1].path, PLAN_JSON_PATH_MAX, "%s", path);

    const cJSON *children = cJSON_GetObjectItemCaseSensitive(node, "children");
    char list_path[PLAN_JSON_PATH_MAX];
    json_child_path(list_path, path, "children");
    size_t index = 0;
    const cJSON *child;
    cJSON_ArrayForEach(child, children) {
        char at[PLAN_JSON_PATH_MAX];
        json_index_path(at, list_path, index++);
        if (!collect_nodes(reader, child, at, (NodeId)id, table, count)) {
            return false;
        }
    }
    return true;
}

static bool build_nodes(JsonReader *reader, Plan *plan, const cJSON *form) {
    size_t count = 0;
    if (!count_nodes(reader, form, "form", &count)) {
        return false;
    }
    ParsedNode *table = calloc(count, sizeof(ParsedNode));
    if (!table) {
        return json_fail(reader, PLAN_ERR_ALLOC, "form", "out of memory");
    }
    bool ok = collect_nodes(reader, form, "form", PLAN_NODE_NONE, table, count);
    for (size_t i = 0; ok && i < count; i++) {
        const ParsedNode *parsed = &table[i];
        int kind;
        PlanSpan span;
        char at[PLAN_JSON_PATH_MAX];
        ok = json_read_enum_member(reader, parsed->json, parsed->path, "kind", NODE_KINDS, COUNT_OF(NODE_KINDS), &kind) &&
             json_read_span(reader, parsed->json, parsed->path, &span);
        const cJSON *label = ok ? plan_json_member(reader, parsed->json, parsed->path, "label", false) : NULL;
        if (ok && label && !cJSON_IsNull(label) && !cJSON_IsString(label)) {
            json_child_path(at, parsed->path, "label");
            ok = json_bad_format(reader, at, "expected a string or null");
        }
        if (ok) {
            NodeId id;
            const char *name = cJSON_IsString(label) ? label->valuestring : NULL;
            PlanResult result = plan_add_node(plan, parsed->parent, (PlanNodeKind)kind, span, name, &id);
            if (result == PLAN_ERR_ALLOC) {
                ok = json_fail(reader, PLAN_ERR_ALLOC, parsed->path, "out of memory");
            } else if (result != PLAN_OK) {
                ok = json_fail(reader, PLAN_ERR_FORMAT, parsed->path, "node %s", result_name(result));
            }
        }
    }
    free(table);
    return ok;
}

typedef struct {
    const cJSON *json;
    unsigned lanes;
    char path[PLAN_JSON_PATH_MAX];
} ParsedLane;

static bool count_section(JsonReader *reader, const cJSON *document, const char *name, size_t *count) {
    const cJSON *section = plan_json_member(reader, document, "", name, false);
    if (!section || cJSON_IsNull(section)) {
        return true;
    }
    if (strcmp(name, "meter") == 0) {
        section = plan_json_member(reader, section, "meter", "spans", true);
        if (!section) {
            return false;
        }
    }
    if (!cJSON_IsArray(section)) {
        return json_bad_format(reader, name, "expected an array");
    }
    *count += (size_t)cJSON_GetArraySize(section);
    return true;
}

static bool collect_section(JsonReader *reader, const cJSON *document, const char *name, unsigned lanes,
                            ParsedLane *table, size_t count) {
    const cJSON *section = cJSON_GetObjectItemCaseSensitive(document, name);
    char section_path[PLAN_JSON_PATH_MAX];
    snprintf(section_path, sizeof(section_path), "%s", name);
    if (!section || cJSON_IsNull(section)) {
        return true;
    }
    if (strcmp(name, "meter") == 0) {
        section = cJSON_GetObjectItemCaseSensitive(section, "spans");
        json_child_path(section_path, name, "spans");
    }
    size_t index = 0;
    const cJSON *entry;
    cJSON_ArrayForEach(entry, section) {
        char at[PLAN_JSON_PATH_MAX];
        json_index_path(at, section_path, index++);
        if (!cJSON_IsObject(entry)) {
            return json_bad_format(reader, at, "expected an object");
        }
        const cJSON *composer = plan_json_member(reader, entry, at, "composer", true);
        if (!composer) {
            return false;
        }
        char composer_path[PLAN_JSON_PATH_MAX];
        json_child_path(composer_path, at, "composer");
        if (!cJSON_IsObject(composer)) {
            return json_bad_format(reader, composer_path, "expected an object");
        }
        long id;
        if (!json_read_int_member(reader, composer, composer_path, "id", 1, (long)count, &id)) {
            return false;
        }
        if (table[id - 1].json) {
            char id_path[PLAN_JSON_PATH_MAX];
            json_child_path(id_path, composer_path, "id");
            return json_bad_format(reader, id_path, "id is used twice");
        }
        table[id - 1].json = entry;
        table[id - 1].lanes = lanes;
        snprintf(table[id - 1].path, PLAN_JSON_PATH_MAX, "%s", at);
    }
    return true;
}

/* The note ids a version 4 file has named so far: open addressing, so a repeat is found at once. */
struct NoteIdSet {
    size_t *slots;
    size_t capacity;
    size_t count;
    size_t max; /* the highest id seen */
};

/* Adds id; false when the set already holds it or memory ran out (*grown_failed then says which). */
static bool note_id_set_add(struct NoteIdSet *set, size_t id, bool *out_of_memory) {
    *out_of_memory = false;
    if (set->count * 2 >= set->capacity) {
        size_t capacity = set->capacity ? set->capacity * 2 : 64;
        size_t *slots = calloc(capacity, sizeof(size_t));
        if (!slots) {
            *out_of_memory = true;
            return false;
        }
        for (size_t i = 0; i < set->capacity; i++) {
            if (set->slots[i]) {
                size_t at = (set->slots[i] * 11400714819323198485ull) & (capacity - 1);
                while (slots[at]) {
                    at = (at + 1) & (capacity - 1);
                }
                slots[at] = set->slots[i];
            }
        }
        free(set->slots);
        set->slots = slots;
        set->capacity = capacity;
    }
    size_t at = (id * 11400714819323198485ull) & (set->capacity - 1);
    while (set->slots[at]) {
        if (set->slots[at] == id) {
            return false;
        }
        at = (at + 1) & (set->capacity - 1);
    }
    set->slots[at] = id;
    set->count++;
    if (id > set->max) {
        set->max = id;
    }
    return true;
}

/* A version 4 note's "id" (required, unique in the file, from 1), "source" and "operation" (optional). */
static bool read_note_identity(JsonReader *reader, const cJSON *item, const char *at, PlanNote *note) {
    long id;
    if (!json_read_int_member(reader, item, at, "id", 1, 0x7fffffffffffffffL, &id)) {
        return false;
    }
    bool out_of_memory = false;
    if (reader->note_ids && !note_id_set_add(reader->note_ids, (size_t)id, &out_of_memory)) {
        char id_path[PLAN_JSON_PATH_MAX];
        json_child_path(id_path, at, "id");
        return out_of_memory ? json_fail(reader, PLAN_ERR_ALLOC, id_path, "out of memory")
                             : json_bad_format(reader, id_path, "this note id is already another note's");
    }
    note->id = (PlanNoteId)id;
    const cJSON *source = cJSON_GetObjectItemCaseSensitive(item, "source");
    if (source) {
        char source_path[PLAN_JSON_PATH_MAX];
        json_child_path(source_path, at, "source");
        long value;
        if (!json_read_long(reader, source, source_path, 1, 0x7fffffffffffffffL, &value)) {
            return false;
        }
        note->source = (PlanNoteId)value;
    }
    const cJSON *operation = cJSON_GetObjectItemCaseSensitive(item, "operation");
    if (operation) {
        char operation_path[PLAN_JSON_PATH_MAX];
        json_child_path(operation_path, at, "operation");
        int value;
        if (!json_read_enum(reader, operation, operation_path, plan_note_operation_names + 1, PLAN_NOTE_OPERATION_COUNT - 1, &value)) {
            return false;
        }
        note->operation = (PlanNoteOperation)(value + 1);
    }
    return true;
}

static bool read_payload(JsonReader *reader, const cJSON *composer, const char *composer_path, PlanLaneKind lane,
                         PlanLanePayload *payload, PlanKeyRelation *relation, PlanNote **notes_storage) {
    const cJSON *object = plan_json_member(reader, composer, composer_path, "payload", true);
    if (!object) {
        return false;
    }
    char path[PLAN_JSON_PATH_MAX];
    json_child_path(path, composer_path, "payload");
    if (!cJSON_IsObject(object)) {
        return json_bad_format(reader, path, "expected an object");
    }
    long first;
    long second;
    long third;
    memset(payload, 0, sizeof(*payload));
    switch (lane) {
    case PLAN_LANE_METER:
        if (!json_read_int_member(reader, object, path, "beats_per_bar", 1, 1000, &first) ||
            !json_read_int_member(reader, object, path, "beat_unit", 1, 1000, &second) ||
            !json_read_int_member(reader, object, path, "bars_per_hyperbar", 0, 1000, &third)) {
            return false;
        }
        payload->meter.beats_per_bar = (int)first;
        payload->meter.beat_unit = (int)second;
        payload->meter.bars_per_hyperbar = (int)third;
        return true;
    case PLAN_LANE_REGION:
        return read_key_span(reader, object, path, &payload->region, relation);
    case PLAN_LANE_HARMONY: {
        const cJSON *numeral = plan_json_member(reader, object, path, "roman_numeral", true);
        if (!numeral) {
            return false;
        }
        char numeral_path[PLAN_JSON_PATH_MAX];
        json_child_path(numeral_path, path, "roman_numeral");
        if (!cJSON_IsString(numeral)) {
            return json_bad_format(reader, numeral_path, "expected a string");
        }
        if (numeral->valuestring[0] == '\0' || strlen(numeral->valuestring) >= PLAN_ROMAN_NUMERAL_MAX) {
            return json_fail(reader, PLAN_ERR_FORMAT, numeral_path, "expected 1 to %d characters",
                        PLAN_ROMAN_NUMERAL_MAX - 1);
        }
        snprintf(payload->harmony.roman_numeral, PLAN_ROMAN_NUMERAL_MAX, "%s", numeral->valuestring);
        return read_function(reader, object, path, &payload->harmony);
    }
    case PLAN_LANE_BASS:
    case PLAN_LANE_MELODY:
    case PLAN_LANE_INNER_VOICE: {
        const cJSON *list = plan_json_member(reader, object, path, "notes", true);
        if (!list) {
            return false;
        }
        char list_path[PLAN_JSON_PATH_MAX];
        json_child_path(list_path, path, "notes");
        /* An inner voice may be silent over its span: an empty array. */
        if (!cJSON_IsArray(list) || (cJSON_GetArraySize(list) == 0 && lane != PLAN_LANE_INNER_VOICE)) {
            return json_bad_format(reader, list_path, "expected a non-empty array");
        }
        size_t count = (size_t)cJSON_GetArraySize(list);
        PlanNote *notes = count ? calloc(count, sizeof(PlanNote)) : NULL;
        if (count && !notes) {
            return json_fail(reader, PLAN_ERR_ALLOC, list_path, "out of memory");
        }
        *notes_storage = notes;
        size_t index = 0;
        const cJSON *item;
        cJSON_ArrayForEach(item, list) {
            char at[PLAN_JSON_PATH_MAX];
            json_index_path(at, list_path, index);
            if (!cJSON_IsObject(item)) {
                return json_bad_format(reader, at, "expected an object");
            }
            long pitch;
            if (!json_read_int_member(reader, item, at, "midi_pitch", 0, 127, &pitch) ||
                !read_rational_member(reader, item, at, "offset", &notes[index].offset) ||
                !read_rational_member(reader, item, at, "duration", &notes[index].duration)) {
                return false;
            }
            notes[index].midi = (int)pitch;
            if (reader->version >= 4 && !read_note_identity(reader, item, at, &notes[index])) {
                return false;
            }
            notes[index].spelled = false;
            const cJSON *spelling = cJSON_GetObjectItemCaseSensitive(item, "spelling");
            if (spelling) {
                char spelling_path[PLAN_JSON_PATH_MAX];
                json_child_path(spelling_path, at, "spelling");
                if (!cJSON_IsString(spelling) ||
                    !pitch_spelling_parse_name(spelling->valuestring, &notes[index].letter, &notes[index].accidental,
                                               NULL)) {
                    return json_bad_format(reader, spelling_path, "expected a note name like \"F#\" or \"Bb\"");
                }
                notes[index].spelled = true;
            }
            index++;
        }
        payload->notes.notes = notes;
        payload->notes.note_count = count;
        return true;
    }
    }
    return false;
}

static bool build_lane_span(JsonReader *reader, Plan *plan, const ParsedLane *parsed, size_t expected_id) {
    const cJSON *composer = cJSON_GetObjectItemCaseSensitive(parsed->json, "composer");
    char composer_path[PLAN_JSON_PATH_MAX];
    json_child_path(composer_path, parsed->path, "composer");

    int lane;
    long owner;
    long voice;
    PlanSpan span;
    if (!json_read_enum_member(reader, composer, composer_path, "lane", plan_lane_names, PLAN_LANE_NAME_COUNT, &lane) ||
        !json_read_int_member(reader, composer, composer_path, "owner", 1, TIME_LIMIT, &owner) ||
        !json_read_int_member(reader, composer, composer_path, "voice", 0, 1000, &voice) ||
        !json_read_span(reader, parsed->json, parsed->path, &span)) {
        return false;
    }
    if (!(parsed->lanes & (1u << lane))) {
        char lane_path[PLAN_JSON_PATH_MAX];
        json_child_path(lane_path, composer_path, "lane");
        return json_bad_format(reader, lane_path, "lane does not belong in this section");
    }

    PlanLanePayload payload;
    PlanKeyRelation relation;
    PlanNote *notes_storage = NULL;
    memset(&relation, 0, sizeof(relation));
    bool ok = read_payload(reader, composer, composer_path, (PlanLaneKind)lane, &payload, &relation, &notes_storage);
    if (ok) {
        LaneSpanId id;
        PlanResult result =
            plan_add_lane_span(plan, (NodeId)owner, (PlanLaneKind)lane, (int)voice, span, &payload, &id);
        if (result == PLAN_ERR_ALLOC) {
            ok = json_fail(reader, PLAN_ERR_ALLOC, parsed->path, "out of memory");
        } else if (result != PLAN_OK) {
            ok = json_fail(reader, PLAN_ERR_FORMAT, parsed->path, "lane span %s", result_name(result));
        } else if (id != expected_id) {
            ok = json_bad_format(reader, parsed->path, "lane span ids must run from 1 without gaps");
        } else if (relation.relation[0] != '\0' &&
                   plan_lane_span_set_key_relation(plan, id, relation.relation, relation.anchor,
                                                   relation.anchor == PLAN_KEY_ANCHOR_SECTION ? relation.anchor_section : NULL,
                                                   relation.resolved) != PLAN_OK) {
            ok = json_bad_format(reader, parsed->path, "a key span names its relation badly");
        }
    }
    free(notes_storage);
    return ok;
}

/* The document member that holds the region lane: "regions" from format 6, "keys" before it. */
static const char *regions_section_name(const JsonReader *reader) {
    return reader->version >= 6 ? "regions" : "keys";
}

static bool build_lane_spans(JsonReader *reader, Plan *plan, const cJSON *document) {
    struct {
        const char *name;
        unsigned lanes;
    } sections[] = {
        {"meter", 1u << PLAN_LANE_METER},
        {NULL, 1u << PLAN_LANE_REGION}, /* "regions" from format 6, "keys" before it */
        {"chords", 1u << PLAN_LANE_HARMONY},
        {"melody", 1u << PLAN_LANE_MELODY},
        {"texture", (1u << PLAN_LANE_BASS) | (1u << PLAN_LANE_INNER_VOICE)},
    };
    for (size_t i = 0; i < COUNT_OF(sections); i++) {
        if (!sections[i].name) {
            sections[i].name = regions_section_name(reader);
        }
    }
    size_t count = 0;
    for (size_t i = 0; i < COUNT_OF(sections); i++) {
        if (!count_section(reader, document, sections[i].name, &count)) {
            return false;
        }
    }
    if (count == 0) {
        return true;
    }
    ParsedLane *table = calloc(count, sizeof(ParsedLane));
    if (!table) {
        return json_fail(reader, PLAN_ERR_ALLOC, "", "out of memory");
    }
    bool ok = true;
    for (size_t i = 0; ok && i < COUNT_OF(sections); i++) {
        ok = collect_section(reader, document, sections[i].name, sections[i].lanes, table, count);
    }
    for (size_t i = 0; ok && i < count; i++) {
        ok = build_lane_span(reader, plan, &table[i], i + 1);
    }

    /* Constraints and states go on last so a Given item never refuses its own constraints. */
    for (size_t i = 0; ok && i < count; i++) {
        const cJSON *composer = cJSON_GetObjectItemCaseSensitive(table[i].json, "composer");
        char composer_path[PLAN_JSON_PATH_MAX];
        json_child_path(composer_path, table[i].path, "composer");
        ok = read_constraints(reader, plan, composer, composer_path, i + 1, add_span_constraint);
    }
    free(table);
    return ok;
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
        ok = collect_nodes(reader, cJSON_GetObjectItemCaseSensitive(document, "form"), "form", PLAN_NODE_NONE, table,
                           node_count);
    }
    for (size_t i = 0; ok && i < node_count; i++) {
        const cJSON *composer = cJSON_GetObjectItemCaseSensitive(table[i].json, "composer");
        char composer_path[PLAN_JSON_PATH_MAX];
        json_child_path(composer_path, table[i].path, "composer");
        ok = read_constraints(reader, plan, composer, composer_path, i + 1, add_node_constraint) &&
             (reader->version < 5 || plan_node_kind(plan, (NodeId)(i + 1)) != PLAN_NODE_SECTION ||
              read_section_member(reader, plan, composer, composer_path, i + 1)) &&
             (reader->version < 5 || (NodeId)(i + 1) != plan_root(plan) || read_form_record(reader, plan, composer, composer_path));
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
        ok = json_read_enum_member(reader, composer, composer_path, "state", STATE_NAMES, COUNT_OF(STATE_NAMES), &state);
        if (ok) {
            plan_node_set_state(plan, (NodeId)(i + 1), (PlanState)state);
        }
    }
    free(table);
    if (!ok) {
        return false;
    }

    const char *const SECTIONS[] = {"meter", regions_section_name(reader), "chords", "melody", "texture"};
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
            if (!json_read_enum(reader, state_item, at, STATE_NAMES, COUNT_OF(STATE_NAMES), &state)) {
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
    if (!cJSON_IsNull(form) && !build_nodes(reader, plan, form)) {
        return json_result(reader);
    }
    if (!build_lane_spans(reader, plan, document)) {
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
