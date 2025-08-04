#include "materials.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <civetweb.h>

#include "../service/material_service.h"

static const char *kMaterialsPrefix = "/api/materials";
static const size_t kMaxRequestBodySize = 256 * 1024;
static const int kMaxSegments = 6;

static int send_json_status(struct mg_connection *conn, int status, const char *reason, const char *body) {
  size_t len = strlen(body);
  mg_printf(conn,
            "HTTP/1.1 %d %s\r\nContent-Type: application/json\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n",
            status, reason, len);
  mg_write(conn, body, len);
  return status;
}

static const char *reason_for(int status) {
  switch (status) {
    case 200:
      return "OK";
    case 404:
      return "Not Found";
    case 405:
      return "Method Not Allowed";
    case 409:
      return "Conflict";
    case 413:
      return "Payload Too Large";
    case 500:
      return "Internal Server Error";
    default:
      return "Bad Request";
  }
}

/* Sends {"ok":false,"error":...} with the given status. The message goes through cJSON, so it is escaped. */
static int send_failure(struct mg_connection *conn, int status, const char *message) {
  cJSON *object = cJSON_CreateObject();
  char *text = NULL;
  if (object && cJSON_AddBoolToObject(object, "ok", 0) && cJSON_AddStringToObject(object, "error", message)) {
    text = cJSON_PrintUnformatted(object);
  }
  cJSON_Delete(object);
  if (!text) {
    return send_json_status(conn, 500, reason_for(500), "{\"ok\":false,\"error\":\"out of memory\"}");
  }
  send_json_status(conn, status, reason_for(status), text);
  free(text);
  return status;
}

/* Sends object as the 200 response and frees it. */
static int send_object(struct mg_connection *conn, cJSON *object) {
  char *text = object ? cJSON_PrintUnformatted(object) : NULL;
  cJSON_Delete(object);
  if (!text) {
    return send_failure(conn, 500, "out of memory");
  }
  send_json_status(conn, 200, reason_for(200), text);
  free(text);
  return 200;
}

/* {"ok":true,<key>:value}; value is moved into the answer (and freed on failure). */
static int send_ok_with(struct mg_connection *conn, const char *key, cJSON *value) {
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
  return send_object(conn, object);
}

static int send_ok(struct mg_connection *conn) {
  send_json_status(conn, 200, reason_for(200), "{\"ok\":true}");
  return 200;
}

static int send_method_not_allowed(struct mg_connection *conn) {
  return send_failure(conn, 405, "method not allowed");
}

static int send_not_found(struct mg_connection *conn) {
  return send_failure(conn, 404, "not found");
}

/* Reads the request body as a JSON object. Returns NULL after sending the failure (400, or 413 past
 * kMaxRequestBodySize); *status is then what the caller returns. */
static cJSON *read_json_body(struct mg_connection *conn, int *status) {
  size_t capacity = 4096;
  char *buffer = malloc(capacity);
  size_t total = 0;
  *status = 400;
  if (!buffer) {
    *status = send_failure(conn, 500, "out of memory");
    return NULL;
  }
  for (;;) {
    if (total + 1 >= capacity) {
      if (capacity > kMaxRequestBodySize) {
        free(buffer);
        *status = send_failure(conn, 413, "request body too large");
        return NULL;
      }
      char *grown = realloc(buffer, capacity * 2);
      if (!grown) {
        free(buffer);
        *status = send_failure(conn, 500, "out of memory");
        return NULL;
      }
      buffer = grown;
      capacity *= 2;
    }
    int n = mg_read(conn, buffer + total, capacity - total - 1);
    if (n < 0) {
      free(buffer);
      *status = send_failure(conn, 400, "cannot read the request body");
      return NULL;
    }
    if (n == 0) {
      break;
    }
    total += (size_t)n;
    if (total > kMaxRequestBodySize) {
      free(buffer);
      *status = send_failure(conn, 413, "request body too large");
      return NULL;
    }
  }
  buffer[total] = '\0';
  cJSON *body = cJSON_Parse(buffer);
  free(buffer);
  if (!cJSON_IsObject(body)) {
    cJSON_Delete(body);
    *status = send_failure(conn, 400, "the body must be a JSON object");
    return NULL;
  }
  *status = 200;
  return body;
}

