#include "route_util.h"

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
