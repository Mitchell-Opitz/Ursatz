#include "common/json.h"

#include <stdarg.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <common/files.h>
#include <common/strings.h>

UrsatzStatus common_json_read_file(const char *path, cJSON **out, UrsatzError *error) {
    *out = NULL;
    char *text;
    UrsatzStatus status = common_file_read(path, &text, NULL, error);
    if (status != URSATZ_OK) {
        return status;
    }
    cJSON *value = cJSON_Parse(text);
    free(text);
    if (!value) {
        return ursatz_error_set(error, URSATZ_ERR_FORMAT, path, "%s is not valid JSON", path);
    }
    *out = value;
    return URSATZ_OK;
}

UrsatzStatus common_json_read_object(const char *path, cJSON **out, UrsatzError *error) {
    UrsatzStatus status = common_json_read_file(path, out, error);
    if (status == URSATZ_OK && !cJSON_IsObject(*out)) {
        cJSON_Delete(*out);
        *out = NULL;
        return ursatz_error_set(error, URSATZ_ERR_FORMAT, path, "%s does not hold a JSON object", path);
    }
    return status;
}

UrsatzStatus common_json_write_file(const char *path, const cJSON *value, bool formatted, UrsatzError *error) {
    char *text = formatted ? cJSON_Print(value) : cJSON_PrintUnformatted(value);
    if (!text) {
        return ursatz_error_set(error, URSATZ_ERR_ALLOC, path, "out of memory writing %s", path);
    }
    UrsatzStatus status = common_file_write_atomic(path, text, strlen(text), error);
    free(text);
    return status;
}

const cJSON *common_json_member(const cJSON *object, const char *name) {
    return cJSON_IsObject(object) ? cJSON_GetObjectItemCaseSensitive(object, name) : NULL;
}

const char *common_json_string_or_null(const cJSON *object, const char *name) {
    const cJSON *item = common_json_member(object, name);
    return cJSON_IsString(item) && item->valuestring ? item->valuestring : NULL;
}

const char *common_json_string(const cJSON *object, const char *name) {
    const char *value = common_json_string_or_null(object, name);
    return value ? value : "";
}

char *common_json_string_copy(const cJSON *object, const char *name) {
    return common_strdup(common_json_string(object, name));
}

static bool whole_number(const cJSON *item, long long *out) {
    if (!cJSON_IsNumber(item)) {
        return false;
    }
    double value = item->valuedouble;
    if (!(value >= -9223372036854775808.0 && value < 9223372036854775808.0) || floor(value) != value) {
        return false;
    }
    *out = (long long)value;
    return true;
}

bool common_json_long(const cJSON *object, const char *name, long long *out) {
    return whole_number(common_json_member(object, name), out);
}

bool common_json_bool(const cJSON *object, const char *name, bool *out) {
    const cJSON *item = common_json_member(object, name);
    if (!cJSON_IsBool(item)) {
        return false;
    }
    *out = cJSON_IsTrue(item);
    return true;
}

bool common_json_add(cJSON *object, const char *name, cJSON *item) {
    if (!item) {
        return false;
    }
    if (!cJSON_AddItemToObject(object, name, item)) {
        cJSON_Delete(item);
        return false;
    }
    return true;
}

bool common_json_append(cJSON *array, cJSON *item) {
    if (!item) {
        return false;
    }
    if (!cJSON_AddItemToArray(array, item)) {
        cJSON_Delete(item);
        return false;
    }
    return true;
}

cJSON *common_json_number(long long value) {
    return cJSON_CreateNumber((double)value);
}

bool common_json_set_string(cJSON *object, const char *name, const char *value) {
    cJSON_DeleteItemFromObjectCaseSensitive(object, name);
    return cJSON_AddStringToObject(object, name, value) != NULL;
}

bool common_json_add_copy_or_null(cJSON *dst, const cJSON *src, const char *name) {
    const cJSON *member = cJSON_GetObjectItemCaseSensitive(src, name);
    return common_json_add(dst, name, member ? cJSON_Duplicate(member, 1) : cJSON_CreateNull());
}

