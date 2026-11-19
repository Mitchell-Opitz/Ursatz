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

#include "jobs.h"
#include "route_util.h"
#include "workshop_internal.h"

static const size_t kMaxRequestBodySize = 256 * 1024;
static const int kMaxSegments = 8;

/* Sends {"ok":false,"error":...} with the given status. The message goes through cJSON, so it is escaped. */
/* The failure answer {"ok":false,"error":...}; a non-empty path (the request member the Composer refused, as a JSON
   Pointer) is added as "path". */
static inline int send_failure_at(struct mg_connection *conn, int status, const char *message, const char *path) {
  cJSON *object = cJSON_CreateObject();
  char *text = NULL;
  if (object && cJSON_AddBoolToObject(object, "ok", 0) && cJSON_AddStringToObject(object, "error", message) &&
      (!path || path[0] == '\0' || cJSON_AddStringToObject(object, "path", path))) {
    text = cJSON_PrintUnformatted(object);
  }
  cJSON_Delete(object);
  if (!text) {
    return ursatz_gui_send_json_status(conn, 500, ursatz_gui_reason_phrase(500), "{\"ok\":false,\"error\":\"out of memory\"}");
  }
  ursatz_gui_send_json_status(conn, status, ursatz_gui_reason_phrase(status), text);
  free(text);
  return status;
}

static inline int send_failure(struct mg_connection *conn, int status, const char *message) {
  return send_failure_at(conn, status, message, NULL);
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

static inline int send_method_not_allowed(struct mg_connection *conn) {
  return send_failure(conn, 405, "method not allowed");
}

static inline int send_not_found(struct mg_connection *conn) {
  return send_failure(conn, 404, "not found");
}

/* Reads the request body as a JSON object. Returns NULL after sending the failure (400, or 413 past
 * kMaxRequestBodySize); *status is then what the caller returns. */
static inline cJSON *read_json_body(struct mg_connection *conn, int *status) {
  return ursatz_gui_read_json_body_max(conn, kMaxRequestBodySize, status);
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

static inline int send_material_failure(struct mg_connection *conn, UrsatzStatus result) {
  return ursatz_gui_send_service_failure(conn, result, "material store failure");
}

/* The value of a query parameter into out; false when absent or too long. */
static inline bool query_value(const struct mg_connection *conn, const char *name, char *out, size_t out_size) {
  const struct mg_request_info *info = mg_get_request_info(conn);
  if (!info->query_string) {
    return false;
  }
  return mg_get_var(info->query_string, strlen(info->query_string), name, out, out_size) > 0;
}

/*
 * Splits the part of local_uri after prefix into path segments (copied into
 * storage). Returns the segment count, or -1 when there are too many, an
 * empty segment, or a segment of 256 characters or more.
 */
static inline int split_path(const char *rest, char segments[][256]) {
  int count = 0;
  if (*rest == '\0') {
    return 0;
  }
  if (*rest != '/') {
    return -1;
  }
  rest++;
  while (*rest != '\0') {
    const char *slash = strchr(rest, '/');
    size_t len = slash ? (size_t)(slash - rest) : strlen(rest);
    if (len == 0 || len >= 256 || count >= kMaxSegments) {
      return -1;
    }
    memcpy(segments[count], rest, len);
    segments[count][len] = '\0';
    count++;
    rest += len;
    if (*rest == '/') {
      rest++;
      if (*rest == '\0') {
        return -1;
      }
    }
  }
  return count;
}

/* Runs the query name for the route (its query variables and the path parameters given as name/value pairs, NULL ending them) and answers
 * it: bytes as a file, JSON as {"ok": true, ...members} (bare: as it is), a failure through failed. */
#define QUERY(conn, name, failed, bare, ...) \
  ursatz_gui_answer_query((conn), (name), ursatz_gui_request_params((conn), __VA_ARGS__, NULL), NULL, (failed), (bare))

/* Runs the query name with the request's JSON body as params (and the id of the path as "id" when given). The body's failures are
 * answered here (400, or 413 past kMaxRequestBodySize). */
static inline int query_with_body(struct mg_connection *conn, const char *name, UrsatzGuiFailureFn failed, bool bare, const char *id) {
  int status = 0;
  cJSON *body = ursatz_gui_read_json_body_max(conn, kMaxRequestBodySize, &status);
  if (!body) {
    return status;
  }
  if (id) {
    cJSON_DeleteItemFromObjectCaseSensitive(body, "id");
    cJSON_AddStringToObject(body, "id", id);
  }
  return ursatz_gui_answer_query(conn, name, body, NULL, failed, bare);
}

/* The failures the material routes answer: the message shape for the store's own failures, the error shape (with the request path at
 * fault) for what is wrong with the request. */
static inline int failed_material(struct mg_connection *conn, UrsatzStatus status, const UrsatzError *error) {
  return ursatz_gui_failed_message(conn, status, error);
}

static inline int failed_material_error(struct mg_connection *conn, UrsatzStatus status, const UrsatzError *error) {
  return ursatz_gui_failed_error(conn, status, error);
}

/* The failure answer of a workshop call: the status its error says (the Composer's refusal path travels as "path"). */
static inline int send_service_error(struct mg_connection *conn, UrsatzStatus status, const UrsatzError *error) {
  return send_failure_at(conn, workshop_http_status(status, NULL), error->message, error->path);
}

#endif /* URSATZ_GUI_ROUTE_MATERIALS_UTIL_H */
