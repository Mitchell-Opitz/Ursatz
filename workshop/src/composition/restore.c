#include "composition/restore.h"

#include <stdio.h>
#include <string.h>

#include <cJSON.h>

#include "base/last_error.h"
#include "composition/compose_common.h"
#include "items/store.h"

UrsatzStatus workshop_material_item_restore(const char *id, int version, const char *note, int *out_number, UrsatzError *error) {
    cJSON *meta = NULL;
    UrsatzStatus status = workshop_material_item_get(id, &meta, NULL);
    if (status != URSATZ_OK) {
        return ursatz_error_set(error, status == URSATZ_ERR_NOT_FOUND ? status : URSATZ_ERR_INTERNAL, "/target", "item not found");
    }
    const cJSON *current_item = cJSON_GetObjectItemCaseSensitive(meta, "current_version");
    int current = cJSON_IsNumber(current_item) ? (int)current_item->valuedouble : 0;
    cJSON_Delete(meta);
    if (version < 1 || version > current) {
        return ursatz_error_set(error, URSATZ_ERR_NOT_FOUND, "/params/version", "no version %d", version);
    }
    if (version == current) {
        return ursatz_error_set(error, URSATZ_ERR_REQUEST, "/params/version", "version %d is the current one", version);
    }
    cJSON *body = NULL;
    status = workshop_material_item_get_version(id, version, &body);
    if (status != URSATZ_OK) {
        return ursatz_error_set(error, URSATZ_ERR_NOT_FOUND, "/params/version", "no version %d", version);
    }
    WorkshopComposeStage stage;
    status = workshop_compose_stage_make(id, current + 1, &stage, error);
    if (status != URSATZ_OK) {
        cJSON_Delete(body);
        return status;
    }
    char from_absolute[WORKSHOP_COMPOSE_PATH_BYTES];
    status = workshop_material_version_copy(id, version, stage.path, from_absolute, sizeof from_absolute);
    if (status != URSATZ_OK) {
        workshop_compose_stage_discard(&stage);
        cJSON_Delete(body);
        return ursatz_error_set(error, status, "/params/version", "version %d could not be copied", version);
    }
    /* the paths the body keeps of its files name the old folder: they name the new one */
    cJSON *result = cJSON_GetObjectItemCaseSensitive(body, "result");
    if (cJSON_IsObject(result) || cJSON_IsArray(result)) {
        workshop_compose_rebase(result, from_absolute, stage.named);
    }
    char default_note[32];
    snprintf(default_note, sizeof default_note, "restore v%d", version);
    cJSON *item = NULL;
    status = workshop_material_item_add_version_restored(id, body, note && note[0] ? note : default_note, stage.path, version, &item);
    cJSON_Delete(body);
    if (status != URSATZ_OK) {
        workshop_compose_stage_discard(&stage);
        const UrsatzError *last = workshop_last_error();
        if (error) {
            snprintf(error->message, sizeof error->message, "%s", last->message[0] ? last->message : "the version could not be restored");
            snprintf(error->path, sizeof error->path, "%s", last->path);
        }
        return status;
    }
    const cJSON *made = cJSON_GetObjectItemCaseSensitive(item, "current_version");
    *out_number = cJSON_IsNumber(made) ? (int)made->valuedouble : current + 1;
    cJSON_Delete(item);
    return URSATZ_OK;
}
