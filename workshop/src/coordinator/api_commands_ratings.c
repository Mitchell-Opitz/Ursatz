#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <common/json.h>

#include "composition/composer_tool.h"
#include "coordinator/dispatch.h"
#include "items/store.h"
#include "items/layout.h"
#include "items/txn.h"
#include "ratings/ratings.h"
#include "settings/settings_keys.h"
#include "settings/settings_resolve.h"
#include "settings/settings_scopes.h"

/* The ratings commands (R-674): listening sessions with their batches, ratings and picks, ratings of item versions, and keep as default, which sets
 * a taste profile's value only with the ratings that say why. The service is ratings/ratings.c. */

static UrsatzStatus failed(UrsatzError *error, UrsatzStatus status, const UrsatzError *failure) {
    return workshop_call_fail(error, status, failure->path, "%s", failure->message);
}

/* A whole number param; false (error filled at its path) when it is absent or not one. */
static bool whole_param(const cJSON *params, const char *name, long long *out, UrsatzError *error) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(params, name);
    if (!cJSON_IsNumber(item) || item->valuedouble != (double)(long long)item->valuedouble) {
        char path[64];
        snprintf(path, sizeof path, "/params/%s", name);
        workshop_call_fail(error, URSATZ_ERR_REQUEST, path, "%s is a whole number", name);
        return false;
    }
    *out = (long long)item->valuedouble;
    return true;
}

static const char *optional_text(const cJSON *params, const char *name) {
    bool bad = false;
    return workshop_param_string(params, name, &bad);
}

WORKSHOP_HANDLER(workshop_c_ratings_session) {
    bool bad = false;
    const char *what = workshop_param_string(call->params, "what", &bad);
    if (!what || (strcmp(what, "motifs") != 0 && strcmp(what, "themes") != 0)) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/what", "what is motifs or themes");
    }
    cJSON *session = NULL;
    UrsatzError failure = {{0}, {0}};
    UrsatzStatus status = workshop_ratings_session_create(what, optional_text(call->params, "taste"), optional_text(call->params, "style"),
                                                          optional_text(call->params, "note"), call->actor, &session, &failure);
    if (status != URSATZ_OK) {
        return failed(error, status, &failure);
    }
    return workshop_reply_member(reply, "session", session, error);
}

WORKSHOP_HANDLER(workshop_c_ratings_batch) {
    const char *session_id = NULL;
    const char *batch = NULL;
    long long round = 0;
    if (!workshop_param_require_string(call->params, "session", &session_id, error) || !whole_param(call->params, "round", &round, error) ||
        !workshop_param_require_string(call->params, "batch", &batch, error)) {
        return URSATZ_ERR_REQUEST;
    }
    cJSON *session = NULL;
    UrsatzError failure = {{0}, {0}};
    UrsatzStatus status = workshop_ratings_batch(session_id, (int)round, batch, cJSON_GetObjectItemCaseSensitive(call->params, "samples"), &session, &failure);
    if (status != URSATZ_OK) {
        return failed(error, status, &failure);
    }
    return workshop_reply_member(reply, "session", session, error);
}

WORKSHOP_HANDLER(workshop_c_ratings_rate) {
    const char *session_id = NULL;
    const char *batch = NULL;
    long long round = 0;
    long long seed = 0;
    long long rating = 0;
    if (!workshop_param_require_string(call->params, "session", &session_id, error) || !whole_param(call->params, "round", &round, error) ||
        !workshop_param_require_string(call->params, "batch", &batch, error) || !whole_param(call->params, "seed", &seed, error) ||
        !whole_param(call->params, "rating", &rating, error)) {
        return URSATZ_ERR_REQUEST;
    }
    char *id = NULL;
    UrsatzError failure = {{0}, {0}};
    UrsatzStatus status = workshop_ratings_rate(session_id, (int)round, batch, seed, (int)rating, optional_text(call->params, "note"), call->actor, &id, &failure);
    if (status != URSATZ_OK) {
        return failed(error, status, &failure);
    }
    cJSON *answer = workshop_reply_object(reply);
    bool ok = answer && cJSON_AddStringToObject(answer, "id", id);
    free(id);
    return ok ? URSATZ_OK : workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
}

WORKSHOP_HANDLER(workshop_c_ratings_pick) {
    const char *session_id = NULL;
    const char *better = NULL;
    long long round = 0;
    long long seed = 0;
    if (!workshop_param_require_string(call->params, "session", &session_id, error) || !whole_param(call->params, "round", &round, error) ||
        !whole_param(call->params, "seed", &seed, error) || !workshop_param_require_string(call->params, "better", &better, error)) {
        return URSATZ_ERR_REQUEST;
    }
    char *id = NULL;
    UrsatzError failure = {{0}, {0}};
    UrsatzStatus status = workshop_ratings_pick(session_id, (int)round, seed, better, optional_text(call->params, "note"), call->actor, &id, &failure);
    if (status != URSATZ_OK) {
        return failed(error, status, &failure);
    }
    cJSON *answer = workshop_reply_object(reply);
    bool ok = answer && cJSON_AddStringToObject(answer, "id", id);
    free(id);
    return ok ? URSATZ_OK : workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
}

