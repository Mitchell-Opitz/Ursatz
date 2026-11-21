#ifndef URSATZ_COMMON_JSON_H
#define URSATZ_COMMON_JSON_H

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>

#include <common/status.h>

/* ------------------------------------------------------------------ files */

/* Parses the file into *out (caller deletes). NOT_FOUND or IO when it cannot be read, FORMAT naming the path when it does not parse. */
UrsatzStatus common_json_read_file(const char *path, cJSON **out, UrsatzError *error);
/* As common_json_read_file, and FORMAT when the file holds anything but a JSON object. *out is NULL on failure. */
UrsatzStatus common_json_read_object(const char *path, cJSON **out, UrsatzError *error);
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
/* Adds the string text under name, or a JSON null when text is NULL. false on allocation failure. */
bool common_json_add_string_or_null(cJSON *object, const char *name, const char *text);
/* Adds the string text under name when text is not NULL (a NULL text adds nothing and succeeds). false on allocation failure. */
bool common_json_add_string_if(cJSON *object, const char *name, const char *text);
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

/* ------------------------------------------------------------------ reader */

/* The longest path a reader writes ("work.movements[0].lines[1].notes[2].id"); a longer one is cut. */
#define COMMON_JSON_PATH_MAX 128

/*
 * The state of a reader that reads a document and refuses it at the first problem: where the refusal goes (a path and a message
 * the caller owns, either may be NULL), whether one happened (the first stays), the codec's own result code for it, the code
 * of a value that is wrong (`bad_code`) and the text of a missing member. A codec keeps its own error type and result codes and
 * copies from here.
 */
typedef struct {
    char *path;
    size_t path_size;
    char *message;
    size_t message_size;
    bool failed;
    int code;
    int bad_code;
    const char *missing_text;
} CommonJsonReader;

/* A reader that has not failed. path and message (with their sizes) are where a refusal is written; NULL for neither. */
void common_json_reader_init(CommonJsonReader *reader, char *path, size_t path_size, char *message, size_t message_size,
                             int bad_code, const char *missing_text);
/* Records the first refusal: `code`, the printf-style message and the path. Later ones change nothing. Always false. */
bool common_json_refuse(CommonJsonReader *reader, int code, const char *path, const char *format, ...);
/* common_json_refuse with the reader's bad_code and the message as it is. Always false. */
bool common_json_bad(CommonJsonReader *reader, const char *path, const char *message);
/* "parent.key", or "key" for an empty parent, written into out (cut to size). */
void common_json_child_path(char *out, size_t size, const char *parent, const char *key);
/* "parent[index]". */
void common_json_index_path(char *out, size_t size, const char *parent, size_t index);

/* The member of object, or NULL after refusing it as missing (the reader's missing_text, at "path.key"). */
const cJSON *common_json_require(CommonJsonReader *reader, const cJSON *object, const char *path, const char *key);
/* True for an object; else refuses "expected an object" at path. */
bool common_json_expect_object(CommonJsonReader *reader, const cJSON *item, const char *path);
/* The array member, with its path ("path.key") written into at; NULL after refusing it missing or "expected an array". */
const cJSON *common_json_require_array(CommonJsonReader *reader, const cJSON *object, const char *path, const char *key, char *at,
                                       size_t at_size);
/* A whole number from min to max; else refuses "expected a number" or "expected an integer from min to max". */
bool common_json_read_whole(CommonJsonReader *reader, const cJSON *item, const char *path, long long min, long long max,
                            long long *out);
bool common_json_require_whole(CommonJsonReader *reader, const cJSON *object, const char *path, const char *key, long long min,
                               long long max, long long *out);
/* A boolean member; else refuses missing or "expected true or false". */
bool common_json_require_bool(CommonJsonReader *reader, const cJSON *object, const char *path, const char *key, bool *out);
/*
 * A time {"num", "den"} with num from -limit to limit and den from 1 to limit, not reduced. A value that is not an object is
 * refused with shape_message; a member that is missing or out of range with the whole-number messages at its own path.
 */
bool common_json_read_time(CommonJsonReader *reader, const cJSON *item, const char *path, long long limit, const char *shape_message,
                           long long *num, long long *den);

#endif