bool common_json_add_copy_if(cJSON *dst, const cJSON *src, const char *name) {
    const cJSON *member = cJSON_GetObjectItemCaseSensitive(src, name);
    return !member || cJSON_IsNull(member) || common_json_add(dst, name, cJSON_Duplicate(member, 1));
}

/* cJSON prints with tabs; the project's JSON uses a 2-space indent and "key": value. */
bool common_json_add_string_or_null(cJSON *object, const char *name, const char *text) {
    return text ? cJSON_AddStringToObject(object, name, text) != NULL : cJSON_AddNullToObject(object, name) != NULL;
}

bool common_json_add_string_if(cJSON *object, const char *name, const char *text) {
    return text == NULL || cJSON_AddStringToObject(object, name, text) != NULL;
}

char *common_json_indent(const char *printed) {
    size_t tabs = 0;
    size_t length = strlen(printed);
    for (size_t i = 0; i < length; i++) {
        tabs += printed[i] == '\t';
    }
    char *out = malloc(length + tabs + 2);
    if (!out) {
        return NULL;
    }
    char *cursor = out;
    for (size_t i = 0; i < length; i++) {
        if (printed[i] == '\t' && i > 0 && printed[i - 1] == ':') {
            *cursor++ = ' ';
        } else if (printed[i] == '\t') {
            *cursor++ = ' ';
            *cursor++ = ' ';
        } else {
            *cursor++ = printed[i];
        }
    }
    *cursor++ = '\n';
    *cursor = '\0';
    return out;
}

/* "n/d" or "n": the whole of text is the fraction. */
static bool text_rational(const char *text, long long *num, long long *den) {
    char *end;
    errno = 0;
    long long n = strtoll(text, &end, 10);
    if (end == text || errno != 0) {
        return false;
    }
    long long d = 1;
    if (*end == '/') {
        const char *rest = end + 1;
        errno = 0;
        d = strtoll(rest, &end, 10);
        if (end == rest || errno != 0) {
            return false;
        }
    }
    if (*end != '\0') {
        return false;
    }
    *num = n;
    *den = d;
    return true;
}

bool common_json_rational_read(const cJSON *value, unsigned forms, long long *num, long long *den) {
    long long n, d;
    bool found = false;
    if ((forms & COMMON_RATIONAL_OBJECT) && cJSON_IsObject(value)) {
        found = common_json_long(value, "num", &n) && common_json_long(value, "den", &d);
        if (!found) {
            return false;
        }
    } else if ((forms & COMMON_RATIONAL_TEXT) && cJSON_IsString(value) && value->valuestring) {
        found = text_rational(value->valuestring, &n, &d);
        if (!found) {
            return false;
        }
    } else if ((forms & COMMON_RATIONAL_INTEGER) && cJSON_IsNumber(value)) {
        found = whole_number(value, &n);
        d = 1;
        if (!found) {
            return false;
        }
    }
    if (!found) {
        return false;
    }
    if (d == 0 ? !(forms & COMMON_RATIONAL_ZERO_DEN) : d < 0) {
        return false;
    }
    *num = n;
    *den = d;
    return true;
}

