#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "coordinator/dispatch.h"
#include "settings/settings_keys.h"
#include "settings/settings_resolve.h"
#include "settings/settings_scopes.h"

/* The settings queries (R-671, R-676): what a request resolves to and where each setting comes from, one scope file, the scope files that exist,
 * and the keys and rules the workshop knows. The commands that change a scope are in api_commands_settings.c. */

/* A string param that must be a string when present. */
static bool optional_text(const WorkshopCall *call, const char *name, const char **out, UrsatzError *error) {
    bool bad = false;
    *out = workshop_param_string(call->params, name, &bad);
    if (bad) {
        char path[64];
        snprintf(path, sizeof path, "/params/%s", name);
        workshop_call_fail(error, URSATZ_ERR_REQUEST, path, "%s is a text", name);
        return false;
    }
    return true;
}

/* settings.resolve: params project_id, item_id, style_id, taste, settings (a preferences document), overrides, off, all. */
WORKSHOP_HANDLER(workshop_q_settings_resolve) {
    WorkshopSettingsRequest request;
    memset(&request, 0, sizeof(request));
    const char *kind = NULL;
    if (!optional_text(call, "project_id", &request.project_id, error) || !optional_text(call, "item_id", &request.item_id, error) ||
        !optional_text(call, "style_id", &request.style_id, error) || !optional_text(call, "taste", &request.taste, error) ||
        !optional_text(call, "kind", &kind, error)) {
        return URSATZ_ERR_REQUEST;
    }
    request.values = cJSON_GetObjectItemCaseSensitive(call->params, "settings");
    request.overrides = cJSON_GetObjectItemCaseSensitive(call->params, "overrides");
    request.off = cJSON_GetObjectItemCaseSensitive(call->params, "off");
    request.kind = kind ? kind : "theme";
    if (strcmp(request.kind, "theme") != 0 && strcmp(request.kind, "motif") != 0) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/kind", "kind is theme or motif");
    }
    const cJSON *explore = cJSON_GetObjectItemCaseSensitive(call->params, "explore");
    if (explore && !cJSON_IsNull(explore)) {
        const cJSON *seed = cJSON_GetObjectItemCaseSensitive(explore, "seed");
        const cJSON *distance = cJSON_GetObjectItemCaseSensitive(explore, "distance");
        if (!cJSON_IsObject(explore) || !cJSON_IsNumber(seed) || seed->valuedouble < 0 || seed->valuedouble > 4294967295.0 ||
            (distance && !cJSON_IsNull(distance) && (!cJSON_IsNumber(distance) || distance->valuedouble < 1 || distance->valuedouble > 8))) {
            return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/explore", "explore is {seed: 0 to 4294967295, distance: 1 to 8}");
        }
        request.explore = true;
        request.explore_seed = (unsigned long long)seed->valuedouble;
        request.explore_distance = cJSON_IsNumber(distance) ? distance->valueint : 0;
    }
    const cJSON *all = cJSON_GetObjectItemCaseSensitive(call->params, "all");
    if (all && !cJSON_IsNull(all) && !cJSON_IsBool(all)) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/all", "all is true or false");
    }
    cJSON *answer = NULL;
    UrsatzError failure = {{0}, {0}};
    UrsatzStatus status = workshop_settings_resolve_request(&request, cJSON_IsTrue(all), &answer, &failure);
    if (status != URSATZ_OK) {
        return workshop_call_fail(error, status, failure.path, "%s", failure.message);
    }
    return workshop_reply_tree(reply, answer, error);
}

/* settings.scope: param scope: the file's content without its format members, or an empty scope. */
WORKSHOP_HANDLER(workshop_q_settings_scope) {
    const char *scope = NULL;
    if (!workshop_param_require_string(call->params, "scope", &scope, error)) {
        return URSATZ_ERR_REQUEST;
    }
    cJSON *file = NULL;
    UrsatzError failure = {{0}, {0}};
    UrsatzStatus status = workshop_scope_read(scope, &file, &failure);
    if (status != URSATZ_OK) {
        return workshop_call_fail(error, status, failure.path, "%s", failure.message);
    }
    return workshop_reply_tree(reply, file, error);
}

/* settings.list: {tastes, styles, projects, items}. */
WORKSHOP_HANDLER(workshop_q_settings_list) {
    (void)call;
    cJSON *lists = NULL;
    UrsatzError failure = {{0}, {0}};
    UrsatzStatus status = workshop_scope_list(&lists, &failure);
    if (status != URSATZ_OK) {
        return workshop_call_fail(error, status, failure.path, "%s", failure.message);
    }
    return workshop_reply_tree(reply, lists, error);
}

/* settings.keys: {keys, rules}, the tables installed at open. */
WORKSHOP_HANDLER(workshop_q_settings_keys) {
    (void)call;
    cJSON *tables = workshop_settings_keys_json();
    if (!tables) {
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    return workshop_reply_tree(reply, tables, error);
}