/* The string member, NULL when absent or null; *bad is set when it is present but not a string. */
static const char *optional_string(const cJSON *body, const char *key, bool *bad) {
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

static int material_status(UrsatzGuiMaterialResult result) {
  switch (result) {
    case URSATZ_GUI_MATERIAL_OK:
      return 200;
    case URSATZ_GUI_MATERIAL_ERR_INVALID_ID:
    case URSATZ_GUI_MATERIAL_ERR_INVALID_NAME:
    case URSATZ_GUI_MATERIAL_ERR_INVALID_TYPE:
    case URSATZ_GUI_MATERIAL_ERR_INVALID_BODY:
      return 400;
    case URSATZ_GUI_MATERIAL_ERR_NOT_FOUND:
      return 404;
    case URSATZ_GUI_MATERIAL_ERR_NOT_EMPTY:
    case URSATZ_GUI_MATERIAL_ERR_REFERENCED:
      return 409;
    default:
      return 500;
  }
}

static const char *material_message(UrsatzGuiMaterialResult result) {
  switch (result) {
    case URSATZ_GUI_MATERIAL_ERR_INVALID_ID:
      return "invalid id";
    case URSATZ_GUI_MATERIAL_ERR_INVALID_NAME:
      return "invalid name, description or note";
    case URSATZ_GUI_MATERIAL_ERR_INVALID_TYPE:
      return "invalid type";
    case URSATZ_GUI_MATERIAL_ERR_INVALID_BODY:
      return "invalid body";
    case URSATZ_GUI_MATERIAL_ERR_NOT_FOUND:
      return "not found";
    case URSATZ_GUI_MATERIAL_ERR_NOT_EMPTY:
      return "the project still holds items";
    case URSATZ_GUI_MATERIAL_ERR_REFERENCED:
      return "the item is used by other items";
    default:
      return "material store failure";
  }
}

static int send_material_failure(struct mg_connection *conn, UrsatzGuiMaterialResult result) {
  return send_failure(conn, material_status(result), material_message(result));
}

/* --- projects ------------------------------------------------------------ */

static int handle_list_projects(struct mg_connection *conn) {
  cJSON *projects = NULL;
  UrsatzGuiMaterialResult result = ursatz_gui_material_project_list(&projects);
  if (result != URSATZ_GUI_MATERIAL_OK) {
    return send_material_failure(conn, result);
  }
  return send_ok_with(conn, "projects", projects);
}

static int handle_create_project(struct mg_connection *conn) {
  int status = 0;
  cJSON *body = read_json_body(conn, &status);
  if (!body) {
    return status;
  }
  bool bad = false;
  const char *name = optional_string(body, "name", &bad);
  const char *description = optional_string(body, "description", &bad);
  if (bad || !name) {
    cJSON_Delete(body);
    return send_failure(conn, 400, "name is required; name and description are strings");
  }
  cJSON *project = NULL;
  UrsatzGuiMaterialResult result = ursatz_gui_material_project_create(name, description, &project);
  cJSON_Delete(body);
  if (result != URSATZ_GUI_MATERIAL_OK) {
    return send_material_failure(conn, result);
  }
  return send_ok_with(conn, "project", project);
}

static int handle_get_project(struct mg_connection *conn, const char *id) {
  cJSON *project = NULL;
  UrsatzGuiMaterialResult result = ursatz_gui_material_project_get(id, &project);
  if (result != URSATZ_GUI_MATERIAL_OK) {
    return send_material_failure(conn, result);
  }
  return send_ok_with(conn, "project", project);
}

static int handle_patch_project(struct mg_connection *conn, const char *id) {
  int status = 0;
  cJSON *body = read_json_body(conn, &status);
  if (!body) {
    return status;
  }
  bool bad = false;
  const char *name = optional_string(body, "name", &bad);
  const char *description = optional_string(body, "description", &bad);
  if (bad || (!name && !description)) {
    cJSON_Delete(body);
    return send_failure(conn, 400, "give name and/or description as strings");
  }
  cJSON *project = NULL;
  UrsatzGuiMaterialResult result = ursatz_gui_material_project_update(id, name, description, &project);
  cJSON_Delete(body);
  if (result != URSATZ_GUI_MATERIAL_OK) {
    return send_material_failure(conn, result);
  }
  return send_ok_with(conn, "project", project);
}

static int handle_delete_project(struct mg_connection *conn, const char *id) {
  UrsatzGuiMaterialResult result = ursatz_gui_material_project_delete(id);
  if (result != URSATZ_GUI_MATERIAL_OK) {
    return send_material_failure(conn, result);
  }
  return send_ok(conn);
}

/*
 * Splits the part of local_uri after prefix into path segments (copied into
 * storage). Returns the segment count, or -1 when there are too many, an
 * empty segment, or a segment of 256 characters or more.
 */
static int split_path(const char *rest, char segments[][256]) {
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

static int route_projects(struct mg_connection *conn, const char *method, char segments[][256], int count) {
  if (count == 1) {
    if (strcmp(method, "GET") == 0) {
      return handle_list_projects(conn);
    }
    if (strcmp(method, "POST") == 0) {
      return handle_create_project(conn);
    }
    return send_method_not_allowed(conn);
  }
  if (count == 2) {
    if (strcmp(method, "GET") == 0) {
      return handle_get_project(conn, segments[1]);
    }
    if (strcmp(method, "PATCH") == 0) {
      return handle_patch_project(conn, segments[1]);
    }
    if (strcmp(method, "DELETE") == 0) {
      return handle_delete_project(conn, segments[1]);
    }
    return send_method_not_allowed(conn);
  }
  return send_not_found(conn);
}

/*
 * Handler registered at "/api/materials". civetweb sends the exact prefix
 * and any "prefix/something" URI here (see the note in styles.c), so every
 * materials route is dispatched by its path segments.
 */
static int materials_handler(struct mg_connection *conn, void *cbdata) {
  (void)cbdata;
  const struct mg_request_info *info = mg_get_request_info(conn);
  const char *method = info->request_method;
  char segments[6][256];
  int count = split_path(info->local_uri + strlen(kMaterialsPrefix), segments);
  if (count < 1) {
    return send_not_found(conn);
  }
  if (strcmp(segments[0], "projects") == 0) {
    return route_projects(conn, method, segments, count);
  }
  return send_not_found(conn);
}

void ursatz_gui_materials_register(struct mg_context *ctx) {
  mg_set_request_handler(ctx, kMaterialsPrefix, materials_handler, NULL);
}
