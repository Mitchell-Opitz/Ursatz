#ifndef URSATZ_GUI_ROUTE_MATERIALS_UTIL_H
#define URSATZ_GUI_ROUTE_MATERIALS_UTIL_H

/*
 * What the material, motif and subject routes share: the answers {"ok": ...}, the JSON body reader, the query value and the path
 * splitter. B12 replaces these routes with the one route table.
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <civetweb.h>

#include "jobs_routes.h"
#include "route_util.h"


/* The failure envelope with a path (the request member the Composer refused, as a JSON Pointer) when it has one. */
static inline int send_failure_at(struct mg_connection *conn, int status, const char *message, const char *path) {
  return ursatz_gui_send_failure_at(conn, status, message, NULL, path);
}

static inline int send_failure(struct mg_connection *conn, int status, const char *message) {
  return ursatz_gui_send_failure(conn, status, message);
}

/* {"ok":true,<key>:value}; value is moved into the answer (and freed on failure). */
static inline int send_ok_with(struct mg_connection *conn, const char *key, cJSON *value) {
  cJSON *object = cJSON_CreateObject();
  if (!object || !value || !cJSON_AddBoolToObject(object, "ok", 1)) {
    cJSON_Delete(value);
    cJSON_Delete(object);
    return send_failure(conn, 500, "out of memory");
  }
  if (!cJSON_AddItemToObject(object, key, value)) {
    cJSON_Delete(value);
    cJSON_Delete(object);
    return send_failure(conn, 500, "out of memory");
  }
  return ursatz_gui_send_object(conn, 200, object);
}

static inline int send_ok(struct mg_connection *conn) {
  ursatz_gui_send_json_status(conn, 200, ursatz_gui_reason_phrase(200), "{\"ok\":true}");
  return 200;
}

/* Reads the request body as a JSON object. Returns NULL after sending the failure (400, or 413 past
 * URSATZ_GUI_JSON_BODY_MAX_BYTES); *status is then what the caller returns. */
static inline cJSON *read_json_body(struct mg_connection *conn, int *status) {
  return ursatz_gui_read_json_body_max(conn, URSATZ_GUI_JSON_BODY_MAX_BYTES, status);
}

/* The string member, NULL when absent or null; *bad is set when it is present but not a string. */
static inline const char *optional_string(const cJSON *body, const char *key, bool *bad) {
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(body, key);
  if (!item || cJSON_IsNull(item)) {
    return NULL;
  }
  if (!cJSON_IsString(item)) {
    *bad = true;
    return NULL;
  }
  return item->valuestring;
}

/* The value of a query parameter into out; false when absent or too long. */
static inline bool query_value(const struct mg_connection *conn, const char *name, char *out, size_t out_size) {
  const struct mg_request_info *info = mg_get_request_info(conn);
  if (!info->query_string) {
    return false;
  }
  return mg_get_var(info->query_string, strlen(info->query_string), name, out, out_size) > 0;
}

/* Runs the query name for the route (its query variables and the path parameters given as name/value pairs, NULL ending them) and answers
 * it: bytes as a file, JSON as {"ok": true, ...members} (bare: as it is), a failure through failed. */
#define QUERY(conn, name, failed, bare, ...) \
  ursatz_gui_answer_query((conn), (name), ursatz_gui_request_params((conn), __VA_ARGS__, NULL), NULL, (failed), (bare))

/* Runs the query name with the request's JSON body as params (and the id of the path as "id" when given). The body's failures are
 * answered here (400, or 413 past URSATZ_GUI_JSON_BODY_MAX_BYTES). */
static inline int query_with_body(struct mg_connection *conn, const char *name, UrsatzGuiFailureFn failed, bool bare, const char *id) {
  int status = 0;
  cJSON *body = ursatz_gui_read_json_body_max(conn, URSATZ_GUI_JSON_BODY_MAX_BYTES, &status);
  if (!body) {
    return status;
  }
  if (id) {
    cJSON_DeleteItemFromObjectCaseSensitive(body, "id");
    cJSON_AddStringToObject(body, "id", id);
  }
  return ursatz_gui_answer_query(conn, name, body, NULL, failed, bare);
}

#endif /* URSATZ_GUI_ROUTE_MATERIALS_UTIL_H */
