#include "route_util.h"
#include "../server.h"
#include "workshop_internal.h"

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

int ursatz_gui_send_failure(struct mg_connection *conn, int status, const char *message) {
  cJSON *object = cJSON_CreateObject();
  char *text = NULL;
  if (object && cJSON_AddBoolToObject(object, "ok", 0) && cJSON_AddStringToObject(object, "message", message)) {
    text = cJSON_PrintUnformatted(object);
  }
  cJSON_Delete(object);
  if (!text) {
    return ursatz_gui_send_json_status(conn, 500, "Internal Server Error", "{\"ok\":false,\"message\":\"out of memory\"}");
  }
  if (status == 200) {
    ursatz_gui_send_json(conn, text);
  } else {
    ursatz_gui_send_json_status(conn, status, ursatz_gui_reason_phrase(status), text);
  }
  free(text);
  return status;
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
  cJSON *object = cJSON_CreateObject();
  char *text = NULL;
  if (object && cJSON_AddBoolToObject(object, "ok", 0) && cJSON_AddStringToObject(object, "error", message)) {
    text = cJSON_PrintUnformatted(object);
  }
  cJSON_Delete(object);
  if (!text) {
    return ursatz_gui_send_json_status(conn, 500, "Internal Server Error", "{\"ok\":false,\"error\":\"out of memory\"}");
  }
  ursatz_gui_send_json_status(conn, status, reason, text);
  free(text);
  return status;
}

/* Reports a failure: sent when the caller asked for a status, silent otherwise. */
static cJSON *fail(struct mg_connection *conn, int *status, int code, const char *reason, const char *message) {
  if (status) {
    *status = send_body_failure(conn, code, reason, message);
  }
  return NULL;
}

cJSON *ursatz_gui_read_json_body_max(struct mg_connection *conn, size_t max_bytes, int *status_or_null) {
  size_t capacity = 4096;
  char *buffer = malloc(capacity);
  size_t total = 0;
  if (status_or_null) {
    *status_or_null = 400;
  }
  if (!buffer) {
    return fail(conn, status_or_null, 500, "Internal Server Error", "out of memory");
  }
  for (;;) {
    if (total + 1 >= capacity) {
      if (capacity > max_bytes) {
        free(buffer);
        return fail(conn, status_or_null, 413, "Payload Too Large", "request body too large");
      }
      char *grown = realloc(buffer, capacity * 2);
      if (!grown) {
        free(buffer);
        return fail(conn, status_or_null, 500, "Internal Server Error", "out of memory");
      }
      buffer = grown;
      capacity *= 2;
    }
    int n = mg_read(conn, buffer + total, capacity - total - 1);
    if (n < 0) {
      free(buffer);
      return fail(conn, status_or_null, 400, "Bad Request", "cannot read the request body");
    }
    if (n == 0) {
      break;
    }
    total += (size_t)n;
    if (total > max_bytes) {
      free(buffer);
      return fail(conn, status_or_null, 413, "Payload Too Large", "request body too large");
    }
  }
  buffer[total] = '\0';
  cJSON *body = cJSON_Parse(buffer);
  free(buffer);
  if (!cJSON_IsObject(body)) {
    cJSON_Delete(body);
    return fail(conn, status_or_null, 400, "Bad Request", "the body must be a JSON object");
  }
  if (status_or_null) {
    *status_or_null = 200;
  }
  return body;
}

cJSON *ursatz_gui_read_json_body(struct mg_connection *conn, int *status_or_null) {
  return ursatz_gui_read_json_body_max(conn, URSATZ_GUI_JSON_BODY_MAX_BYTES, status_or_null);
}

const char *ursatz_gui_failure_message(const char *fallback) {
  const UrsatzError *error = workshop_last_error();
  return error->message[0] != '\0' ? error->message : fallback;
}

int ursatz_gui_send_service_failure(struct mg_connection *conn, UrsatzStatus status, const char *fallback) {
  return ursatz_gui_send_failure(conn, workshop_http_status(status, workshop_last_error()), ursatz_gui_failure_message(fallback));
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

int ursatz_gui_failed_message(struct mg_connection *conn, UrsatzStatus status, const UrsatzError *error) {
  return ursatz_gui_send_failure(conn, workshop_http_status(status, NULL), error->message);
}

int ursatz_gui_failed_error(struct mg_connection *conn, UrsatzStatus status, const UrsatzError *error) {
  cJSON *object = cJSON_CreateObject();
  char *text = NULL;
  if (object && cJSON_AddBoolToObject(object, "ok", 0) && cJSON_AddStringToObject(object, "error", error->message) &&
      (error->path[0] == '\0' || cJSON_AddStringToObject(object, "path", error->path))) {
    text = cJSON_PrintUnformatted(object);
  }
  cJSON_Delete(object);
  int http = workshop_http_status(status, NULL);
  if (!text) {
    return ursatz_gui_send_json_status(conn, 500, "Internal Server Error", "{\"ok\":false,\"error\":\"out of memory\"}");
  }
  ursatz_gui_send_json_status(conn, http, ursatz_gui_reason_phrase(http), text);
  free(text);
  return http;
}
