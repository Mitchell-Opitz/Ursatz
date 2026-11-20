#include "styles_routes.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <civetweb.h>

#include "jobs_routes.h"
#include "route_util.h"


static int handle_list_styles(struct mg_connection *conn) {
  return ursatz_gui_answer_query(conn, "style.list", ursatz_gui_request_params(conn, NULL), NULL, ursatz_gui_failed, false);
}

static int handle_get_style(struct mg_connection *conn, const char *id) {
  return ursatz_gui_answer_query(conn, "style.get", ursatz_gui_request_params(conn, "id", id, NULL), NULL, ursatz_gui_failed, false);
}

#define ARG(args, i) ((args)->values[i])

/* An id that cannot name a style is an unknown style. */
static int unknown_style(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) {
  if (ursatz_gui_valid_id(ARG(args, 0))) {
    return 0;
  }
  return ursatz_gui_send_failure(conn, 404, "style not found");
}

static int route_list(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { (void)args; return handle_list_styles(conn); }
static int route_get(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) {
  int refused = unknown_style(conn, args);
  return refused ? refused : handle_get_style(conn, ARG(args, 0));
}
static const UrsatzGuiRoute kRoutes[] = {
    {"GET", "/api/styles", route_list},
    {"GET", "/api/styles/<id>", route_get},
};

const UrsatzGuiRoute *ursatz_gui_styles_routes(size_t *count) {
  *count = sizeof(kRoutes) / sizeof(kRoutes[0]);
  return kRoutes;
}
