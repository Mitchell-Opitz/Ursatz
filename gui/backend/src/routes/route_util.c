#include "route_util.h"
#include "../server.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

int ursatz_gui_send_json_status(struct mg_connection *conn, int status, const char *reason, const char *body) {
  size_t len = strlen(body);
  mg_printf(conn,
            "HTTP/1.1 %d %s\r\nContent-Type: application/json\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n",
            status, reason, len);
  mg_write(conn, body, len);
  return status;
}

const char *ursatz_gui_reason_phrase(int status) {
  switch (status) {
    case 200:
      return "OK";
    case 202:
      return "Accepted";
    case 204:
      return "No Content";
    case 400:
      return "Bad Request";
    case 404:
      return "Not Found";
    case 405:
      return "Method Not Allowed";
    case 409:
      return "Conflict";
    case 413:
      return "Payload Too Large";
    case 422:
      return "Unprocessable Entity";
    case 503:
      return "Service Unavailable";
    case 504:
      return "Gateway Timeout";
    default:
      return status < 500 ? "Bad Request" : "Internal Server Error";
  }
}

void ursatz_gui_send_json(struct mg_connection *conn, const char *body) {
  size_t len = strlen(body);
  mg_send_http_ok(conn, "application/json", (long)len);
  mg_write(conn, body, len);
}

/* The name of the status a failure with this HTTP status stands for (the "code" member of the envelope). */
static const char *code_of_http(int status) {
  switch (status) {
    case 400:
    case 405:
    case 413:
      return ursatz_status_name(URSATZ_ERR_REQUEST);
    case 404:
      return ursatz_status_name(URSATZ_ERR_NOT_FOUND);
    case 409:
      return ursatz_status_name(URSATZ_ERR_CONFLICT);
    case 422:
      return ursatz_status_name(URSATZ_ERR_UNSATISFIABLE);
    case 499:
      return ursatz_status_name(URSATZ_ERR_CANCELLED);
    default:
      return ursatz_status_name(URSATZ_ERR_INTERNAL);
  }
}

/* The failure envelope (R-647): {"ok": false, "error": <message>, "code": <status name>, "path": <pointer, when set>}. */
int ursatz_gui_send_failure_at(struct mg_connection *conn, int status, const char *message, const char *code, const char *path) {
  cJSON *object = cJSON_CreateObject();
  char *text = NULL;
  if (object && cJSON_AddBoolToObject(object, "ok", 0) && cJSON_AddStringToObject(object, "error", message) &&
      cJSON_AddStringToObject(object, "code", code ? code : code_of_http(status)) &&
      (!path || path[0] == '\0' || cJSON_AddStringToObject(object, "path", path))) {
    text = cJSON_PrintUnformatted(object);
  }
  cJSON_Delete(object);
  if (!text) {
    return ursatz_gui_send_json_status(conn, 500, "Internal Server Error", "{\"ok\":false,\"error\":\"out of memory\",\"code\":\"alloc\"}");
  }
  ursatz_gui_send_json_status(conn, status, ursatz_gui_reason_phrase(status), text);
  free(text);
  return status;
}

int ursatz_gui_send_failure(struct mg_connection *conn, int status, const char *message) {
  return ursatz_gui_send_failure_at(conn, status, message, NULL, NULL);
}

int ursatz_gui_send_object(struct mg_connection *conn, int status, cJSON *object) {
  char *text = object ? cJSON_PrintUnformatted(object) : NULL;
  cJSON_Delete(object);
  if (!text) {
    return ursatz_gui_send_failure(conn, 500, "out of memory");
  }
  if (status == 200) {
    ursatz_gui_send_json(conn, text);
  } else {
    ursatz_gui_send_json_status(conn, status, ursatz_gui_reason_phrase(status), text);
  }
  free(text);
  return status;
}

static int send_body_failure(struct mg_connection *conn, int status, const char *reason, const char *message) {
  (void)reason;
  return ursatz_gui_send_failure(conn, status, message);
}

/* The request body the route reads, up to cap bytes: the one reader of every route (R-647). Over the cap is a REQUEST at /body/size (413), a body that cannot
 * be read a REQUEST at /body (400). *data is NUL-terminated and the caller's (free it). */
