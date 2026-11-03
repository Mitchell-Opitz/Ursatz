#ifndef URSATZ_COMPOSER_PLAN_JSON_INTERNAL_H
#define URSATZ_COMPOSER_PLAN_JSON_INTERNAL_H

#include <stdbool.h>

#include <cJSON.h>
#include <ursatz_composer/plan_json.h>

/* Writing helpers shared by plan_json.c and fill_record.c. */

/* Adds item under key, taking ownership; false (item freed) on failure or when item is NULL. */
bool plan_json_add(cJSON *object, const char *key, cJSON *item);
bool plan_json_append(cJSON *array, cJSON *item);
cJSON *json_number(long value);
/* {"num": n, "den": d} as given: not reduced, so a score fraction keeps its written form. */
cJSON *rational_json(long long numerator, long long denominator);
/* cJSON's tab-indented print as 2-space indent, "key": value, and a trailing newline. Caller frees. */
char *json_indent_with_spaces(const char *printed);

/* Reading helpers shared by plan_json.c and analysis_load.c. */

typedef struct {
    PlanJsonError *error;
    PlanResult result;
    int version; /* the plan file's format version being read: 1 or 2; 0 where no plan file is read */
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

#endif
