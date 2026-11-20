#include "jobs_routes.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <civetweb.h>
#include <cJSON.h>

#include "route_util.h"
#include "../server.h"


static int send_error(struct mg_connection *conn, int status, const char *error) {
  return ursatz_gui_send_failure(conn, status, error);
}
/* GET /api/jobs/<id>: {"ok": true, "job": {id, kind, state, stage, done, total, http_status?, result?}}. */
static int handle_get(struct mg_connection *conn, const char *id) {
  return ursatz_gui_answer_query(conn, "job.get", ursatz_gui_request_params(conn, "id", id, NULL), NULL, ursatz_gui_failed, false);
}

/* DELETE /api/jobs/<id>: asks the job to stop (204). */
static int handle_delete(struct mg_connection *conn, const char *id) {
  WorkshopReply reply;
  UrsatzError error = {{0}, {0}};
  cJSON *command = cJSON_CreateObject();
  if (!command || !cJSON_AddStringToObject(command, "command", "job.cancel") || !cJSON_AddStringToObject(command, "actor", "michelle") ||
      !cJSON_AddStringToObject(command, "target", id)) {
    cJSON_Delete(command);
    return send_error(conn, 500, "out of memory");
  }
  UrsatzStatus status = workshop_command(ursatz_gui_workshop(), command, NULL, &reply, &error);
  cJSON_Delete(command);
  workshop_reply_destroy(&reply);
  if (status != URSATZ_OK) {
    return send_error(conn, ursatz_gui_http_status(status, NULL), error.message);
  }
  mg_printf(conn, "HTTP/1.1 204 No Content\r\nConnection: close\r\n\r\n");
  return 204;
}

static int route_get(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_get(conn, args->values[0]); }
static int route_delete(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_delete(conn, args->values[0]); }

static const UrsatzGuiRoute kRoutes[] = {
    {"GET", "/api/jobs/<id>", route_get},
    {"DELETE", "/api/jobs/<id>", route_delete},
};

const UrsatzGuiRoute *ursatz_gui_jobs_routes(size_t *count) {
  *count = sizeof(kRoutes) / sizeof(kRoutes[0]);
  return kRoutes;
}
