#include "styles.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <civetweb.h>

#include "jobs.h"
#include "route_util.h"

static const char *kStylesPrefix = "/api/styles";

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

/*
 * Single handler registered at kStylesPrefix ("/api/styles"). civetweb routes
 * both the exact prefix and any "prefix/something" URI to a handler
 * registered at prefix (see the identical note in corpus.c), so every styles
 * route arrives here and is dispatched by local_uri.
 */
static int styles_handler(struct mg_connection *conn, void *cbdata) {
  (void)cbdata;
  const struct mg_request_info *info = mg_get_request_info(conn);
  const char *uri = info->local_uri;
  const char *method = info->request_method;
  size_t prefix_len = strlen(kStylesPrefix);

  if (strcmp(uri, kStylesPrefix) == 0) {
    if (strcmp(method, "POST") == 0) {
      return handle_build_style(conn);
    }
    if (strcmp(method, "GET") == 0) {
      return handle_list_styles(conn);
    }
    mg_send_http_error(conn, 405, "Method Not Allowed");
    return 405;
  }

  if (strncmp(uri, kStylesPrefix, prefix_len) != 0 || uri[prefix_len] != '/') {
    mg_send_http_error(conn, 404, "Not Found");
    return 404;
  }

  /* rest is "<id>" or "<id>/rebuild". */
  const char *rest = uri + prefix_len + 1;
  const char *slash = strchr(rest, '/');
  size_t id_len = slash ? (size_t)(slash - rest) : strlen(rest);
  if (id_len == 0 || id_len >= 256) {
    mg_send_http_error(conn, 404, "Not Found");
    return 404;
  }
  char id[256];
  memcpy(id, rest, id_len);
  id[id_len] = '\0';

  /* An id that cannot name a style is an unknown style. */
  if (!ursatz_gui_valid_id(id)) {
    return ursatz_gui_send_failure(conn, 404, "style not found");
  }

  if (!slash) {
    if (strcmp(method, "GET") == 0) {
      return handle_get_style(conn, id);
    }
    if (strcmp(method, "DELETE") == 0) {
      return handle_delete_style(conn, id);
    }
    mg_send_http_error(conn, 405, "Method Not Allowed");
    return 405;
  }

  if (strcmp(slash, "/rebuild") == 0) {
    if (strcmp(method, "POST") == 0) {
      return handle_rebuild_style(conn, id);
    }
    mg_send_http_error(conn, 405, "Method Not Allowed");
    return 405;
  }

  mg_send_http_error(conn, 404, "Not Found");
  return 404;
}

void ursatz_gui_styles_register(struct mg_context *ctx) {
  mg_set_request_handler(ctx, kStylesPrefix, styles_handler, NULL);
}
