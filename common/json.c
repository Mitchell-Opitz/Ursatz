#include "common/json.h"

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
