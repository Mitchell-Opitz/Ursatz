#ifndef URSATZ_ANALYZER_JSON_UTIL_H
#define URSATZ_ANALYZER_JSON_UTIL_H

#include <stdbool.h>

#include <cJSON.h>

/* The member of an object, or NULL when object is not an object or has no such member. */
const cJSON *json_member(const cJSON *object, const char *name);

/* A string member, or NULL when it is missing or not a string. */
const char *json_string_member(const cJSON *object, const char *name);

/* A number member truncated to a long; false when it is missing or not a number. */
bool json_number_member(const cJSON *object, const char *name, long *out);

/* Adds item under key, taking ownership; false (item freed) on failure, including a NULL item. */
bool json_add(cJSON *object, const char *key, cJSON *item);

/* Appends item to array, taking ownership; false (item freed) on failure, including a NULL item. */
bool json_append(cJSON *array, cJSON *item);

#endif
