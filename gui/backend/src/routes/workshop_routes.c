#include "workshop_routes.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <civetweb.h>
#include <cJSON.h>

#include "route_util.h"
#include "../server.h"

/*
 * The generic workshop routes (R-659): the command line connects to the process that holds a data folder and calls the workshop through them.
 *   POST /api/workshop/query/<name>   the body is the params object (an empty body is {}); answered like every query
 *   POST /api/workshop/command        a JSON body is the command object exactly as workshop_command takes it, its actor included; a body of
 *                                     application/octet-stream is the command's file, with the command object percent-encoded in the header
 *                                     X-Ursatz-Command and the file's name in X-Filename
 *   GET  /api/workshop/status         workshop_status's members, the process id and what the open recovered
 * A refusal is {"ok": false, "error", "code" (the status's name), "path"} and, for a stale base version, "current_version"; its HTTP status is
 * the one table's (ursatz_gui_http_status).
 */

/* Paths that name the call's own members (a 400), not a member of a request a service read (a 422). */
static bool is_call_path(const char *path) {
  static const char *const kCallPaths[] = {"/params", "/body", "/target", "/bytes", "/base_version", "/command", "/actor", "/name", "/query"};
  for (size_t i = 0; i < sizeof(kCallPaths) / sizeof(kCallPaths[0]); i++) {
    if (strncmp(path, kCallPaths[i], strlen(kCallPaths[i])) == 0) {
      return true;
    }
  }
  return false;
}

/* Answers a refused call: the envelope with the members the failed call's reply holds (current_version). Frees the reply. */
static int send_refusal(struct mg_connection *conn, UrsatzStatus status, const UrsatzError *error, WorkshopReply *reply) {
  cJSON *object = cJSON_CreateObject();
  bool ok = object && cJSON_AddBoolToObject(object, "ok", 0) && cJSON_AddStringToObject(object, "error", error->message) &&
            cJSON_AddStringToObject(object, "code", ursatz_status_name(status)) &&
            (error->path[0] == '\0' || cJSON_AddStringToObject(object, "path", error->path));
  const cJSON *members = reply ? reply->json : NULL;
  const cJSON *member = NULL;
  cJSON_ArrayForEach(member, members) {
    if (ok && member->string && !cJSON_GetObjectItemCaseSensitive(object, member->string)) {
      cJSON *copy = cJSON_Duplicate(member, true);
      ok = copy && cJSON_AddItemToObject(object, member->string, copy);
      if (!ok) {
        cJSON_Delete(copy);
      }
    }
  }
  if (reply) {
    workshop_reply_destroy(reply);
  }
  if (!ok) {
    cJSON_Delete(object);
    return ursatz_gui_send_failure(conn, 500, "out of memory");
  }
  return ursatz_gui_send_object(conn, ursatz_gui_http_status(status, is_call_path(error->path) ? NULL : error), object);
}

static int send_request_refusal(struct mg_connection *conn, const char *path, const char *message) {
  UrsatzError error = {{0}, {0}};
  snprintf(error.message, sizeof(error.message), "%s", message);
  snprintf(error.path, sizeof(error.path), "%s", path);
  return send_refusal(conn, URSATZ_ERR_REQUEST, &error, NULL);
}

/* The body as a JSON object ({} for an empty one). NULL with the refusal sent (*status is what the route returns). */
static cJSON *read_object_body(struct mg_connection *conn, int *status) {
  char *data = NULL;
  size_t size = 0;
  UrsatzError error = {{0}, {0}};
  UrsatzStatus read = ursatz_gui_read_body(conn, URSATZ_GUI_JSON_BODY_MAX_BYTES, &data, &size, &error);
  if (read != URSATZ_OK) {
    *status = send_refusal(conn, read, &error, NULL);
    return NULL;
  }
  cJSON *object = size == 0 ? cJSON_CreateObject() : cJSON_Parse(data);
  free(data);
  if (!cJSON_IsObject(object)) {
    cJSON_Delete(object);
    *status = send_request_refusal(conn, "/body", "the body is a JSON object");
    return NULL;
  }
  return object;
}

/* POST /api/workshop/query/<name>. */
static int route_query(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) {
  int status = 0;
  cJSON *params = read_object_body(conn, &status);
  if (!params) {
    return status;
  }
  WorkshopReply reply;
  UrsatzError error = {{0}, {0}};
  UrsatzStatus result = ursatz_gui_run_query(args->values[0], params, NULL, &reply, &error);
  if (result != URSATZ_OK) {
    if (strcmp(error.path, "/query") == 0) {
      snprintf(error.path, sizeof(error.path), "/name"); /* the name is the route's */
    }
    return send_refusal(conn, result, &error, &reply);
  }
  if (reply.bytes) {
    return ursatz_gui_send_reply_bytes(conn, &reply);
  }
  const cJSON *job = cJSON_GetObjectItemCaseSensitive(reply.json, "job");
  return ursatz_gui_send_reply_ok(conn, job && !cJSON_GetObjectItemCaseSensitive(job, "stage") ? 202 : 200, &reply);
}

