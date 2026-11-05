#include "links.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <civetweb.h>

#include "route_util.h"
#include "../service/collection_service.h"
#include "../service/link_service.h"

static const char *kLinksPrefix = "/api/links";

/* --- scope ---------------------------------------------------------------- */

/* A string member, or NULL when it is missing or null. *bad is set when it is some other type. */
static const char *optional_string(const cJSON *object, const char *key, bool *bad) {
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
  if (!item || cJSON_IsNull(item)) {
    return NULL;
  }
  if (!cJSON_IsString(item)) {
    *bad = true;
    return NULL;
  }
  return item->valuestring;
}

/* Reads {"kind", "collection_id", "filter": {...}, "subject_id", "project_id"}. false when it cannot be a scope. */
static bool parse_scope(const cJSON *json, UrsatzGuiLinkScope *scope) {
  memset(scope, 0, sizeof(*scope));
  if (!cJSON_IsObject(json)) {
    return false;
  }
  bool bad = false;
  const char *kind = optional_string(json, "kind", &bad);
  if (!kind || bad) {
    return false;
  }
  if (strcmp(kind, "all") == 0) {
    scope->kind = URSATZ_GUI_LINK_SCOPE_ALL;
  } else if (strcmp(kind, "collection") == 0) {
    scope->kind = URSATZ_GUI_LINK_SCOPE_COLLECTION;
  } else if (strcmp(kind, "filter") == 0) {
    scope->kind = URSATZ_GUI_LINK_SCOPE_FILTER;
  } else if (strcmp(kind, "subject") == 0) {
    scope->kind = URSATZ_GUI_LINK_SCOPE_SUBJECT;
  } else if (strcmp(kind, "project") == 0) {
    scope->kind = URSATZ_GUI_LINK_SCOPE_PROJECT;
  } else {
    return false;
  }
  scope->collection_id = optional_string(json, "collection_id", &bad);
  scope->subject_id = optional_string(json, "subject_id", &bad);
  scope->project_id = optional_string(json, "project_id", &bad);
  const cJSON *filter = cJSON_GetObjectItemCaseSensitive(json, "filter");
  if (filter && !cJSON_IsNull(filter)) {
    if (!cJSON_IsObject(filter)) {
      return false;
    }
    scope->composer = optional_string(filter, "composer", &bad);
    scope->source = optional_string(filter, "source", &bad);
    scope->tag = optional_string(filter, "tag", &bad);
  }
  return !bad;
}

/* --- titles --------------------------------------------------------------- */

/* Adds "title" to an object that names a piece in "piece". */
static bool add_title(cJSON *end) {
  const cJSON *piece = cJSON_GetObjectItemCaseSensitive(end, "piece");
  if (!cJSON_IsString(piece)) {
    return true;
  }
  char *title = ursatz_gui_link_piece_title(piece->valuestring);
  bool ok = title && cJSON_AddStringToObject(end, "title", title);
  free(title);
  return ok;
}

/* Adds each end's title to a link ({"a", "b", ...}). */
static bool add_link_titles(cJSON *link) {
  return add_title(cJSON_GetObjectItemCaseSensitive(link, "a")) &&
         add_title(cJSON_GetObjectItemCaseSensitive(link, "b"));
}

/* Adds titles to every link end and every group member of a run's "result". */
static bool add_run_titles(cJSON *run) {
  cJSON *result = cJSON_GetObjectItemCaseSensitive(run, "result");
  cJSON *item = NULL;
  cJSON_ArrayForEach(item, cJSON_GetObjectItemCaseSensitive(result, "links")) {
    if (!add_link_titles(item)) {
      return false;
    }
  }
  cJSON_ArrayForEach(item, cJSON_GetObjectItemCaseSensitive(result, "groups")) {
    cJSON *member = NULL;
    cJSON_ArrayForEach(member, cJSON_GetObjectItemCaseSensitive(item, "members")) {
      if (!add_title(member)) {
        return false;
      }
    }
  }
  return true;
}

/* --- handlers ------------------------------------------------------------- */

static int send_link_failure(struct mg_connection *conn, UrsatzGuiLinkResult result) {
  switch (result) {
    case URSATZ_GUI_LINK_ERR_INVALID_SCOPE:
      return ursatz_gui_send_failure(conn, 400, "invalid scope");
    case URSATZ_GUI_LINK_ERR_NOT_FOUND:
      return ursatz_gui_send_failure(conn, 404, "not found");
    case URSATZ_GUI_LINK_ERR_EMPTY:
      return ursatz_gui_send_failure(conn, 422, "the scope names no pieces");
    case URSATZ_GUI_LINK_ERR_BUSY:
      return ursatz_gui_send_failure(conn, 409, "a run is in progress");
    case URSATZ_GUI_LINK_ERR_ALLOC:
      return ursatz_gui_send_failure(conn, 500, "out of memory");
    case URSATZ_GUI_LINK_ERR_FAILED:
    case URSATZ_GUI_LINK_OK:
    default:
      return ursatz_gui_send_failure(conn, 500, "link operation failed");
  }
}

