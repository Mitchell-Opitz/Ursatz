#include "util/json_util.h"

#include "util/fs_util.h"

const char *ursatz_gui_json_string(const cJSON *object, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsString(item) && item->valuestring ? item->valuestring : "";
}

char *ursatz_gui_json_string_copy(const cJSON *object, const char *name) {
    return ursatz_gui_dup_string(ursatz_gui_json_string(object, name));
}

bool ursatz_gui_json_set_string(cJSON *object, const char *name, const char *value) {
    cJSON_DeleteItemFromObjectCaseSensitive(object, name);
    return cJSON_AddStringToObject(object, name, value) != NULL;
}

bool ursatz_gui_json_attach(cJSON *parent, const char *name, cJSON *child) {
    if (child && cJSON_AddItemToObject(parent, name, child)) {
        return true;
    }
    cJSON_Delete(child);
    return false;
}

bool ursatz_gui_json_add_copy_or_null(cJSON *dst, const cJSON *src, const char *name) {
    const cJSON *member = cJSON_GetObjectItemCaseSensitive(src, name);
    return ursatz_gui_json_attach(dst, name, member ? cJSON_Duplicate(member, 1) : cJSON_CreateNull());
}

bool ursatz_gui_json_add_copy_if(cJSON *dst, const cJSON *src, const char *name) {
    const cJSON *member = cJSON_GetObjectItemCaseSensitive(src, name);
    return !member || cJSON_IsNull(member) || ursatz_gui_json_attach(dst, name, cJSON_Duplicate(member, 1));
}
