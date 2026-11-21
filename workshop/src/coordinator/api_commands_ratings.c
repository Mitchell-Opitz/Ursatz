#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <common/json.h>

#include "coordinator/dispatch.h"
#include "items/layout.h"
#include "items/txn.h"
#include "ratings/ratings.h"
#include "settings/settings_keys.h"
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
    UrsatzStatus status = workshop_ratings_batch(session_id, (int)round, batch, cJSON_GetObjectItemCaseSensitive(call->params, "samples"), call->actor, &session, &failure);
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
    WorkshopTxn *txn = workshop_txn_begin(NULL, "ratings.keep", call->actor, "", 0);
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