/* {"ok":true,<key>:value}; takes value. */
static int send_ok_with(struct mg_connection *conn, int status, const char *key, cJSON *value) {
  cJSON *object = cJSON_CreateObject();
  if (!object || !value || !cJSON_AddBoolToObject(object, "ok", 1) || !cJSON_AddItemToObject(object, key, value)) {
    cJSON_Delete(value);
    cJSON_Delete(object);
    return ursatz_gui_send_failure(conn, 500, "out of memory");
  }
  return ursatz_gui_send_object(conn, status, object);
}

static int handle_start_run(struct mg_connection *conn) {
  cJSON *body = ursatz_gui_read_json_body(conn, NULL);
  if (!body) {
    return ursatz_gui_send_failure(conn, 400, "invalid request body");
  }
  UrsatzGuiLinkScope scope;
  if (!parse_scope(cJSON_GetObjectItemCaseSensitive(body, "scope"), &scope)) {
    cJSON_Delete(body);
    return ursatz_gui_send_failure(conn, 400, "invalid scope");
  }
  char *id = NULL;
  UrsatzGuiLinkResult result = ursatz_gui_link_run_start(&scope, &id);
  cJSON_Delete(body);
  if (result != URSATZ_GUI_LINK_OK) {
    return send_link_failure(conn, result);
  }
  cJSON *run = NULL;
  result = ursatz_gui_link_run_get(id, &run);
  free(id);
  if (result != URSATZ_GUI_LINK_OK) {
    return send_link_failure(conn, result);
  }
  return send_ok_with(conn, 202, "run", run);
}

static int handle_list_runs(struct mg_connection *conn) {
  cJSON *runs = NULL;
  UrsatzGuiLinkResult result = ursatz_gui_link_run_list(&runs);
  if (result != URSATZ_GUI_LINK_OK) {
    return send_link_failure(conn, result);
  }
  return send_ok_with(conn, 200, "runs", runs);
}

static int handle_get_run(struct mg_connection *conn, const char *id) {
  cJSON *run = NULL;
  UrsatzGuiLinkResult result = ursatz_gui_link_run_get(id, &run);
  if (result != URSATZ_GUI_LINK_OK) {
    return result == URSATZ_GUI_LINK_ERR_NOT_FOUND ? ursatz_gui_send_failure(conn, 404, "run not found")
                                                    : send_link_failure(conn, result);
  }
  if (!add_run_titles(run)) {
    cJSON_Delete(run);
    return ursatz_gui_send_failure(conn, 500, "out of memory");
  }
  return send_ok_with(conn, 200, "run", run);
}

static int handle_delete_run(struct mg_connection *conn, const char *id) {
  UrsatzGuiLinkResult result = ursatz_gui_link_run_delete(id);
  if (result == URSATZ_GUI_LINK_ERR_NOT_FOUND) {
    return ursatz_gui_send_failure(conn, 404, "run not found");
  }
  if (result != URSATZ_GUI_LINK_OK) {
    return send_link_failure(conn, result);
  }
  ursatz_gui_send_json(conn, "{\"ok\":true}");
  return 200;
}

static int handle_piece_links(struct mg_connection *conn, const char *piece_id) {
  cJSON *links = NULL;
  UrsatzGuiLinkResult result = ursatz_gui_link_piece_links(piece_id, &links);
  if (result != URSATZ_GUI_LINK_OK) {
    return send_link_failure(conn, result);
  }
  cJSON *entry = NULL;
  cJSON_ArrayForEach(entry, links) {
    if (!add_link_titles(cJSON_GetObjectItemCaseSensitive(entry, "link"))) {
      cJSON_Delete(links);
      return ursatz_gui_send_failure(conn, 500, "out of memory");
    }
  }
  return send_ok_with(conn, 200, "links", links);
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
    if (!path_id(rest + 5, id, sizeof(id)) || !ursatz_gui_collection_is_valid_id(id)) {
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
    if (!path_id(rest + 7, id, sizeof(id)) || !ursatz_gui_collection_is_valid_id(id)) {
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
