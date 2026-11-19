#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "coordinator/dispatch.h"

/* The style and motive link queries (R-646). */

static const cJSON *require_id(const WorkshopCall *call, const char **id, UrsatzError *error) {
    return workshop_param_require_string(call->params, "id", id, error) ? call->params : NULL;
}

WORKSHOP_HANDLER(workshop_q_style_list) {
    (void)call;
    cJSON *list = NULL;
    UrsatzStatus result = workshop_style_list(&list);
    if (result != URSATZ_OK) {
        return workshop_call_fail_service(error, result, "style operation failed");
    }
    return workshop_reply_member(reply, "styles", list, error);
}

WORKSHOP_HANDLER(workshop_q_style_get) {
    const char *id = NULL;
    if (!require_id(call, &id, error)) {
        return URSATZ_ERR_REQUEST;
    }
    /* An id that cannot name a style is an unknown style. */
    if (!workshop_collection_is_valid_id(id)) {
        return workshop_call_fail(error, URSATZ_ERR_NOT_FOUND, "", "style not found");
    }
    cJSON *style = NULL;
    UrsatzStatus result = workshop_style_get(id, &style);
    if (result != URSATZ_OK) {
        return workshop_call_fail_service(error, result, "style operation failed");
    }
    return workshop_reply_member(reply, "style", style, error);
}

WORKSHOP_HANDLER(workshop_q_links_list) {
    (void)call;
    cJSON *runs = NULL;
    UrsatzStatus result = workshop_link_run_list(&runs);
    if (result != URSATZ_OK) {
        return workshop_call_fail_service(error, result, "link operation failed");
    }
    return workshop_reply_member(reply, "runs", runs, error);
}

WORKSHOP_HANDLER(workshop_q_links_get) {
    const char *id = NULL;
    if (!require_id(call, &id, error)) {
        return URSATZ_ERR_REQUEST;
    }
    if (!workshop_collection_is_valid_id(id)) {
        return workshop_call_fail(error, URSATZ_ERR_NOT_FOUND, "", "run not found");
    }
    cJSON *run = NULL;
    UrsatzStatus result = workshop_link_run_get(id, &run);
    if (result != URSATZ_OK) {
        return result == URSATZ_ERR_NOT_FOUND ? workshop_call_fail(error, result, "", "run not found")
                                              : workshop_call_fail_service(error, result, "link operation failed");
    }
    if (!workshop_link_titles_for_run(run)) {
        cJSON_Delete(run);
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    return workshop_reply_member(reply, "run", run, error);
}

/* The motive links of one piece: params id is the piece. */
WORKSHOP_HANDLER(workshop_q_links_piece) {
    const char *id = NULL;
    if (!require_id(call, &id, error)) {
        return URSATZ_ERR_REQUEST;
    }
    if (!workshop_collection_is_valid_id(id)) {
        return workshop_call_fail(error, URSATZ_ERR_NOT_FOUND, "", "piece not found");
    }
    cJSON *links = NULL;
    UrsatzStatus result = workshop_link_piece_links(id, &links);
    if (result != URSATZ_OK) {
        return workshop_call_fail_service(error, result, "link operation failed");
    }
    if (!workshop_link_titles_for_piece_links(links)) {
        cJSON_Delete(links);
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    return workshop_reply_member(reply, "links", links, error);
}
