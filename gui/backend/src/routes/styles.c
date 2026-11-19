#include "styles.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <civetweb.h>

#include "route_util.h"
#include "workshop_internal.h"

static const char *kStylesPrefix = "/api/styles";

static int send_style_failure(struct mg_connection *conn, UrsatzStatus result) {
  return ursatz_gui_send_service_failure(conn, result, "style operation failed");
}

/* {"ok":true,"style":{list entry},"skipped":[...]} for the stored style id. */
static int send_built_style(struct mg_connection *conn, const char *id) {
  cJSON *entry = NULL;
  cJSON *skipped = NULL;
  UrsatzStatus result = workshop_style_get_entry(id, &entry, &skipped);
  if (result != URSATZ_OK) {
    return send_style_failure(conn, result);
  }
  cJSON *object = cJSON_CreateObject();
  if (!object || !cJSON_AddBoolToObject(object, "ok", 1) || !cJSON_AddItemToObject(object, "style", entry)) {
    cJSON_Delete(entry);
    cJSON_Delete(skipped);
    cJSON_Delete(object);
    return ursatz_gui_send_failure(conn, 500, "out of memory");
  }
  if (!cJSON_AddItemToObject(object, "skipped", skipped)) {
    cJSON_Delete(skipped);
    cJSON_Delete(object);
    return ursatz_gui_send_failure(conn, 500, "out of memory");
  }
  return ursatz_gui_send_object(conn, 200, object);
}

static int handle_build_style(struct mg_connection *conn) {
  cJSON *body = ursatz_gui_read_json_body(conn, NULL);
  if (!body) {
    return ursatz_gui_send_failure(conn, 400, "invalid request body");
  }
  WorkshopStyleRequest request;
  UrsatzError error = {{0}, {0}};
  UrsatzStatus parsed = workshop_style_build_request_parse(body, &request, &error);
  if (parsed != URSATZ_OK) {
    cJSON_Delete(body);
    return ursatz_gui_send_failure(conn, workshop_http_status(parsed, NULL), error.message);
  }
  char *id = NULL;
  UrsatzStatus result = workshop_style_build(request.name, &request.source, &id, NULL, NULL);
  WorkshopStyleSourceKind kind = request.source.kind;
  workshop_style_request_free(&request);
  cJSON_Delete(body);
  if (result == URSATZ_ERR_NOT_FOUND && kind == WORKSHOP_STYLE_SOURCE_COLLECTION) {
    return ursatz_gui_send_failure(conn, 404, "collection not found");
  }
  if (result == URSATZ_ERR_NOT_FOUND && kind == WORKSHOP_STYLE_SOURCE_BRIEF) {
    return ursatz_gui_send_failure(conn, 404, "brief source not found");
  }
  if (result != URSATZ_OK) {
    return send_style_failure(conn, result);
  }
  int status = send_built_style(conn, id);
  free(id);
  return status;
}

static int handle_list_styles(struct mg_connection *conn) {
  cJSON *list = NULL;
  UrsatzStatus result = workshop_style_list(&list);
  if (result != URSATZ_OK) {
    return send_style_failure(conn, result);
  }
  cJSON *object = cJSON_CreateObject();
  if (!object || !cJSON_AddBoolToObject(object, "ok", 1) || !cJSON_AddItemToObject(object, "styles", list)) {
    cJSON_Delete(list);
    cJSON_Delete(object);
    return ursatz_gui_send_failure(conn, 500, "out of memory");
  }
  return ursatz_gui_send_object(conn, 200, object);
}

static int handle_get_style(struct mg_connection *conn, const char *id) {
  cJSON *style = NULL;
  UrsatzStatus result = workshop_style_get(id, &style);
  if (result != URSATZ_OK) {
    return send_style_failure(conn, result);
  }
  cJSON *object = cJSON_CreateObject();
  if (!object || !cJSON_AddBoolToObject(object, "ok", 1) || !cJSON_AddItemToObject(object, "style", style)) {
    cJSON_Delete(style);
    cJSON_Delete(object);
    return ursatz_gui_send_failure(conn, 500, "out of memory");
  }
  return ursatz_gui_send_object(conn, 200, object);
}

static int handle_rebuild_style(struct mg_connection *conn, const char *id) {
  UrsatzStatus result = workshop_style_rebuild(id);
  if (result != URSATZ_OK) {
    return send_style_failure(conn, result);
  }
  return send_built_style(conn, id);
}

static int handle_delete_style(struct mg_connection *conn, const char *id) {
  UrsatzStatus result = workshop_style_delete(id);
  if (result != URSATZ_OK) {
    return send_style_failure(conn, result);
  }
  ursatz_gui_send_json(conn, "{\"ok\":true}");
  return 200;
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
  if (!workshop_collection_is_valid_id(id)) {
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