WORKSHOP_HANDLER(workshop_c_ratings_rate_version) {
    long long version = 0;
    long long rating = 0;
    if (!whole_param(call->params, "version", &version, error) || !whole_param(call->params, "rating", &rating, error)) {
        return URSATZ_ERR_REQUEST;
    }
    char *id = NULL;
    UrsatzError failure = {{0}, {0}};
    UrsatzStatus status = workshop_ratings_rate_version(call->target, (int)version, (int)rating, optional_text(call->params, "note"), call->actor, &id, &failure);
    if (status != URSATZ_OK) {
        return failed(error, status, &failure);
    }
    cJSON *answer = workshop_reply_object(reply);
    bool ok = answer && cJSON_AddStringToObject(answer, "id", id);
    free(id);
    return ok ? URSATZ_OK : workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
}

/* ratings.critique (R-723): the critic's judgment of a version of a motif or a theme, kept with the ids of its observations. */

/* The candidate of a version for the critique entry: a theme's plan.json, or a motif's motive document (the Composer's analysis of its body). */
static UrsatzStatus candidate_of(const char *item, const cJSON *meta, int version, cJSON **out, UrsatzError *error) {
    const char *type = common_json_string(meta, "type");
    *out = NULL;
    if (strcmp(type, "theme") == 0) {
        unsigned char *data = NULL;
        size_t size = 0;
        if (workshop_material_version_file(item, version, "plan.json", &data, &size) != URSATZ_OK) {
            return workshop_call_fail(error, URSATZ_ERR_NOT_FOUND, "/params/version", "version %d of %s has no plan", version, item);
        }
        char *text = malloc(size + 1);
        if (!text) {
            free(data);
            return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
        }
        memcpy(text, data, size);
        text[size] = '\0';
        free(data);
        cJSON *plan = cJSON_Parse(text);
        free(text);
        cJSON *candidate = plan ? cJSON_CreateObject() : NULL;
        if (!candidate || !cJSON_AddItemToObject(candidate, "plan", plan)) {
            cJSON_Delete(candidate);
            cJSON_Delete(plan);
            return workshop_call_fail(error, URSATZ_ERR_INTERNAL, "", "the plan of version %d of %s is not readable", version, item);
        }
        *out = candidate;
        return URSATZ_OK;
    }
    if (strcmp(type, "motif") == 0) {
        cJSON *body = NULL;
        if (workshop_material_item_get_version(item, version, &body) != URSATZ_OK) {
            return workshop_call_fail(error, URSATZ_ERR_NOT_FOUND, "/params/version", "item %s has no version %d", item, version);
        }
        const cJSON *motive = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(body, "motif"), "analysis"), "motive");
        cJSON *candidate = cJSON_IsObject(motive) ? cJSON_CreateObject() : NULL;
        if (!candidate || !cJSON_AddItemToObject(candidate, "motive", cJSON_Duplicate(motive, true))) {
            cJSON_Delete(candidate);
            cJSON_Delete(body);
            return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/target", "version %d of %s has no analyzed motive to judge", version, item);
        }
        cJSON_Delete(body);
        *out = candidate;
        return URSATZ_OK;
    }
    return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/target", "only a motif or a theme can be judged, not a %s", type);
}

