#ifndef URSATZ_WORKSHOP_ITEMS_BODY_REQUESTS_H
#define URSATZ_WORKSHOP_ITEMS_BODY_REQUESTS_H

#include <stdbool.h>

#include <cJSON.h>

/* Internal to the workshop (R-692): the migration of a body is the data folder's own and no application calls it. */

/*
 * The migration of a theme or piece body from format 1 (a `theme` or `piece` member the services translated into a Composer request
 * on every compose) to format 2 (R-650): {uses, request, brief, result, check, seed}, where `request` is the Composer request
 * (ursatz-theme-request or ursatz-piece-request, version 2) with {"$item": {item_id, version[, file]}} in place of a motif version or a
 * stored plan and {"$pool": {item_id, pool}} in place of the saved forms of a motif (composition/request_resolve.h). The translation is
 * the one the services made until P63 B15, kept here so that a body written before the migration composes the same request after it.
 * Nothing is judged: a draft keeps what it holds. A body that is not a format 1 theme or piece body is left as it is. false on
 * allocation failure (the body is then unchanged).
 */
bool workshop_body_requests_migrate(cJSON *body);

#endif
