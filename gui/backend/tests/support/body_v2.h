#ifndef URSATZ_GUI_TEST_BODY_V2_H
#define URSATZ_GUI_TEST_BODY_V2_H

#include <stdlib.h>

#include <cJSON.h>
#include <ursatz_workshop/workshop_api.h>

#include "server_support.h"

/*
 * The route tests write the bodies of themes and pieces as the GUI wrote them until P63 B15 (a `theme` or `piece` member); the routes take
 * the stored request of format 2 (R-650). This gives the text of a request whose `body` member is such a body rewritten the way the
 * migration rewrites it (workshop_body_requests_migrate), so a test keeps naming what it composes in the words it always had. The text
 * is malloc'ed (free it); NULL for a NULL request. A request with no theme or piece body comes back as it was.
 */
static inline char *request_with_request_body(const char *json) {
    if (!json) {
        return NULL;
    }
    cJSON *root = cJSON_Parse(json);
    cJSON *body = root ? cJSON_GetObjectItemCaseSensitive(root, "body") : NULL;
    if (cJSON_IsObject(body)) {
        workshop_body_requests_migrate(body);
    }
    char *text = root ? cJSON_PrintUnformatted(root) : NULL;
    cJSON_Delete(root);
    if (!text) {
        text = malloc(strlen(json) + 1);
        if (text) {
            memcpy(text, json, strlen(json) + 1);
        }
    }
    return text;
}


/* test_gui_send and test_gui_send_finish with the params' body rewritten as request_with_request_body does. */
static inline cJSON *test_gui_send_body_v2(const char *port, const char *name, const char *target, const char *params, int *status) {
    char *converted = request_with_request_body(params);
    cJSON *answer = test_gui_send(port, name, target, converted, status);
    free(converted);
    return answer;
}

static inline cJSON *test_gui_send_finish_body_v2(const char *port, const char *name, const char *target, const char *params, int *status) {
    char *converted = request_with_request_body(params);
    cJSON *answer = test_gui_send_finish(port, name, target, converted, status);
    free(converted);
    return answer;
}

#endif
