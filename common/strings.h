#ifndef URSATZ_COMMON_STRINGS_H
#define URSATZ_COMMON_STRINGS_H

#include <stdbool.h>
#include <stddef.h>

/* A growing text buffer with a sticky failure flag: once an allocation fails, every append is a no-op that returns false. */
typedef struct {
    char *data;
    size_t length;
    size_t capacity;
    bool failed;
} CommonText;

void common_text_init(CommonText *text);
bool common_text_append(CommonText *text, const char *value);
bool common_text_appendf(CommonText *text, const char *format, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 2, 3)))
#endif
    ;
/* value as a quoted JSON string, escaped as cJSON escapes. */
bool common_text_append_json_string(CommonText *text, const char *value);
/* The text as a NUL-terminated malloc'd string the caller frees ("" when nothing was appended); NULL once failed. The buffer is
 * handed over: the text is empty again. */
char *common_text_take(CommonText *text);
void common_text_free(CommonText *text);

/* A list of malloc'd strings. */
typedef struct {
    char **items;
    size_t count;
    size_t capacity;
} CommonNames;

/* Adds a copy of name; false on allocation failure. */
bool common_names_add(CommonNames *names, const char *name);
bool common_names_contains(const CommonNames *names, const char *name);
/* Bytewise order. */
void common_names_sort(CommonNames *names);
void common_names_free(CommonNames *names);

/* A malloc'd copy of value; NULL for a NULL value or on allocation failure. */
char *common_strdup(const char *value);
/* Removes leading and trailing whitespace in place and returns the new length. */
size_t common_trim(char *text);
/* ASCII case-insensitive comparison, like strcmp. */
int common_compare_ci(const char *left, const char *right);
/* The current UTC time as "YYYY-MM-DDTHH:MM:SSZ". */
void common_utc_now(char out[32]);

#endif
