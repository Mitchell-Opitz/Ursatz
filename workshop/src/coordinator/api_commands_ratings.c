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
