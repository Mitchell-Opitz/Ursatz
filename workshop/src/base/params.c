#include "base/params.h"

#include <stdio.h>
#include <stdlib.h>

#include <common/strings.h>

const char *workshop_param_string(const cJSON *params, const char *name, bool *bad) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(params, name);
    if (!item || cJSON_IsNull(item)) {
        return NULL;
    }
    if (!cJSON_IsString(item)) {
        if (bad) {
            *bad = true;
        }
        return NULL;
    }
    return item->valuestring;
}

bool workshop_param_require_string(const cJSON *params, const char *name, const char **out, UrsatzError *error) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(params, name);
    if (!cJSON_IsString(item)) {
        if (error) {
            snprintf(error->message, sizeof(error->message), "%s is required and a string", name);
            snprintf(error->path, sizeof(error->path), "/params/%s", name);
        }
        return false;
    }
    *out = item->valuestring;
    return true;
}

UrsatzStatus workshop_text_normalize(const char *text, size_t max_chars, bool required, const char *refusal, char **out) {
    *out = NULL;
    char *copy = common_strdup(text ? text : "");
    if (!copy) {
        return workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    size_t length = common_trim(copy);
    size_t chars = 0;
    for (size_t i = 0; i < length; i++) {
        unsigned char c = (unsigned char)copy[i];
        if (c < 0x20 || c == 0x7f || (chars += (c & 0xC0) != 0x80) > max_chars) {
            free(copy);
            return workshop_fail(URSATZ_ERR_REQUEST, refusal);
        }
    }
    if (required && copy[0] == '\0') {
        free(copy);
        return workshop_fail(URSATZ_ERR_REQUEST, refusal);
    }
    *out = copy;
    return URSATZ_OK;
}
