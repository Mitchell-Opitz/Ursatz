#include "styles_routes.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <civetweb.h>

#include "jobs_routes.h"
#include "route_util.h"


/* The failures of a style command: the status of the table, no queue a 503. */
static int failed_style_command(struct mg_connection *conn, UrsatzStatus status, const UrsatzError *error) {
  return ursatz_gui_send_failure(conn, status == URSATZ_ERR_INTERNAL && strstr(error->message, "queue") ? 503 : ursatz_gui_http_status(status, NULL),
                                 error->message);
}

/* POST /api/styles: a "style" job (R-645; a second build waits): the answer is 202 with the job, which answers what this route used to. The
 * request is read here, so a bad one is refused at once. */
static int handle_build_style(struct mg_connection *conn) {
  cJSON *body = ursatz_gui_read_json_body(conn, NULL);
  if (!body) {
    return ursatz_gui_send_failure(conn, 400, "invalid request body");
  }
  return ursatz_gui_answer_command(conn, "style.build", "", body, NULL, failed_style_command, false);
}

static int handle_list_styles(struct mg_connection *conn) {
  return ursatz_gui_answer_query(conn, "style.list", ursatz_gui_request_params(conn, NULL), NULL, ursatz_gui_failed_message, false);
}

static int handle_get_style(struct mg_connection *conn, const char *id) {
  return ursatz_gui_answer_query(conn, "style.get", ursatz_gui_request_params(conn, "id", id, NULL), NULL, ursatz_gui_failed_message, false);
}

static int handle_rebuild_style(struct mg_connection *conn, const char *id) {
  return ursatz_gui_answer_command(conn, "style.rebuild", id, NULL, NULL, failed_style_command, false);
}

static int handle_delete_style(struct mg_connection *conn, const char *id) {
  return ursatz_gui_answer_command(conn, "style.delete", id, NULL, NULL, failed_style_command, false);
}

#define ARG(args, i) ((args)->values[i])

/* An id that cannot name a style is an unknown style. */
static int unknown_style(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) {
  if (ursatz_gui_valid_id(ARG(args, 0))) {
    return 0;
  }
  return ursatz_gui_send_failure(conn, 404, "style not found");
}

static int route_build(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { (void)args; return handle_build_style(conn); }
static int route_list(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { (void)args; return handle_list_styles(conn); }
static int route_get(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) {
  int refused = unknown_style(conn, args);
  return refused ? refused : handle_get_style(conn, ARG(args, 0));
}
static int route_delete(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) {
  int refused = unknown_style(conn, args);
  return refused ? refused : handle_delete_style(conn, ARG(args, 0));
}
static int route_rebuild(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) {
  int refused = unknown_style(conn, args);
  return refused ? refused : handle_rebuild_style(conn, ARG(args, 0));
}

static const UrsatzGuiRoute kRoutes[] = {
    {"POST", "/api/styles", route_build},
    {"GET", "/api/styles", route_list},
    {"GET", "/api/styles/<id>", route_get},
    {"DELETE", "/api/styles/<id>", route_delete},
    {"POST", "/api/styles/<id>/rebuild", route_rebuild},
};

const UrsatzGuiRoute *ursatz_gui_styles_routes(size_t *count) {
  *count = sizeof(kRoutes) / sizeof(kRoutes[0]);
  return kRoutes;
}
