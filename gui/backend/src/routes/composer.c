#include "composer.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <civetweb.h>
#include <cJSON.h>

#include "../service/composer_service.h"

static const char *kComposerPrefix = "/api/composer";
#define MAX_SEGMENTS 8

/* Sends body as application/json with the given status line. */
static int send_json_status(struct mg_connection *conn, int status, const char *reason, const char *body) {
  size_t len = strlen(body);
  mg_printf(conn,
            "HTTP/1.1 %d %s\r\nContent-Type: application/json\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n",
            status, reason, len);
  mg_write(conn, body, len);
  return status;
}

static const char *reason_phrase(int status) {
  switch (status) {
    case 200:
      return "OK";
    case 400:
      return "Bad Request";
    case 404:
      return "Not Found";
    case 405:
      return "Method Not Allowed";
    case 422:
      return "Unprocessable Entity";
    case 503:
      return "Service Unavailable";
    case 504:
      return "Gateway Timeout";
    default:
      return "Internal Server Error";
  }
}

/* Sends a cJSON object (and frees it). */
static int send_object(struct mg_connection *conn, int status, cJSON *object) {
  char *text = object ? cJSON_PrintUnformatted(object) : NULL;
  cJSON_Delete(object);
  if (!text) {
    return send_json_status(conn, 500, "Internal Server Error", "{\"error\":\"out of memory\"}");
  }
  int sent = send_json_status(conn, status, reason_phrase(status), text);
  free(text);
  return sent;
}

static int send_error(struct mg_connection *conn, int status, const char *error) {
  cJSON *object = cJSON_CreateObject();
  if (object) {
    cJSON_AddStringToObject(object, "error", error);
  }
  return send_object(conn, status, object);
}

static char *trimmed_copy(const char *text) {
  size_t len = strlen(text);
  while (len > 0 && (text[len - 1] == '\n' || text[len - 1] == '\r' || text[len - 1] == ' ')) {
    len--;
  }
  char *copy = malloc(len + 1);
  if (copy) {
    memcpy(copy, text, len);
    copy[len] = '\0';
  }
  return copy;
}

/* GET /api/composer/status: whether the Composer binary is there and what
 * it reports as its version. */
static int handle_status(struct mg_connection *conn) {
  const char *args[] = {"--version", NULL};
  UrsatzGuiComposerOutput output;
  UrsatzGuiComposerResult result = ursatz_gui_composer_run(args, &output);

  if (result == URSATZ_GUI_COMPOSER_ERR_NO_BINARY) {
    cJSON *object = cJSON_CreateObject();
    if (object) {
      cJSON_AddStringToObject(object, "error", "composer not found");
      cJSON_AddFalseToObject(object, "available");
    }
    return send_object(conn, 503, object);
  }
  if (result == URSATZ_GUI_COMPOSER_ERR_TIMEOUT) {
    return send_error(conn, 504, "composer timed out");
  }
  if (result != URSATZ_GUI_COMPOSER_OK || output.exit_status != 0) {
    cJSON *object = cJSON_CreateObject();
    if (object) {
      cJSON_AddStringToObject(object, "error", "composer failed");
      cJSON_AddStringToObject(object, "stderr", result == URSATZ_GUI_COMPOSER_OK ? output.stderr_text : "");
    }
    ursatz_gui_composer_output_destroy(&output);
    return send_object(conn, 500, object);
  }

  char *version = trimmed_copy(output.stdout_text);
  ursatz_gui_composer_output_destroy(&output);
  cJSON *object = cJSON_CreateObject();
  if (!version || !object) {
    free(version);
    cJSON_Delete(object);
    return send_error(conn, 500, "out of memory");
  }
  cJSON_AddTrueToObject(object, "ok");
  cJSON_AddTrueToObject(object, "available");
  cJSON_AddStringToObject(object, "version", version);
  free(version);
  return send_object(conn, 200, object);
}

static size_t split_path(char *rest, char *segments[MAX_SEGMENTS]) {
  size_t count = 0;
  char *start = rest;
  for (;;) {
    char *slash = strchr(start, '/');
    if (*start == '\0' || slash == start || count == MAX_SEGMENTS) {
      return 0;
    }
    segments[count++] = start;
    if (!slash) {
      return count;
    }
    *slash = '\0';
    start = slash + 1;
  }
}

/*
 * Single handler registered at kComposerPrefix ("/api/composer"). civetweb
 * routes the prefix and every "prefix/something" URI to it (see the note in
 * analyzer.c), so each composer route is dispatched here by local_uri.
 */
static int composer_handler(struct mg_connection *conn, void *cbdata) {
  (void)cbdata;
  const struct mg_request_info *info = mg_get_request_info(conn);
  const char *uri = info->local_uri;
  const char *method = info->request_method;
  size_t prefix_len = strlen(kComposerPrefix);

  if (strncmp(uri, kComposerPrefix, prefix_len) != 0 || uri[prefix_len] != '/') {
    return send_error(conn, 404, "not found");
  }

  char rest[256];
  if (snprintf(rest, sizeof(rest), "%s", uri + prefix_len + 1) >= (int)sizeof(rest)) {
    return send_error(conn, 404, "not found");
  }
  char *segments[MAX_SEGMENTS];
  size_t count = split_path(rest, segments);
  if (count == 0) {
    return send_error(conn, 404, "not found");
  }

  if (count == 1 && strcmp(segments[0], "status") == 0) {
    if (strcmp(method, "GET") != 0) {
      return send_error(conn, 405, "method not allowed");
    }
    return handle_status(conn);
  }

  return send_error(conn, 404, "not found");
}

void ursatz_gui_composer_register(struct mg_context *ctx) {
  mg_set_request_handler(ctx, kComposerPrefix, composer_handler, NULL);
}