cJSON *common_json_rational_object(long long num, long long den) {
    cJSON *object = cJSON_CreateObject();
    if (object && !(common_json_add(object, "num", common_json_number(num)) && common_json_add(object, "den", common_json_number(den)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

bool common_json_rational_text(long long num, long long den, char *out, size_t capacity) {
    int written = snprintf(out, capacity, "%lld/%lld", num, den);
    return written >= 0 && (size_t)written < capacity;
}

/* ------------------------------------------------------------------ reader */

void common_json_reader_init(CommonJsonReader *reader, char *path, size_t path_size, char *message, size_t message_size,
                             int bad_code, const char *missing_text) {
    reader->path = path;
    reader->path_size = path_size;
    reader->message = message;
    reader->message_size = message_size;
    reader->failed = false;
    reader->code = 0;
    reader->bad_code = bad_code;
    reader->missing_text = missing_text;
}

bool common_json_refuse(CommonJsonReader *reader, int code, const char *path, const char *format, ...) {
    if (!reader->failed) {
        reader->failed = true;
        reader->code = code;
        if (reader->message != NULL && reader->message_size > 0) {
            va_list args;
            va_start(args, format);
            vsnprintf(reader->message, reader->message_size, format, args);
            va_end(args);
        }
        if (reader->path != NULL && reader->path_size > 0) {
            snprintf(reader->path, reader->path_size, "%s", path);
        }
    }
    return false;
}

bool common_json_bad(CommonJsonReader *reader, const char *path, const char *message) {
    return common_json_refuse(reader, reader->bad_code, path, "%s", message);
}

void common_json_child_path(char *out, size_t size, const char *parent, const char *key) {
    if (parent[0] == '\0') {
        snprintf(out, size, "%s", key);
    } else {
        snprintf(out, size, "%s.%s", parent, key);
    }
}

void common_json_index_path(char *out, size_t size, const char *parent, size_t index) {
    snprintf(out, size, "%s[%zu]", parent, index);
}

const cJSON *common_json_require(CommonJsonReader *reader, const cJSON *object, const char *path, const char *key) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    if (!item) {
        char at[COMMON_JSON_PATH_MAX];
        common_json_child_path(at, sizeof(at), path, key);
        common_json_bad(reader, at, reader->missing_text);
    }
    return item;
}

bool common_json_expect_object(CommonJsonReader *reader, const cJSON *item, const char *path) {
    return cJSON_IsObject(item) || common_json_bad(reader, path, "expected an object");
}

const cJSON *common_json_require_array(CommonJsonReader *reader, const cJSON *object, const char *path, const char *key, char *at,
                                       size_t at_size) {
    const cJSON *item = common_json_require(reader, object, path, key);
    if (!item) {
        return NULL;
    }
    common_json_child_path(at, at_size, path, key);
    if (!cJSON_IsArray(item)) {
        common_json_bad(reader, at, "expected an array");
        return NULL;
    }
    return item;
}

bool common_json_read_whole(CommonJsonReader *reader, const cJSON *item, const char *path, long long min, long long max,
                            long long *out) {
    if (!cJSON_IsNumber(item)) {
        return common_json_bad(reader, path, "expected a number");
    }
    double value = item->valuedouble;
    if (value < (double)min || value > (double)max || (double)(long long)value != value) {
        return common_json_refuse(reader, reader->bad_code, path, "expected an integer from %lld to %lld", min, max);
    }
    *out = (long long)value;
    return true;
}

bool common_json_require_whole(CommonJsonReader *reader, const cJSON *object, const char *path, const char *key, long long min,
                               long long max, long long *out) {
    const cJSON *item = common_json_require(reader, object, path, key);
    char at[COMMON_JSON_PATH_MAX];
    common_json_child_path(at, sizeof(at), path, key);
    return item && common_json_read_whole(reader, item, at, min, max, out);
}

bool common_json_require_bool(CommonJsonReader *reader, const cJSON *object, const char *path, const char *key, bool *out) {
    const cJSON *item = common_json_require(reader, object, path, key);
    if (!item) {
        return false;
    }
    if (!cJSON_IsBool(item)) {
        char at[COMMON_JSON_PATH_MAX];
        common_json_child_path(at, sizeof(at), path, key);
        return common_json_bad(reader, at, "expected true or false");
    }
    *out = cJSON_IsTrue(item);
    return true;
}

bool common_json_read_time(CommonJsonReader *reader, const cJSON *item, const char *path, long long limit, const char *shape_message,
                           long long *num, long long *den) {
    /* A time in range is read by the rational reader; any other value falls through to the checks that say what is wrong. */
    if (common_json_rational_read(item, COMMON_RATIONAL_OBJECT, num, den) && *num >= -limit && *num <= limit && *den <= limit) {
        return true;
    }
    if (!cJSON_IsObject(item)) {
        return common_json_bad(reader, path, shape_message);
    }
    char at[COMMON_JSON_PATH_MAX];
    const cJSON *n = common_json_require(reader, item, path, "num");
    common_json_child_path(at, sizeof(at), path, "num");
    if (!n || !common_json_read_whole(reader, n, at, -limit, limit, num)) {
        return false;
    }
    const cJSON *d = common_json_require(reader, item, path, "den");
    common_json_child_path(at, sizeof(at), path, "den");
    return d && common_json_read_whole(reader, d, at, 1, limit, den);
}
