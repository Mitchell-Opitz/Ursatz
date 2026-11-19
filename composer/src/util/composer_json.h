#ifndef URSATZ_COMPOSER_COMPOSER_JSON_H
#define URSATZ_COMPOSER_COMPOSER_JSON_H

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>

/*
 * The Composer's own JSON helpers: what the generation records, the motive file, the variant registry, the explanation and the tests
 * write and read with. The plan keeps its own copies (library plan), so the two never share a helper.
 */

#define COMPOSER_JSON_PATH_MAX 128
#define COMPOSER_JSON_MESSAGE_MAX 160

/* Filled on the first failure of a reader. */
typedef struct {
    char path[COMPOSER_JSON_PATH_MAX];
    char message[COMPOSER_JSON_MESSAGE_MAX];
    long offset; /* -1 where there is no byte offset */
} ComposerJsonError;

/* ------------------------------------------------------------------ writing */

/* Adds item under key, taking ownership; false (item freed) on failure or when item is NULL. */
bool composer_json_add(cJSON *object, const char *key, cJSON *item);
bool composer_json_append(cJSON *array, cJSON *item);
cJSON *composer_json_number(long value);
/* {"num": n, "den": d} as given: not reduced, so a score fraction keeps its written form. */
cJSON *composer_json_rational(long long numerator, long long denominator);
/* cJSON's tab-indented print as 2-space indent, "key": value, and a trailing newline. Caller frees. */
char *composer_json_indent(const char *printed);

/* ------------------------------------------------------------------ reading */

typedef struct {
    ComposerJsonError *error; /* NULL where the details are not wanted */
    bool failed;
} ComposerJsonReader;

/* Records the first failure only; always returns false. */
bool composer_json_fail(ComposerJsonReader *reader, const char *path, const char *format, ...);
bool composer_json_bad_format(ComposerJsonReader *reader, const char *path, const char *message);

void composer_json_child_path(char *out, const char *parent, const char *key);
void composer_json_index_path(char *out, const char *parent, size_t index);

/* The member of object named key. NULL when absent, after reporting it if required. */
const cJSON *composer_json_member(ComposerJsonReader *reader, const cJSON *object, const char *path, const char *key, bool required);
bool composer_json_read_long(ComposerJsonReader *reader, const cJSON *item, const char *path, long min, long max, long *out);
bool composer_json_read_int_member(ComposerJsonReader *reader, const cJSON *object, const char *path, const char *key, long min,
                                   long max, long *out);

#endif
