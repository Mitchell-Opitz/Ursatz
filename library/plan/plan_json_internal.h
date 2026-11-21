#ifndef URSATZ_PLAN_JSON_INTERNAL_H
#define URSATZ_PLAN_JSON_INTERNAL_H

#include <stdbool.h>

#include <cJSON.h>
#include <theory/knowledge/knowledge_snapshot.h>
#include <theory/tonality/spelled_key.h>
#include <theory/tonality/common_practice_materials.h>
#include "plan/plan_json.h"

/* Writing helpers shared by plan_json.c and fill_record.c. */

/* Adds item under key, taking ownership; false (item freed) on failure or when item is NULL. */
bool plan_json_add(cJSON *object, const char *key, cJSON *item);
bool plan_json_append(cJSON *array, cJSON *item);
cJSON *json_number(long value);
/* {"num": n, "den": d} as given: not reduced, so a score fraction keeps its written form. */
/* cJSON's tab-indented print as 2-space indent, "key": value, and a trailing newline. Caller frees. */
char *json_indent_with_spaces(const char *printed);

/* Reading helpers of plan_json.c. */

typedef struct {
    PlanJsonError *error;
    PlanResult result;
    int version; /* the plan file's format version being read: 1 to 4; 0 where no plan file is read */
    const CommonPracticeMaterials *materials; /* the materials a plan's keys are made of; NULL where no plan file is read */
    const KnowledgeSnapshot *knowledge;       /* finds the material a version 3 key names by id; NULL where no plan file is read */
    struct NoteIdSet *note_ids;               /* version 4: the note ids read so far, to refuse a repeat; NULL otherwise */
    const PlanReadHooks *hooks;               /* the reading program's hooks; NULL for none */
} JsonReader;

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

#endif
