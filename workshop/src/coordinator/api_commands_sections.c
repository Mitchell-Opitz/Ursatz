#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <common/json.h>

#include "composition/sections.h"
#include "coordinator/dispatch.h"

/* The section commands (R-666): each edits the Composer request of a piece version and saves it as the next version. The new version holds the
 * body only; item.compose composes it. */

static UrsatzStatus section_failure(UrsatzError *error, UrsatzStatus status) {
    return workshop_call_fail_service(error, status, "material store failure");
}

/* The Composer's check of the edited body, as item.validate makes it: the references resolved, then composer_api_validate of kind piece. A body that
 * cannot be composed yet (a brief whose style is missing) is kept as it is. A refusal is the command's failure, at the Composer's path. */
static UrsatzStatus check_edited(const cJSON *body, UrsatzError *error) {
    cJSON *filled = NULL;
    UrsatzStatus uses = workshop_material_body_with_uses(body, &filled);
    if (uses != URSATZ_OK) {
        cJSON_Delete(filled);
        return uses == URSATZ_ERR_ALLOC ? workshop_call_fail(error, uses, "", "out of memory")
                                        : workshop_call_fail(error, URSATZ_ERR_REQUEST, "/target", "a motif or theme the piece names cannot be used");
    }
    cJSON *request = NULL;
    bool complete = false;
    UrsatzError failure = {{0}, {0}};
    UrsatzStatus built = workshop_piece_validate(filled, &complete, &request, &failure);
    cJSON_Delete(filled);
    if (built != URSATZ_OK) {
        cJSON_Delete(request);
        return workshop_call_fail(error, built == URSATZ_ERR_ALLOC ? built : URSATZ_ERR_REQUEST, failure.path, "%s",
                                  failure.message[0] ? failure.message : "the request could not be built");
    }
    if (!complete) {
        cJSON_Delete(request);
        return URSATZ_OK;
    }
    WorkshopComposerReply composer;
    bool called = workshop_motif_validate("piece", request, &composer);
    cJSON_Delete(request);
    if (!called) {
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    UrsatzStatus status = composer.status;
    if (status != URSATZ_OK) {
        /* the Composer's path points into the request; the request is the member /request of the body */
        char path[160];
        snprintf(path, sizeof path, "/request%s", composer.path);
        workshop_call_fail(error, status, path, "%s", composer.message[0] ? composer.message : "the Composer refused the request");
    }
    workshop_composer_reply_destroy(&composer);
    return status;
}

static UrsatzStatus section_command(const WorkshopCall *call, WorkshopReply *reply, UrsatzError *error, const char *command) {
    cJSON *meta = NULL;
    cJSON *body = NULL;
    UrsatzStatus result = workshop_material_item_get(call->target, &meta, &body);
    if (result != URSATZ_OK) {
        return section_failure(error, result);
    }
    if (strcmp(common_json_string(meta, "type"), "piece") != 0) {
        cJSON_Delete(meta);
        cJSON_Delete(body);
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/target", "section commands change a piece");
    }
    cJSON_Delete(meta);
    char note[200];
    cJSON *edited = NULL;
    UrsatzStatus status = workshop_sections_edit(command, cJSON_GetObjectItemCaseSensitive(body, "request"), call->params, &edited, note, sizeof note, error);
    if (status != URSATZ_OK) {
        cJSON_Delete(body);
        return status;
    }
    /* the new version: the body with the edited request, and no result (it was made from the request before the edit) */
    cJSON_DeleteItemFromObjectCaseSensitive(body, "request");
    cJSON_DeleteItemFromObjectCaseSensitive(body, "result");
    if (!cJSON_AddItemToObject(body, "request", edited)) {
        cJSON_Delete(edited);
        cJSON_Delete(body);
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    status = check_edited(body, error);
    if (status != URSATZ_OK) {
        cJSON_Delete(body);
        return status;
    }
    cJSON *item = NULL;
    result = workshop_material_item_add_version(call->target, body, note, &item);
    cJSON *ids = workshop_sections_ids(cJSON_GetObjectItemCaseSensitive(body, "request"));
    cJSON_Delete(body);
    if (result != URSATZ_OK) {
        cJSON_Delete(ids);
        return section_failure(error, result);
    }
    cJSON *answer = workshop_reply_object(reply);
    const cJSON *current = cJSON_GetObjectItemCaseSensitive(item, "current_version");
    bool ok = answer && ids && cJSON_IsNumber(current) && cJSON_AddNumberToObject(answer, "version", current->valuedouble) &&
              cJSON_AddItemToObject(answer, "sections", ids);
    if (!ok) {
        cJSON_Delete(ids);
    }
    cJSON_Delete(item);
    return ok ? URSATZ_OK : workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
}

WORKSHOP_HANDLER(workshop_c_section_insert) {
    return section_command(call, reply, error, "section.insert");
}

WORKSHOP_HANDLER(workshop_c_section_delete) {
    return section_command(call, reply, error, "section.delete");
}

WORKSHOP_HANDLER(workshop_c_section_move) {
    return section_command(call, reply, error, "section.move");
}

WORKSHOP_HANDLER(workshop_c_section_resize) {
    return section_command(call, reply, error, "section.resize");
}

WORKSHOP_HANDLER(workshop_c_section_set) {
    return section_command(call, reply, error, "section.set");
}
