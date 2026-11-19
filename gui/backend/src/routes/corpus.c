#include "corpus.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <civetweb.h>

#include "route_util.h"
#include "workshop_internal.h"

static const char *kCorpusPrefix = "/api/corpus";

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

/*
 * Single handler registered at kCorpusPrefix ("/api/corpus"). civetweb
 * routes both the exact prefix and any "prefix/something" URI to a
 * handler registered at prefix (see the identical note in analyzer.c), so
 * every corpus route -- "/api/corpus", ".../<id>/pieces",
 * ".../<id>/pieces/<piece_id>" -- arrives here and is dispatched by
 * local_uri.
 */
static int corpus_handler(struct mg_connection *conn, void *cbdata) {
  (void)cbdata;
  const struct mg_request_info *info = mg_get_request_info(conn);
  const char *uri = info->local_uri;
  const char *method = info->request_method;
  size_t prefix_len = strlen(kCorpusPrefix);

  if (strcmp(uri, kCorpusPrefix) == 0) {
    if (strcmp(method, "POST") == 0) {
      return handle_create_corpus(conn);
    }
    if (strcmp(method, "GET") == 0) {
      return handle_list_corpora(conn);
    }
    mg_send_http_error(conn, 405, "Method Not Allowed");
    return 405;
  }

  if (strncmp(uri, kCorpusPrefix, prefix_len) != 0 || uri[prefix_len] != '/') {
    mg_send_http_error(conn, 404, "Not Found");
    return 404;
  }

  /* rest is "<corpus_id>", "<corpus_id>/pieces" or "<corpus_id>/pieces/<piece_id>". */
  const char *rest = uri + prefix_len + 1;
  const char *slash = strchr(rest, '/');
  size_t corpus_id_len = slash ? (size_t)(slash - rest) : strlen(rest);
  if (corpus_id_len == 0 || corpus_id_len >= 256) {
    mg_send_http_error(conn, 404, "Not Found");
    return 404;
  }
  char corpus_id[256];
  memcpy(corpus_id, rest, corpus_id_len);
  corpus_id[corpus_id_len] = '\0';

  if (!workshop_collection_is_valid_id(corpus_id)) {
    send_ok_false(conn, "invalid id");
    return 200;
  }

  if (!slash) {
    if (strcmp(method, "GET") == 0) {
      return handle_get_corpus(conn, corpus_id);
    }
    if (strcmp(method, "PATCH") == 0) {
      return handle_patch_corpus(conn, corpus_id);
    }
    if (strcmp(method, "DELETE") == 0) {
      return handle_delete_corpus(conn, corpus_id);
    }
    mg_send_http_error(conn, 405, "Method Not Allowed");
    return 405;
  }

  const char *pieces_start = slash;
  if (strncmp(pieces_start, "/pieces", strlen("/pieces")) != 0 ||
      (pieces_start[strlen("/pieces")] != '\0' && pieces_start[strlen("/pieces")] != '/')) {
    mg_send_http_error(conn, 404, "Not Found");
    return 404;
  }

  const char *after_pieces = pieces_start + strlen("/pieces");

  if (after_pieces[0] == '\0') {
    if (strcmp(method, "POST") == 0) {
      return handle_add_pieces(conn, corpus_id);
    }
    if (strcmp(method, "GET") == 0) {
      return handle_list_pieces(conn, corpus_id);
    }
    mg_send_http_error(conn, 405, "Method Not Allowed");
    return 405;
  }

  if (after_pieces[0] == '/' && after_pieces[1] != '\0') {
    const char *piece_id = after_pieces + 1;
    if (!workshop_collection_is_valid_id(piece_id)) {
      send_ok_false(conn, "invalid piece_id");
      return 200;
    }
    if (strcmp(method, "DELETE") == 0) {
      return handle_remove_piece(conn, corpus_id, piece_id);
    }
    mg_send_http_error(conn, 405, "Method Not Allowed");
    return 405;
  }

  mg_send_http_error(conn, 404, "Not Found");
  return 404;
}

void ursatz_gui_corpus_register(struct mg_context *ctx) {
  mg_set_request_handler(ctx, kCorpusPrefix, corpus_handler, NULL);
}
