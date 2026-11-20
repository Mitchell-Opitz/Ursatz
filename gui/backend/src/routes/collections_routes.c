#include "collections_routes.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <civetweb.h>

#include "route_util.h"


static void send_ok_false(struct mg_connection *conn, const char *message) {
  ursatz_gui_send_failure(conn, 200, message);
}

/* A missing collection is a 404; every other failure keeps the route's
 * 200 with {"ok":false,...}. */

/* The failures of a collection command: a missing collection is a 404; every other one keeps the route's 200 with {"ok":false,...} (but the
 * add of pieces answers 400 for a request it refuses: too many pieces, an unknown piece). */
static int failed_collection(struct mg_connection *conn, UrsatzStatus status, const UrsatzError *error) {
  return ursatz_gui_send_failure(conn, status == URSATZ_ERR_NOT_FOUND ? 404 : 200, error->message);
}

static int failed_add_pieces(struct mg_connection *conn, UrsatzStatus status, const UrsatzError *error) {
  if (status == URSATZ_ERR_REQUEST && strncmp(error->message, "invalid", 7) != 0) {
    return ursatz_gui_send_failure(conn, 400, error->message);
  }
  return failed_collection(conn, status, error);
}

static int handle_create_corpus(struct mg_connection *conn) {
  cJSON *body = ursatz_gui_read_json_body(conn, NULL);
  if (!body) {
    return ursatz_gui_send_failure(conn, 200, "invalid request body");
  }
  return ursatz_gui_answer_command(conn, "collection.create", "", body, NULL, failed_collection, false);
}

static int handle_list_corpora(struct mg_connection *conn) {
  return ursatz_gui_answer_query(conn, "collection.list", ursatz_gui_request_params(conn, NULL), NULL, failed_collection, false);
}

static int handle_get_corpus(struct mg_connection *conn, const char *id) {
  return ursatz_gui_answer_query(conn, "collection.get", ursatz_gui_request_params(conn, "id", id, NULL), NULL, failed_collection, false);
}

static int handle_patch_corpus(struct mg_connection *conn, const char *id) {
  cJSON *body = ursatz_gui_read_json_body(conn, NULL);
  if (!body) {
    return ursatz_gui_send_failure(conn, 200, "invalid request body");
  }
  return ursatz_gui_answer_command(conn, "collection.update", id, body, NULL, failed_collection, false);
}

static int handle_delete_corpus(struct mg_connection *conn, const char *id) {
  return ursatz_gui_answer_command(conn, "collection.delete", id, NULL, NULL, failed_collection, false);
}

/* Adds the ids of {"piece_id":"..."} or {"piece_ids":["...", ...]} to the collection. A piece already in it is counted, not an error:
 * {"ok":true,"added":n,"already":n}. */
static int handle_add_pieces(struct mg_connection *conn, const char *corpus_id) {
  cJSON *body = ursatz_gui_read_json_body(conn, NULL);
  if (!body) {
    return ursatz_gui_send_failure(conn, 200, "invalid request body");
  }
  return ursatz_gui_answer_command(conn, "collection.add-pieces", corpus_id, body, NULL, failed_add_pieces, false);
}

static int handle_remove_piece(struct mg_connection *conn, const char *corpus_id, const char *piece_id) {
  cJSON *params = cJSON_CreateObject();
  if (params) {
    cJSON_AddStringToObject(params, "piece_id", piece_id);
  }
  return ursatz_gui_answer_command(conn, "collection.remove-piece", corpus_id, params, NULL, failed_collection, false);
}

static int handle_list_pieces(struct mg_connection *conn, const char *corpus_id) {
  return ursatz_gui_answer_query(conn, "collection.pieces", ursatz_gui_request_params(conn, "id", corpus_id, NULL), NULL,
                                 failed_collection, false);
}

#define ARG(args, i) ((args)->values[i])

/* An id that cannot name a collection or a piece is refused before any call: the route's 200 with {"ok": false} (until the envelope). */
static int check_ids(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) {
  if (!ursatz_gui_valid_id(ARG(args, 0))) {
    send_ok_false(conn, "invalid id");
    return 200;
  }
  if (args->count > 1 && !ursatz_gui_valid_id(ARG(args, 1))) {
    send_ok_false(conn, "invalid piece_id");
    return 200;
  }
  return 0;
}

static int route_create(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { (void)args; return handle_create_corpus(conn); }
static int route_list(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { (void)args; return handle_list_corpora(conn); }
static int route_get(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) {
  int refused = check_ids(conn, args);
  return refused ? refused : handle_get_corpus(conn, ARG(args, 0));
}
static int route_patch(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) {
  int refused = check_ids(conn, args);
  return refused ? refused : handle_patch_corpus(conn, ARG(args, 0));
}
static int route_delete(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) {
  int refused = check_ids(conn, args);
  return refused ? refused : handle_delete_corpus(conn, ARG(args, 0));
}
static int route_add(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) {
  int refused = check_ids(conn, args);
  return refused ? refused : handle_add_pieces(conn, ARG(args, 0));
}
static int route_pieces(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) {
  int refused = check_ids(conn, args);
  return refused ? refused : handle_list_pieces(conn, ARG(args, 0));
}
static int route_remove(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) {
  int refused = check_ids(conn, args);
  return refused ? refused : handle_remove_piece(conn, ARG(args, 0), ARG(args, 1));
}

static const UrsatzGuiRoute kRoutes[] = {
    {"POST", "/api/corpus", route_create},
    {"GET", "/api/corpus", route_list},
    {"GET", "/api/corpus/<id>", route_get},
    {"PATCH", "/api/corpus/<id>", route_patch},
    {"DELETE", "/api/corpus/<id>", route_delete},
    {"POST", "/api/corpus/<id>/pieces", route_add},
    {"GET", "/api/corpus/<id>/pieces", route_pieces},
    {"DELETE", "/api/corpus/<id>/pieces/<id>", route_remove},
};

const UrsatzGuiRoute *ursatz_gui_collections_routes(size_t *count) {
  *count = sizeof(kRoutes) / sizeof(kRoutes[0]);
  return kRoutes;
}
