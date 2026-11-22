#include <stdbool.h>

#include "coordinator/dispatch.h"
#include "ratings/ratings.h"

/* The ratings queries (R-674, R-723): a session, the sessions newest first, the ratings of an item and its critiques and verdicts. */

WORKSHOP_HANDLER(workshop_q_ratings_session) {
    const char *id = NULL;
    if (!workshop_param_require_string(call->params, "session", &id, error)) {
        return URSATZ_ERR_REQUEST;
    }
    cJSON *session = NULL;
    UrsatzError failure = {{0}, {0}};
    UrsatzStatus status = workshop_ratings_session_read(id, &session, &failure);
    if (status != URSATZ_OK) {
        return workshop_call_fail(error, status, failure.path, "%s", failure.message);
    }
    return workshop_reply_tree(reply, session, error);
}

WORKSHOP_HANDLER(workshop_q_ratings_sessions) {
    (void)call;
    cJSON *list = NULL;
    UrsatzError failure = {{0}, {0}};
    UrsatzStatus status = workshop_ratings_sessions(&list, &failure);
    if (status != URSATZ_OK) {
        return workshop_call_fail(error, status, failure.path, "%s", failure.message);
    }
    return workshop_reply_member(reply, "sessions", list, error);
}

WORKSHOP_HANDLER(workshop_q_ratings_item) {
    const char *id = NULL;
    if (!workshop_param_require_string(call->params, "item_id", &id, error)) {
        return URSATZ_ERR_REQUEST;
    }
    cJSON *file = NULL;
    UrsatzError failure = {{0}, {0}};
    UrsatzStatus status = workshop_ratings_item(id, &file, &failure);
    if (status != URSATZ_OK) {
        return workshop_call_fail(error, status, failure.path, "%s", failure.message);
    }
    return workshop_reply_tree(reply, file, error);
}

WORKSHOP_HANDLER(workshop_q_ratings_critiques) {
    const char *id = NULL;
    if (!workshop_param_require_string(call->params, "item", &id, error)) {
        return URSATZ_ERR_REQUEST;
    }
    cJSON *file = NULL;
    UrsatzError failure = {{0}, {0}};
    UrsatzStatus status = workshop_ratings_critiques(id, &file, &failure);
    if (status != URSATZ_OK) {
        return workshop_call_fail(error, status, failure.path, "%s", failure.message);
    }
    return workshop_reply_tree(reply, file, error);
}