UrsatzStatus ursatz_gui_read_body(struct mg_connection *conn, size_t cap, char **data, size_t *size, UrsatzError *error) {
  size_t capacity = 4096;
  size_t total = 0;
  char *buffer = malloc(capacity);
  *data = NULL;
  if (!buffer) {
    return ursatz_error_set(error, URSATZ_ERR_ALLOC, "", "out of memory");
  }
  for (;;) {
    if (total + 1 >= capacity) {
      if (capacity > cap) {
        free(buffer);
        return ursatz_error_set(error, URSATZ_ERR_REQUEST, "/body/size", "request body too large");
      }
      char *grown = realloc(buffer, capacity * 2);
      if (!grown) {
        free(buffer);
        return ursatz_error_set(error, URSATZ_ERR_ALLOC, "", "out of memory");
      }
      buffer = grown;
      capacity *= 2;
    }
    int n = mg_read(conn, buffer + total, capacity - total - 1);
    if (n < 0) {
      free(buffer);
      return ursatz_error_set(error, URSATZ_ERR_REQUEST, "/body", "cannot read the request body");
    }
    if (n == 0) {
      break;
    }
    total += (size_t)n;
    if (total > cap) {
      free(buffer);
      return ursatz_error_set(error, URSATZ_ERR_REQUEST, "/body/size", "request body too large");
    }
  }
  buffer[total] = '\0';
  *data = buffer;
  if (size) {
    *size = total;
  }
  return URSATZ_OK;
}

/* The request body as a JSON object (caller frees), up to max_bytes; NULL after sending the failure envelope (status in *status_or_null when given,
 * else nothing is sent and the caller answers). */
cJSON *ursatz_gui_read_json_body_max(struct mg_connection *conn, size_t max_bytes, int *status_or_null) {
  char *text = NULL;
  UrsatzError error = {{0}, {0}};
  UrsatzStatus read = ursatz_gui_read_body(conn, max_bytes, &text, NULL, &error);
  cJSON *body = read == URSATZ_OK ? cJSON_Parse(text) : NULL;
  free(text);
  if (read == URSATZ_OK && !cJSON_IsObject(body)) {
    cJSON_Delete(body);
    body = NULL;
    ursatz_error_set(&error, URSATZ_ERR_REQUEST, "/body", "the body must be a JSON object");
    read = URSATZ_ERR_REQUEST;
  }
  if (body) {
    if (status_or_null) {
      *status_or_null = 200;
    }
    return body;
  }
  if (status_or_null) {
    *status_or_null = ursatz_gui_failed(conn, read, &error);
  }
  return NULL;
}

cJSON *ursatz_gui_read_json_body(struct mg_connection *conn, int *status_or_null) {
  return ursatz_gui_read_json_body_max(conn, URSATZ_GUI_JSON_BODY_MAX_BYTES, status_or_null);
}

int ursatz_gui_http_status(UrsatzStatus status, const UrsatzError *error) {
  if (status == URSATZ_ERR_REQUEST && error && strcmp(error->path, "/body/size") == 0) {
    return 413;
  }
  switch (status) {
    case URSATZ_OK:
      return 200;
    case URSATZ_ERR_REQUEST:
      return error && error->path[0] != '\0' ? 422 : 400;
    case URSATZ_ERR_UNSATISFIABLE:
      return 422;
    case URSATZ_ERR_NOT_FOUND:
      return 404;
    case URSATZ_ERR_CONFLICT:
      return 409;
    case URSATZ_ERR_CANCELLED:
      return 499;
    default:
      return 500;
  }
}

bool ursatz_gui_valid_id(const char *text) {
  size_t length = text ? strlen(text) : 0;
  if (length == 0 || length > 255) {
    return false;
  }
  for (size_t i = 0; i < length; i++) {
    unsigned char c = (unsigned char)text[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-' || c == '_')) {
      return false;
    }
  }
  return true;
}

