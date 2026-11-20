#include "collections_routes.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <civetweb.h>

#include "route_util.h"


/* A missing collection is a 404; every other failure keeps the route's
 * 200 with {"ok":false,...}. */

static int handle_list_corpora(struct mg_connection *conn) {
  return ursatz_gui_answer_query(conn, "collection.list", ursatz_gui_request_params(conn, NULL), NULL, ursatz_gui_failed, false);
}

static int handle_get_corpus(struct mg_connection *conn, const char *id) {
  return ursatz_gui_answer_query(conn, "collection.get", ursatz_gui_request_params(conn, "id", id, NULL), NULL, ursatz_gui_failed, false);
}

static int handle_list_pieces(struct mg_connection *conn, const char *corpus_id) {
  return ursatz_gui_answer_query(conn, "collection.pieces", ursatz_gui_request_params(conn, "id", corpus_id, NULL), NULL,
                                 ursatz_gui_failed, false);
}

#define ARG(args, i) ((args)->values[i])

/* An id that cannot name a collection is refused before any call. */
static int check_ids(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) {
  if (!ursatz_gui_valid_id(ARG(args, 0))) {
    return ursatz_gui_send_failure(conn, 400, "invalid id");
  }
  return 0;
}

static int route_list(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { (void)args; return handle_list_corpora(conn); }
static int route_get(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) {
  int refused = check_ids(conn, args);
  return refused ? refused : handle_get_corpus(conn, ARG(args, 0));
}
static int route_pieces(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) {
  int refused = check_ids(conn, args);
  return refused ? refused : handle_list_pieces(conn, ARG(args, 0));
}
static const UrsatzGuiRoute kRoutes[] = {
    {"GET", "/api/corpus", route_list},
    {"GET", "/api/corpus/<id>", route_get},
    {"GET", "/api/corpus/<id>/pieces", route_pieces},
};

const UrsatzGuiRoute *ursatz_gui_collections_routes(size_t *count) {
  *count = sizeof(kRoutes) / sizeof(kRoutes[0]);
  return kRoutes;
}
