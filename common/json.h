#ifndef URSATZ_COMMON_JSON_H
#define URSATZ_COMMON_JSON_H

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>

#include <common/status.h>

/* ------------------------------------------------------------------ files */

/* Parses the file into *out (caller deletes). NOT_FOUND or IO when it cannot be read, FORMAT naming the path when it does not parse. */
UrsatzStatus common_json_read_file(const char *path, cJSON **out, UrsatzError *error);
/* Writes value atomically (common_file_write_atomic): cJSON_Print when formatted, else cJSON_PrintUnformatted. */
UrsatzStatus common_json_write_file(const char *path, const cJSON *value, bool formatted, UrsatzError *error);

/* ------------------------------------------------------------------ reading members */

/* The member of an object, or NULL when object is not an object or has no such member. */
const cJSON *common_json_member(const cJSON *object, const char *name);
/* The string member, or "" when it is missing or not a string. Owned by object. */
const char *common_json_string(const cJSON *object, const char *name);
/* The string member, or NULL when it is missing or not a string. */
const char *common_json_string_or_null(const cJSON *object, const char *name);
/* A malloc'd copy of common_json_string; NULL on allocation failure. */
char *common_json_string_copy(const cJSON *object, const char *name);
/* A number member that is a whole number in the range of long long; false otherwise. */
bool common_json_long(const cJSON *object, const char *name, long long *out);
bool common_json_bool(const cJSON *object, const char *name, bool *out);

/* ------------------------------------------------------------------ building */

/* Adds item under name, taking ownership; false (item freed) on failure, including a NULL item. */
bool common_json_add(cJSON *object, const char *name, cJSON *item);
/* Appends item to array, taking ownership; false (item freed) on failure, including a NULL item. */
bool common_json_append(cJSON *array, cJSON *item);
/* A number node from a whole number. */
cJSON *common_json_number(long long value);
/* Replaces the member name of object with the string value. false on allocation failure. */
bool common_json_set_string(cJSON *object, const char *name, const char *value);
/* Adds a copy of src's member name to dst, or null when src lacks it. false on allocation failure. */
bool common_json_add_copy_or_null(cJSON *dst, const cJSON *src, const char *name);
/* Adds a copy of src's member name to dst when it is there and not null. false on allocation failure. */
bool common_json_add_copy_if(cJSON *dst, const cJSON *src, const char *name);
/* cJSON's tab-indented print as a 2-space indent, "key": value, and a trailing newline. Caller frees; NULL on allocation failure. */
char *common_json_indent(const char *printed);

/* ------------------------------------------------------------------ rationals */

/* The forms common_json_rational_read accepts (a caller passes the ones it accepted before). */
#define COMMON_RATIONAL_OBJECT 1u       /* {"num": n, "den": d} */
#define COMMON_RATIONAL_TEXT 2u         /* "n/d" or "n" */
#define COMMON_RATIONAL_INTEGER 4u      /* a JSON whole number n, as n/1 */
#define COMMON_RATIONAL_ZERO_DEN 8u     /* a denominator of 0 is accepted (the Composer's "none"); otherwise it must be positive */

/* Reads a rational in one of the forms. The fraction is not reduced. false when value is none of them. */
bool common_json_rational_read(const cJSON *value, unsigned forms, long long *num, long long *den);
/* {"num": n, "den": d} as given: not reduced, so a score fraction keeps its written form. NULL on allocation failure. */
cJSON *common_json_rational_object(long long num, long long den);
/* "n/d" into out; false when it does not fit. */
bool common_json_rational_text(long long num, long long den, char *out, size_t capacity);

#endif