cJSON *ursatz_gui_query_params(const struct mg_request_info *info) {
  cJSON *params = cJSON_CreateObject();
  const char *query = info->query_string;
  if (!params || !query) {
    return params;
  }
  size_t query_length = strlen(query);
  char name[128];
  char value[1024];
  const char *cursor = query;
  while (cursor < query + query_length) {
    const char *end = strchr(cursor, '&');
    size_t length = end ? (size_t)(end - cursor) : strlen(cursor);
    const char *equals = memchr(cursor, '=', length);
    size_t name_length = equals ? (size_t)(equals - cursor) : length;
    if (name_length > 0 && name_length < sizeof(name)) {
      char raw[128];
      memcpy(raw, cursor, name_length);
      raw[name_length] = '\0';
      int decoded_name = mg_url_decode(raw, (int)name_length, name, (int)sizeof(name), 1);
      int occurrence = 0;
      if (decoded_name > 0 && !cJSON_GetObjectItemCaseSensitive(params, name)) {
        for (;;) {
          int got = mg_get_var2(query, query_length, name, value, sizeof(value), (size_t)occurrence);
          if (got < 0) {
            break;
          }
          cJSON *existing = cJSON_GetObjectItemCaseSensitive(params, name);
          if (occurrence == 0) {
            cJSON_AddStringToObject(params, name, value);
          } else if (existing) {
            if (!cJSON_IsArray(existing)) {
              cJSON *array = cJSON_CreateArray();
              cJSON_AddItemToArray(array, cJSON_CreateString(existing->valuestring));
              cJSON_ReplaceItemInObjectCaseSensitive(params, name, array);
              existing = array;
            }
            cJSON_AddItemToArray(existing, cJSON_CreateString(value));
          }
          occurrence++;
        }
      }
    }
    if (!end) {
      break;
    }
    cursor = end + 1;
  }
  return params;
}

UrsatzStatus ursatz_gui_run_query(const char *name, cJSON *params, const WorkshopBytes *bytes, WorkshopReply *reply, UrsatzError *error) {
  Workshop *workshop = ursatz_gui_workshop();
  UrsatzStatus status = workshop_query(workshop, name, params, bytes, reply, error);
  cJSON_Delete(params);
  return status;
}

int ursatz_gui_send_reply_ok(struct mg_connection *conn, int status, WorkshopReply *reply) {
  cJSON *answer = cJSON_CreateObject();
  cJSON *members = reply->json;
  reply->json = NULL;
  bool built = answer && cJSON_AddBoolToObject(answer, "ok", 1);
  if (built && cJSON_IsObject(members)) {
    cJSON *member = members->child;
    while (member) {
      cJSON *next = member->next;
      if (strcmp(member->string, "ok") != 0) {
        cJSON_DetachItemViaPointer(members, member);
        cJSON_AddItemToObject(answer, member->string, member);
      }
      member = next;
    }
  }
  cJSON_Delete(members);
  workshop_reply_destroy(reply);
  if (!built) {
    cJSON_Delete(answer);
    return ursatz_gui_send_failure(conn, 500, "out of memory");
  }
  return ursatz_gui_send_object(conn, status, answer);
}

int ursatz_gui_send_reply_json(struct mg_connection *conn, int status, WorkshopReply *reply) {
  cJSON *answer = reply->json;
  reply->json = NULL;
  workshop_reply_destroy(reply);
  return ursatz_gui_send_object(conn, status, answer);
}

int ursatz_gui_send_reply_bytes(struct mg_connection *conn, WorkshopReply *reply) {
  mg_send_http_ok(conn, reply->content_type, (long long)reply->size);
  mg_write(conn, reply->bytes, reply->size);
  workshop_reply_destroy(reply);
  return 200;
}

int ursatz_gui_answer_query(struct mg_connection *conn, const char *name, cJSON *params, const WorkshopBytes *bytes,
                            UrsatzGuiFailureFn failed, bool bare) {
  WorkshopReply reply;
  UrsatzError error = {{0}, {0}};
  UrsatzStatus status = ursatz_gui_run_query(name, params, bytes, &reply, &error);
  if (status != URSATZ_OK) {
    workshop_reply_destroy(&reply);
    return failed(conn, status, &error);
  }
  if (reply.bytes) {
    return ursatz_gui_send_reply_bytes(conn, &reply);
  }
  /* A job just queued ({id, kind, state}) is 202; the state of a job a query reads (job.get) has its stage and is 200. */
  const cJSON *job = cJSON_GetObjectItemCaseSensitive(reply.json, "job");
  if (job && !cJSON_GetObjectItemCaseSensitive(job, "stage")) {
    return ursatz_gui_send_reply_ok(conn, 202, &reply);
  }
  return bare ? ursatz_gui_send_reply_json(conn, 200, &reply) : ursatz_gui_send_reply_ok(conn, 200, &reply);
}

cJSON *ursatz_gui_request_params(struct mg_connection *conn, ...) {
  cJSON *params = ursatz_gui_query_params(mg_get_request_info(conn));
  va_list arguments;
  va_start(arguments, conn);
  const char *name;
  while (params && (name = va_arg(arguments, const char *)) != NULL) {
    const char *value = va_arg(arguments, const char *);
    cJSON_DeleteItemFromObjectCaseSensitive(params, name);
    cJSON_AddStringToObject(params, name, value);
  }
  va_end(arguments);
  return params;
}

