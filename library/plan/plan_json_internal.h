#ifndef URSATZ_PLAN_JSON_INTERNAL_H
#define URSATZ_PLAN_JSON_INTERNAL_H

#include <stdbool.h>

#include <cJSON.h>
#include <common/json.h>
#include <theory/knowledge/knowledge_snapshot.h>
#include <theory/tonality/spelled_key.h>
#include <theory/tonality/common_practice_materials.h>
#include "plan/plan_json.h"

/* Writing helpers shared by plan_json.c and fill_record.c. */

/* Adds item under key, taking ownership; false (item freed) on failure or when item is NULL. */
bool plan_json_add(cJSON *object, const char *key, cJSON *item);
bool plan_json_append(cJSON *array, cJSON *item);
cJSON *json_number(long value);

/* Reading helpers of plan_json.c. */

typedef struct {
    PlanJsonError *error;
    CommonJsonReader json;                    /* the first refusal and where it is written (common/json.h); the result is json_result */
    int version; /* the plan file's format version being read: 1 to 4; 0 where no plan file is read */
    const CommonPracticeMaterials *materials; /* the materials a plan's keys are made of; NULL where no plan file is read */
    const KnowledgeSnapshot *knowledge;       /* finds the material a version 3 key names by id; NULL where no plan file is read */
    struct NoteIdSet *note_ids;               /* version 4: the note ids read so far, to refuse a repeat; NULL otherwise */
    const PlanReadHooks *hooks;               /* the reading program's hooks; NULL for none */
} JsonReader;

/* PLAN_OK, or the result of the first refusal. */
static inline PlanResult json_result(const JsonReader *reader) {
    return reader->json.failed ? (PlanResult)reader->json.code : PLAN_OK;
}

void json_set_error(PlanJsonError *error, long offset, const char *path, const char *format, ...);

/* Records the first failure only; always returns false. */
bool json_fail(JsonReader *reader, PlanResult result, const char *path, const char *format, ...);
bool json_bad_format(JsonReader *reader, const char *path, const char *message);

void json_child_path(char *out, const char *parent, const char *key);
void json_index_path(char *out, const char *parent, size_t index);

/* The member of object named key. NULL when absent, after reporting it if required. */
const cJSON *plan_json_member(JsonReader *reader, const cJSON *object, const char *path, const char *key, bool required);
bool json_read_long(JsonReader *reader, const cJSON *item, const char *path, long min, long max, long *out);
bool json_read_int_member(JsonReader *reader, const cJSON *object, const char *path, const char *key, long min,
                          long max, long *out);
/* Reads the "span" member, {"start", "end"}, into a start and a positive length. */
bool json_read_span(JsonReader *reader, const cJSON *object, const char *path, PlanSpan *out);

#define JSON_ACCOMPANIMENT_COUNT 6
extern const char *const JSON_ACCOMPANIMENT_NAMES[JSON_ACCOMPANIMENT_COUNT];

/* The members of a key as a constraint and a key span write them ("tonic", "material"), and read them. */
bool json_key_members(cJSON *object, const SpelledKey *key);
bool json_read_key_members(JsonReader *reader, const cJSON *object, const char *path, SpelledKey *out);
/* A member that names one of `count` names: its index in *out; the path of the member is reported on a refusal. */
bool json_read_enum(JsonReader *reader, const cJSON *item, const char *path, const char *const *names, size_t count, int *out);
bool json_read_enum_member(JsonReader *reader, const cJSON *object, const char *path, const char *key, const char *const *names,
                           size_t count, int *out);
/* An optional boolean member: *out is left as it was when the member is absent. */
bool json_read_optional_bool(JsonReader *reader, const cJSON *object, const char *path, const char *key, bool *out);

/* Shared by plan_json.c, plan_json_nodes.c, plan_json_lanes.c and plan_json_constraints.c (the split by section). */

/* The materials a file of format 1 or 2 means by "major" and "minor". */
#define MATERIAL_ID_MAJOR "omt.material.major"

#define MATERIAL_ID_MINOR "omt.material.natural-minor"

