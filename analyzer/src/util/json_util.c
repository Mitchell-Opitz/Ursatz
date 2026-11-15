#include "util/json_util.h"

const cJSON *json_member(const cJSON *object, const char *name) {
    return cJSON_IsObject(object) ? cJSON_GetObjectItemCaseSensitive(object, name) : NULL;
}

const char *json_string_member(const cJSON *object, const char *name) {
    const cJSON *item = json_member(object, name);
    return cJSON_IsString(item) ? item->valuestring : NULL;
}

bool json_number_member(const cJSON *object, const char *name, long *out) {
    const cJSON *item = json_member(object, name);
    if (!cJSON_IsNumber(item)) {
        return false;
    }
    *out = (long)item->valuedouble;
    return true;
}

bool json_add(cJSON *object, const char *key, cJSON *item) {
    if (!item) {
        return false;
    }
    if (!cJSON_AddItemToObject(object, key, item)) {
        cJSON_Delete(item);
        return false;
    }
    return true;
}

bool json_append(cJSON *array, cJSON *item) {
    if (!item) {
        return false;
    }
    if (!cJSON_AddItemToArray(array, item)) {
        cJSON_Delete(item);
        return false;
    }
    return true;
}