/* A path that points into the call itself ("/params/...", "/target", "/bytes") says what is wrong with the request's own members: a 400. A path into
 * the request the Composer reads (the body of a motif, a theme, a piece) is the member the Composer refused: a 422. */
static int is_call_path(const char *path) {
  return strncmp(path, "/params", 7) == 0 || strncmp(path, "/body", 5) == 0 || strncmp(path, "/target", 7) == 0 || strncmp(path, "/bytes", 6) == 0 ||
         strncmp(path, "/base_version", 13) == 0;
}

int ursatz_gui_failed(struct mg_connection *conn, UrsatzStatus status, const UrsatzError *error) {
  int http = ursatz_gui_http_status(status, is_call_path(error->path) && strcmp(error->path, "/body/size") != 0 ? NULL : error);
  return ursatz_gui_send_failure_at(conn, http, error->message, ursatz_status_name(status), error->path);
}

UrsatzStatus ursatz_gui_run_command(const char *name, const char *target, cJSON *params, const WorkshopBytes *bytes, WorkshopReply *reply,
                                    UrsatzError *error) {
  cJSON *command = cJSON_CreateObject();
  /* The version the page read travels in the request as base_version (the body, or the query of a delete); the command carries it. */
  cJSON *base = params ? cJSON_DetachItemFromObjectCaseSensitive(params, "base_version") : NULL;
  if (cJSON_IsString(base)) {
    char *end = NULL;
    double number = strtod(base->valuestring, &end);
    bool whole = end && *end == '\0' && end != base->valuestring;
    cJSON_Delete(base);
    base = whole ? cJSON_CreateNumber(number) : cJSON_CreateString("");
  }
  if (!command || !cJSON_AddStringToObject(command, "command", name) || !cJSON_AddStringToObject(command, "actor", "michelle") ||
      !cJSON_AddStringToObject(command, "target", target ? target : "") || !cJSON_AddItemToObject(command, "params", params ? params : cJSON_CreateObject()) ||
      (base && !cJSON_AddItemToObject(command, "base_version", base))) {
    cJSON_Delete(command);
    memset(reply, 0, sizeof(*reply));
    snprintf(error->message, sizeof(error->message), "out of memory");
    error->path[0] = '\0';
    return URSATZ_ERR_ALLOC;
  }
  UrsatzStatus status = workshop_command(ursatz_gui_workshop(), command, bytes, reply, error);
  cJSON_Delete(command);
  if (reply->json) {
    cJSON_DeleteItemFromObjectCaseSensitive(reply->json, "journal_seq");
  }
  return status;
}

int ursatz_gui_answer_command(struct mg_connection *conn, const char *name, const char *target, cJSON *params, const WorkshopBytes *bytes,
                              UrsatzGuiFailureFn failed, bool bare) {
  WorkshopReply reply;
  UrsatzError error = {{0}, {0}};
  UrsatzStatus status = ursatz_gui_run_command(name, target, params, bytes, &reply, &error);
  if (status == URSATZ_ERR_CONFLICT && cJSON_GetObjectItemCaseSensitive(reply.json, "current_version")) {
    /* The item changed since the page read it: 409 {"ok": false, "error", "current_version"}. */
    cJSON *object = cJSON_CreateObject();
    cJSON *current = cJSON_DetachItemFromObjectCaseSensitive(reply.json, "current_version");
    workshop_reply_destroy(&reply);
    if (!object || !cJSON_AddBoolToObject(object, "ok", 0) || !cJSON_AddStringToObject(object, "error", error.message) ||
        !cJSON_AddItemToObject(object, "current_version", current)) {
      cJSON_Delete(object);
      cJSON_Delete(current);
      return ursatz_gui_send_failure(conn, 500, "out of memory");
    }
    return ursatz_gui_send_object(conn, 409, object);
  }
  if (status != URSATZ_OK) {
    int answered = failed(conn, status, &error);
    workshop_reply_destroy(&reply);
    return answered;
  }
  const cJSON *job = cJSON_GetObjectItemCaseSensitive(reply.json, "job");
  if (job && !cJSON_GetObjectItemCaseSensitive(job, "stage")) {
    return ursatz_gui_send_reply_ok(conn, 202, &reply);
  }
  return bare ? ursatz_gui_send_reply_json(conn, 200, &reply) : ursatz_gui_send_reply_ok(conn, 200, &reply);
}