/* A version 1 file wrote a chord with no function as "unknown". */
#define LEGACY_FUNCTION_NONE "unknown"

#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

#define TIME_LIMIT 1000000000L

typedef struct {
    const cJSON *json;
    NodeId parent;
    char path[PLAN_JSON_PATH_MAX];
} ParsedNode;

/* The note ids a version 4 file has named so far: open addressing, so a repeat is found at once. */
struct NoteIdSet {
    size_t *slots;
    size_t capacity;
    size_t count;
    size_t max; /* the highest id seen */
};

/* Reads composer.constraints and adds each one through add. */
typedef PlanResult (*AddConstraintFn)(Plan *plan, size_t id, const PlanConstraint *constraint);

/* The sizes of the shared name tables (plan_json.c holds them). */
#define PLAN_JSON_NODE_KIND_COUNT 6
#define PLAN_JSON_STATE_COUNT 3
#define PLAN_JSON_MODE_COUNT 2
#define PLAN_JSON_KEY_ANCHOR_COUNT 2
#define PLAN_JSON_ARCHETYPE_COUNT 3
#define PLAN_JSON_PIECE_PART_COUNT 4
#define PLAN_JSON_FORM_CHECK_COUNT 5

extern const char *const plan_json_NODE_KINDS[PLAN_JSON_NODE_KIND_COUNT];
extern const char *const plan_json_STATE_NAMES[PLAN_JSON_STATE_COUNT];
extern const char *const plan_json_MODE_NAMES[PLAN_JSON_MODE_COUNT];
extern const char *const plan_json_KEY_ANCHOR_NAMES[PLAN_JSON_KEY_ANCHOR_COUNT];
extern const char *const plan_json_ARCHETYPE_NAMES[PLAN_JSON_ARCHETYPE_COUNT];
extern const char *const plan_json_PIECE_PART_NAMES[PLAN_JSON_PIECE_PART_COUNT];
cJSON *plan_json_span_json(PlanSpan span);
extern const char *const plan_json_FORM_CHECK_NAMES[PLAN_JSON_FORM_CHECK_COUNT];
bool plan_json_read_rational_member(JsonReader *reader, const cJSON *object, const char *path, const char *key, Rational *out);
bool plan_json_key_payload_json(cJSON *object, const SpelledKey *key, const PlanKeyRelation *relation);
cJSON *plan_json_node_constraints_json(const Plan *plan, NodeId id);
cJSON *plan_json_span_constraints_json(const Plan *plan, LaneSpanId id);
bool plan_json_read_borrowed_from(JsonReader *reader, const cJSON *object, const char *path, PlanHarmony *harmony);
bool plan_json_read_key_span(JsonReader *reader, const cJSON *object, const char *path, SpelledKey *key, PlanKeyRelation *relation);
bool plan_json_read_function(JsonReader *reader, const cJSON *object, const char *path, PlanHarmony *harmony);
const char *plan_json_result_name(PlanResult result);
PlanResult plan_json_add_node_constraint(Plan *plan, size_t id, const PlanConstraint *constraint);
PlanResult plan_json_add_span_constraint(Plan *plan, size_t id, const PlanConstraint *constraint);
bool plan_json_read_constraints(JsonReader *reader, Plan *plan, const cJSON *composer, const char *composer_path, size_t id, AddConstraintFn add_constraint);
bool plan_json_read_section_member(JsonReader *reader, Plan *plan, const cJSON *composer, const char *composer_path, size_t id);
bool plan_json_read_form_record(JsonReader *reader, Plan *plan, const cJSON *composer, const char *composer_path);
cJSON *plan_json_node_json(const Plan *plan, NodeId id);
bool plan_json_collect_nodes(JsonReader *reader, const cJSON *node, const char *path, NodeId parent, ParsedNode *table, size_t count);
bool plan_json_build_nodes(JsonReader *reader, Plan *plan, const cJSON *form);
cJSON *plan_json_lane_array_json(const Plan *plan, unsigned mask);
const char *plan_json_regions_section_name(const JsonReader *reader);
bool plan_json_build_lane_spans(JsonReader *reader, Plan *plan, const cJSON *document);

#endif
