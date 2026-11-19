#include "util/composer_json.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool composer_json_add(cJSON *object, const char *key, cJSON *item) {
    if (!item) {
        return false;
    }
    if (!cJSON_AddItemToObject(object, key, item)) {
        cJSON_Delete(item);
        return false;
    }
    return true;
}

bool composer_json_append(cJSON *array, cJSON *item) {
    if (!item) {
        return false;
    }
    if (!cJSON_AddItemToArray(array, item)) {
        cJSON_Delete(item);
        return false;
    }
    return true;
}

cJSON *composer_json_number(long value) {
    return cJSON_CreateNumber((double)value);
}

cJSON *composer_json_rational(long long numerator, long long denominator) {
    cJSON *object = cJSON_CreateObject();
    if (object && !(composer_json_add(object, "num", composer_json_number((long)numerator)) &&
                    composer_json_add(object, "den", composer_json_number((long)denominator)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* cJSON prints with tabs; the project's JSON uses a 2-space indent and "key": value. */
char *composer_json_indent(const char *printed) {
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

bool composer_json_fail(ComposerJsonReader *reader, const char *path, const char *format, ...) {
    if (!reader->failed) {
        reader->failed = true;
        if (reader->error) {
            va_list args;
            va_start(args, format);
            vsnprintf(reader->error->message, sizeof(reader->error->message), format, args);
            va_end(args);
            snprintf(reader->error->path, sizeof(reader->error->path), "%s", path);
            reader->error->offset = -1;
        }
    }
    return false;
}

bool composer_json_bad_format(ComposerJsonReader *reader, const char *path, const char *message) {
    return composer_json_fail(reader, path, "%s", message);
}

void composer_json_child_path(char *out, const char *parent, const char *key) {
    if (parent[0] == '\0') {
        snprintf(out, COMPOSER_JSON_PATH_MAX, "%s", key);
    } else {
        snprintf(out, COMPOSER_JSON_PATH_MAX, "%s.%s", parent, key);
    }
}

void composer_json_index_path(char *out, const char *parent, size_t index) {
    snprintf(out, COMPOSER_JSON_PATH_MAX, "%s[%zu]", parent, index);
}

const cJSON *composer_json_member(ComposerJsonReader *reader, const cJSON *object, const char *path, const char *key, bool required) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    if (!item && required) {
        char at[COMPOSER_JSON_PATH_MAX];
        composer_json_child_path(at, path, key);
        composer_json_bad_format(reader, at, "missing required field");
    }
    return item;
}

bool composer_json_read_long(ComposerJsonReader *reader, const cJSON *item, const char *path, long min, long max, long *out) {
    if (!cJSON_IsNumber(item)) {
        return composer_json_bad_format(reader, path, "expected a number");
    }
    double value = item->valuedouble;
    if (value < (double)min || value > (double)max || (double)(long)value != value) {
        return composer_json_fail(reader, path, "expected an integer from %ld to %ld", min, max);
    }
    *out = (long)value;
    return true;
}

bool composer_json_read_int_member(ComposerJsonReader *reader, const cJSON *object, const char *path, const char *key, long min,
                                   long max, long *out) {
    const cJSON *item = composer_json_member(reader, object, path, key, true);
    if (!item) {
        return false;
    }
    char at[COMPOSER_JSON_PATH_MAX];
    composer_json_child_path(at, path, key);
    return composer_json_read_long(reader, item, at, min, max, out);
}