/* The preferences document of the critique: the settings param when it is an object, else the item's resolved settings (NULL: the defaults). */
static UrsatzStatus preferences_of(const WorkshopCall *call, const cJSON *meta, const char *item, cJSON **out, UrsatzError *error) {
    const cJSON *settings = cJSON_GetObjectItemCaseSensitive(call->params, "settings");
    *out = NULL;
    if (settings && !cJSON_IsNull(settings)) {
        if (!cJSON_IsObject(settings)) {
            return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/settings", "settings is a preferences document");
        }
        *out = cJSON_Duplicate(settings, true);
        return *out ? URSATZ_OK : workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    if (workshop_settings_key_count() == 0) {
        return URSATZ_OK;
    }
    WorkshopSettingsRequest resolve;
    memset(&resolve, 0, sizeof(resolve));
    resolve.project_id = common_json_string(meta, "project_id");
    resolve.item_id = item;
    resolve.kind = strcmp(common_json_string(meta, "type"), "motif") == 0 ? "motif" : "theme";
    cJSON *answer = NULL;
    UrsatzError failure = {{0}, {0}};
    UrsatzStatus status = workshop_settings_resolve_request(&resolve, false, &answer, &failure);
    if (status != URSATZ_OK) {
        return failed(error, status, &failure);
    }
    const cJSON *document = cJSON_GetObjectItemCaseSensitive(answer, "document");
    *out = cJSON_IsObject(document) ? cJSON_Duplicate(document, true) : NULL;
    cJSON_Delete(answer);
    return cJSON_IsObject(document) && !*out ? workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory") : URSATZ_OK;
}

WORKSHOP_HANDLER(workshop_c_ratings_critique) {
    long long version = 0;
    if (!whole_param(call->params, "version", &version, error)) {
        return URSATZ_ERR_REQUEST;
    }
    cJSON *meta = NULL;
    if (workshop_material_item_get(call->target, &meta, NULL) != URSATZ_OK) {
        return workshop_call_fail(error, URSATZ_ERR_NOT_FOUND, "/target", "there is no item '%s'", call->target);
    }
    const cJSON *current = cJSON_GetObjectItemCaseSensitive(meta, "current_version");
    if (version < 1 || !cJSON_IsNumber(current) || version > current->valueint) {
        cJSON_Delete(meta);
        return workshop_call_fail(error, URSATZ_ERR_NOT_FOUND, "/params/version", "item %s has no version %lld", call->target, version);
    }
    cJSON *candidate = NULL;
    cJSON *preferences = NULL;
    UrsatzStatus status = candidate_of(call->target, meta, (int)version, &candidate, error);
    if (status == URSATZ_OK) {
        status = preferences_of(call, meta, call->target, &preferences, error);
    }
    cJSON_Delete(meta);
    cJSON *request = status == URSATZ_OK ? cJSON_CreateObject() : NULL;
    cJSON *candidates = request ? cJSON_AddArrayToObject(request, "candidates") : NULL;
    if (status == URSATZ_OK &&
        !(candidates && cJSON_AddStringToObject(request, "schema", "ursatz-critique-request") && cJSON_AddNumberToObject(request, "schema_version", 1))) {
        status = workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    if (status == URSATZ_OK) {
        /* each is moved into the request, or freed with it */
        bool added = cJSON_AddItemToArray(candidates, candidate);
        candidate = added ? NULL : candidate;
        if (added && preferences && cJSON_AddItemToObject(request, "preferences", preferences)) {
            preferences = NULL;
        }
        if (!added) {
            status = workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
        }
    }
    cJSON_Delete(candidate);
    cJSON_Delete(preferences);
    if (status != URSATZ_OK) {
        cJSON_Delete(request);
        return status;
    }
    ComposerResponse response;
    memset(&response, 0, sizeof(response));
    UrsatzError refused = {{0}, {0}};
    status = workshop_composer_tool("critique", request, NULL, NULL, 0, &response, &refused);
    cJSON_Delete(request);
    if (status != URSATZ_OK) {
        composer_response_destroy(&response);
        return failed(error, status, &refused);
    }
    cJSON *answer = cJSON_Parse(response.json);
    composer_response_destroy(&response);
    const cJSON *first = answer ? cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(answer, "candidates"), 0) : NULL;
    cJSON *record = NULL;
    UrsatzError failure = {{0}, {0}};
    status = first ? workshop_ratings_critique_add(call->target, (int)version, first, optional_text(call->params, "note"), call->actor, &record, &failure)
                   : workshop_call_fail(&failure, URSATZ_ERR_INTERNAL, "", "the critic gave no answer");
    cJSON_Delete(answer);
    if (status != URSATZ_OK) {
        return failed(error, status, &failure);
    }
    /* The critique as recorded: its id, score and observations with their ids. */
    cJSON *shown = workshop_reply_object(reply);
    bool shown_ok = shown && cJSON_AddItemToObject(shown, "id", cJSON_DetachItemFromObjectCaseSensitive(record, "id")) &&
                    cJSON_AddItemToObject(shown, "version", cJSON_DetachItemFromObjectCaseSensitive(record, "version")) &&
                    cJSON_AddItemToObject(shown, "score", cJSON_DetachItemFromObjectCaseSensitive(record, "score")) &&
                    cJSON_AddItemToObject(shown, "observations", cJSON_DetachItemFromObjectCaseSensitive(record, "observations"));
    cJSON_Delete(record);
    return shown_ok ? URSATZ_OK : workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
}

WORKSHOP_HANDLER(workshop_c_ratings_verdict) {
    const char *observation = NULL;
    const char *verdict = NULL;
    if (!workshop_param_require_string(call->params, "observation", &observation, error) ||
        !workshop_param_require_string(call->params, "verdict", &verdict, error)) {
        return URSATZ_ERR_REQUEST;
    }
    cJSON *meta = NULL;
    if (workshop_material_item_get(call->target, &meta, NULL) != URSATZ_OK) {
        return workshop_call_fail(error, URSATZ_ERR_NOT_FOUND, "/target", "there is no item '%s'", call->target);
    }
    cJSON_Delete(meta);
    char *id = NULL;
    UrsatzError failure = {{0}, {0}};
    UrsatzStatus status = workshop_ratings_verdict(call->target, observation, verdict, optional_text(call->params, "note"), call->actor, &id, &failure);
    if (status != URSATZ_OK) {
        return failed(error, status, &failure);
    }
    cJSON *answer = workshop_reply_object(reply);
    bool ok = answer && cJSON_AddStringToObject(answer, "id", id);
    free(id);
    return ok ? URSATZ_OK : workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
}

/* ratings.keep: a taste profile's value, with the ratings and picks that say why (R-674). */
WORKSHOP_HANDLER(workshop_c_ratings_keep) {
    const char *taste = NULL;
    const char *key = NULL;
    if (!workshop_param_require_string(call->params, "taste", &taste, error) || !workshop_param_require_string(call->params, "key", &key, error)) {
        return URSATZ_ERR_REQUEST;
    }
    const cJSON *ratings = cJSON_GetObjectItemCaseSensitive(call->params, "ratings");
    if (!cJSON_IsArray(ratings) || cJSON_GetArraySize(ratings) < 1) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/ratings", "keep as default cites the ratings behind it");
    }
    int index = 0;
    const cJSON *cited;
    cJSON_ArrayForEach(cited, ratings) {
        char path[64];
        snprintf(path, sizeof path, "/params/ratings/%d", index++);
        if (!cJSON_IsString(cited) || !workshop_ratings_id_recorded(cited->valuestring)) {
            return workshop_call_fail(error, URSATZ_ERR_REQUEST, path, "%s is not a recorded rating or pick", cJSON_IsString(cited) ? cited->valuestring : "(not an id)");
        }
    }
    if (!workshop_layout_id_ok(taste)) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/taste", "a taste profile's name is 1 to 64 letters, digits, hyphens and underscores");
    }
    if (!workshop_settings_key_find(key)) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/key", "unknown setting '%s'", key);
    }
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(call->params, "value");
    bool drop = !value || cJSON_IsNull(value);
    if (!drop) {
        UrsatzStatus checked = workshop_settings_check_value(key, value, "/params/value", false, error);
        if (checked != URSATZ_OK) {
            return checked;
        }
    }
    char scope[96];
    snprintf(scope, sizeof scope, "taste:%s", taste);
    cJSON *file = NULL;
    UrsatzError failure = {{0}, {0}};
    UrsatzStatus status = workshop_scope_read(scope, &file, &failure);
    if (status != URSATZ_OK) {
        return failed(error, status, &failure);
    }
    cJSON *values = cJSON_GetObjectItemCaseSensitive(file, "values");
    if (!cJSON_IsObject(values)) {
        cJSON_DeleteItemFromObjectCaseSensitive(file, "values");
        values = cJSON_AddObjectToObject(file, "values");
    }
    cJSON *copy = drop ? NULL : cJSON_Duplicate(value, true);
    if (!values || (!drop && !copy)) {
        cJSON_Delete(copy);
        cJSON_Delete(file);
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    if (drop) {
        cJSON_DeleteItemFromObjectCaseSensitive(values, key);
    } else if (cJSON_GetObjectItemCaseSensitive(values, key)) {
        cJSON_ReplaceItemInObjectCaseSensitive(values, key, copy);
    } else {
        cJSON_AddItemToObject(values, key, copy);
    }
    status = workshop_settings_keep_record(file, key, drop ? NULL : value, ratings, optional_text(call->params, "note"), call->actor, error);
    if (status != URSATZ_OK) {
        cJSON_Delete(file);
        return status;
    }
    WorkshopTxn *txn = workshop_txn_begin(NULL, "ratings.keep", "", 0);
    if (!txn) {
        cJSON_Delete(file);
        return workshop_call_fail(error, URSATZ_ERR_IO, "", "the journal cannot be read");
    }
    status = workshop_scope_stage(txn, scope, file, &failure);
    if (status != URSATZ_OK) {
        workshop_txn_abort(txn);
        cJSON_Delete(file);
        return failed(error, status, &failure);
    }
    status = workshop_txn_commit(txn, &failure);
    if (status != URSATZ_OK) {
        cJSON_Delete(file);
        return workshop_call_fail(error, status, failure.path, "%s", failure.message[0] ? failure.message : "the taste profile cannot be saved");
    }
    return workshop_reply_tree(reply, file, error);
}