/* The percent-decoded header as a JSON object; NULL when it is absent or not one. */
static cJSON *command_from_header(struct mg_connection *conn) {
  const char *raw = mg_get_header(conn, "X-Ursatz-Command");
  if (!raw) {
    return NULL;
  }
  size_t length = strlen(raw);
  char *decoded = malloc(length + 1);
  if (!decoded) {
    return NULL;
  }
  int size = mg_url_decode(raw, (int)length, decoded, (int)length + 1, 0);
  cJSON *command = size >= 0 ? cJSON_Parse(decoded) : NULL;
  free(decoded);
  if (!cJSON_IsObject(command)) {
    cJSON_Delete(command);
    return NULL;
  }
  return command;
}

/* The file name header, percent-decoded ("" when absent); a malloc'ed copy. */
static char *filename_from_header(struct mg_connection *conn) {
  const char *raw = mg_get_header(conn, "X-Filename");
  size_t length = raw ? strlen(raw) : 0;
  char *decoded = calloc(length + 1, 1);
  if (decoded && raw) {
    int size = mg_url_decode(raw, (int)length, decoded, (int)length + 1, 0);
    decoded[size >= 0 ? size : 0] = '\0';
  }
  return decoded;
}

/* POST /api/workshop/command. */
static int route_command(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) {
  (void)args;
  const char *type = mg_get_header(conn, "Content-Type");
  bool file = type && strncmp(type, "application/octet-stream", 24) == 0;
  cJSON *command = NULL;
  char *data = NULL;
  size_t size = 0;
  int refused = 0;
  if (file) {
    UrsatzError error = {{0}, {0}};
    UrsatzStatus read = ursatz_gui_read_body(conn, URSATZ_GUI_UPLOAD_MAX_BYTES, &data, &size, &error);
    if (read != URSATZ_OK) {
      return send_refusal(conn, read, &error, NULL);
    }
    command = command_from_header(conn);
    if (!command) {
      free(data);
      return send_request_refusal(conn, "/body", "a file comes with the command object in the header X-Ursatz-Command");
    }
  } else {
    command = read_object_body(conn, &refused);
    if (!command) {
      return refused;
    }
  }
  char *filename = file ? filename_from_header(conn) : NULL;
  WorkshopBytes bytes = {(const uint8_t *)data, size, filename ? filename : ""};
  WorkshopReply reply;
  UrsatzError error = {{0}, {0}};
  UrsatzStatus result = workshop_command(ursatz_gui_workshop(), command, file ? &bytes : NULL, &reply, &error);
  cJSON_Delete(command);
  free(data);
  free(filename);
  if (result != URSATZ_OK) {
    return send_refusal(conn, result, &error, &reply);
  }
  const cJSON *job = cJSON_GetObjectItemCaseSensitive(reply.json, "job");
  return ursatz_gui_send_reply_ok(conn, job ? 202 : 200, &reply);
}

/* GET /api/workshop/status. */
static int route_status(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) {
  (void)args;
  Workshop *workshop = ursatz_gui_workshop();
  cJSON *versions = NULL;
  UrsatzError error = {{0}, {0}};
  UrsatzStatus status = workshop_status(workshop, &versions, &error);
  if (status != URSATZ_OK) {
    return send_refusal(conn, status, &error, NULL);
  }
  cJSON *object = cJSON_CreateObject();
  bool ok = object && cJSON_AddBoolToObject(object, "ok", 1);
  const cJSON *member = NULL;
  cJSON_ArrayForEach(member, versions) {
    cJSON *copy = ok && member->string ? cJSON_Duplicate(member, true) : NULL;
    ok = ok && copy && cJSON_AddItemToObject(object, member->string, copy);
    if (!ok) {
      cJSON_Delete(copy);
    }
  }
  cJSON_Delete(versions);
  ok = ok && cJSON_AddNumberToObject(object, "pid", (double)getpid()) &&
       cJSON_AddStringToObject(object, "recovery", workshop_recovery_summary(workshop));
  if (!ok) {
    cJSON_Delete(object);
    return ursatz_gui_send_failure(conn, 500, "out of memory");
  }
  return ursatz_gui_send_object(conn, 200, object);
}

static const UrsatzGuiRoute kRoutes[] = {
    {"POST", "/api/workshop/query/<name>", route_query},
    {"POST", "/api/workshop/command", route_command},
    {"GET", "/api/workshop/status", route_status},
};

const UrsatzGuiRoute *ursatz_gui_workshop_routes(size_t *count) {
  *count = sizeof(kRoutes) / sizeof(kRoutes[0]);
  return kRoutes;
}
