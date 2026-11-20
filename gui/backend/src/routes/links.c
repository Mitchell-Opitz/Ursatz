#include "links.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <civetweb.h>

#include "route_util.h"

static const char *kLinksPrefix = "/api/links";

/* --- scope ---------------------------------------------------------------- */

/* --- handlers ------------------------------------------------------------- */

static int handle_start_run(struct mg_connection *conn) {
  cJSON *body = ursatz_gui_read_json_body(conn, NULL);
  if (!body) {
    return ursatz_gui_send_failure(conn, 400, "invalid request body");
  }
  /* 202 {"ok", "run", "job"}: the page follows the job (BackendJobs.runJob) and gets {"ok", "run"} when it ends. */
  return ursatz_gui_answer_command(conn, "links.run", "", body, NULL, ursatz_gui_failed_message, false);
}

static int handle_list_runs(struct mg_connection *conn) {
  return ursatz_gui_answer_query(conn, "links.list", ursatz_gui_request_params(conn, NULL), NULL, ursatz_gui_failed_message, false);
}

static int handle_get_run(struct mg_connection *conn, const char *id) {
  return ursatz_gui_answer_query(conn, "links.get", ursatz_gui_request_params(conn, "id", id, NULL), NULL, ursatz_gui_failed_message, false);
}

static int handle_delete_run(struct mg_connection *conn, const char *id) {
  return ursatz_gui_answer_command(conn, "links.delete", id, NULL, NULL, ursatz_gui_failed_message, false);
}

static int handle_piece_links(struct mg_connection *conn, const char *piece_id) {
  return ursatz_gui_answer_query(conn, "links.piece", ursatz_gui_request_params(conn, "id", piece_id, NULL), NULL, ursatz_gui_failed_message,
                                 false);
}

/* The id after "<prefix>/", or false when it is empty, too long, or has a slash in it. */
static bool path_id(const char *rest, char *out, size_t out_size) {
  size_t len = strlen(rest);
  if (len == 0 || len >= out_size || strchr(rest, '/') != NULL) {
    return false;
  }
  memcpy(out, rest, len + 1);
  return true;
}

/*
 * Single handler registered at kLinksPrefix ("/api/links"). civetweb routes both the exact prefix and any
 * "prefix/something" URI to a handler registered at prefix (see the identical note in corpus.c), so every links route
 * arrives here and is dispatched by local_uri.
 */
static int links_handler(struct mg_connection *conn, void *cbdata) {
  (void)cbdata;
  const struct mg_request_info *info = mg_get_request_info(conn);
  const char *uri = info->local_uri;
  const char *method = info->request_method;
  size_t prefix_len = strlen(kLinksPrefix);

  if (strncmp(uri, kLinksPrefix, prefix_len) != 0 || uri[prefix_len] != '/') {
    mg_send_http_error(conn, 404, "Not Found");
    return 404;
  }
  const char *rest = uri + prefix_len + 1;
  char id[256];

  if (strcmp(rest, "runs") == 0) {
    if (strcmp(method, "POST") == 0) {
      return handle_start_run(conn);
    }
    if (strcmp(method, "GET") == 0) {
      return handle_list_runs(conn);
    }
    mg_send_http_error(conn, 405, "Method Not Allowed");
    return 405;
  }

  if (strncmp(rest, "runs/", 5) == 0) {
    if (!path_id(rest + 5, id, sizeof(id)) || !ursatz_gui_valid_id(id)) {
      return ursatz_gui_send_failure(conn, 404, "run not found");
    }
    if (strcmp(method, "GET") == 0) {
      return handle_get_run(conn, id);
    }
    if (strcmp(method, "DELETE") == 0) {
      return handle_delete_run(conn, id);
    }
    mg_send_http_error(conn, 405, "Method Not Allowed");
    return 405;
  }

  if (strncmp(rest, "pieces/", 7) == 0) {
    if (!path_id(rest + 7, id, sizeof(id)) || !ursatz_gui_valid_id(id)) {
      return ursatz_gui_send_failure(conn, 404, "piece not found");
    }
    if (strcmp(method, "GET") == 0) {
      return handle_piece_links(conn, id);
    }
    mg_send_http_error(conn, 405, "Method Not Allowed");
    return 405;
  }

  mg_send_http_error(conn, 404, "Not Found");
  return 404;
}

void ursatz_gui_links_register(struct mg_context *ctx) {
  mg_set_request_handler(ctx, kLinksPrefix, links_handler, NULL);
}
