#include "links_routes.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <civetweb.h>

#include "route_util.h"


/* --- scope ---------------------------------------------------------------- */

/* --- handlers ------------------------------------------------------------- */

static int handle_start_run(struct mg_connection *conn) {
  cJSON *body = ursatz_gui_read_json_body(conn, NULL);
  if (!body) {
    return ursatz_gui_send_failure(conn, 400, "invalid request body");
  }
  /* 202 {"ok", "run", "job"}: the page follows the job (BackendJobs.runJob) and gets {"ok", "run"} when it ends. */
  return ursatz_gui_answer_command(conn, "links.run", "", body, NULL, ursatz_gui_failed, false);
}

static int handle_list_runs(struct mg_connection *conn) {
  return ursatz_gui_answer_query(conn, "links.list", ursatz_gui_request_params(conn, NULL), NULL, ursatz_gui_failed, false);
}

static int handle_get_run(struct mg_connection *conn, const char *id) {
  return ursatz_gui_answer_query(conn, "links.get", ursatz_gui_request_params(conn, "id", id, NULL), NULL, ursatz_gui_failed, false);
}

static int handle_delete_run(struct mg_connection *conn, const char *id) {
  return ursatz_gui_answer_command(conn, "links.delete", id, NULL, NULL, ursatz_gui_failed, false);
}

static int handle_piece_links(struct mg_connection *conn, const char *piece_id) {
  return ursatz_gui_answer_query(conn, "links.piece", ursatz_gui_request_params(conn, "id", piece_id, NULL), NULL, ursatz_gui_failed,
                                 false);
}

#define ARG(args, i) ((args)->values[i])

/* An id that names no run or piece is not found. */
static int unknown(struct mg_connection *conn, const UrsatzGuiRouteArgs *args, const char *what) {
  return ursatz_gui_valid_id(ARG(args, 0)) ? 0 : ursatz_gui_send_failure(conn, 404, what);
}

static int route_start(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { (void)args; return handle_start_run(conn); }
static int route_list(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { (void)args; return handle_list_runs(conn); }
static int route_get(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) {
  int refused = unknown(conn, args, "run not found");
  return refused ? refused : handle_get_run(conn, ARG(args, 0));
}
static int route_delete(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) {
  int refused = unknown(conn, args, "run not found");
  return refused ? refused : handle_delete_run(conn, ARG(args, 0));
}
static int route_piece(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) {
  int refused = unknown(conn, args, "piece not found");
  return refused ? refused : handle_piece_links(conn, ARG(args, 0));
}

static const UrsatzGuiRoute kRoutes[] = {
    {"POST", "/api/links/runs", route_start},
    {"GET", "/api/links/runs", route_list},
    {"GET", "/api/links/runs/<id>", route_get},
    {"DELETE", "/api/links/runs/<id>", route_delete},
    {"GET", "/api/links/pieces/<id>", route_piece},
};

const UrsatzGuiRoute *ursatz_gui_links_routes(size_t *count) {
  *count = sizeof(kRoutes) / sizeof(kRoutes[0]);
  return kRoutes;
}
