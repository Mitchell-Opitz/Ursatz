#include "jobs_routes.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <civetweb.h>
#include <cJSON.h>

#include "route_util.h"


/* GET /api/jobs/<id>: {"ok": true, "job": {id, kind, state, stage, done, total, http_status?, result?}}. */
static int handle_get(struct mg_connection *conn, const char *id) {
  return ursatz_gui_answer_query(conn, "job.get", ursatz_gui_request_params(conn, "id", id, NULL), NULL, ursatz_gui_failed, false);
}

static int route_get(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_get(conn, args->values[0]); }

static const UrsatzGuiRoute kRoutes[] = {
    {"GET", "/api/jobs/<id>", route_get},
};

const UrsatzGuiRoute *ursatz_gui_jobs_routes(size_t *count) {
  *count = sizeof(kRoutes) / sizeof(kRoutes[0]);
  return kRoutes;
}
