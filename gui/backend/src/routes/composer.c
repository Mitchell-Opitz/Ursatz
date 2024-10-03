#include "composer.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <civetweb.h>
#include <cJSON.h>

#include "../service/composer_service.h"
#include "../service/composition_service.h"

static const char *kComposerPrefix = "/api/composer";
static const char *kCompositionsDir = "compositions";
static const size_t kMaxRequestBodySize = 64 * 1024;
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

/* Reads the request body into a NUL-terminated buffer (caller frees), or NULL
 * on a read error or a body over kMaxRequestBodySize. */
static char *read_request_body(struct mg_connection *conn) {
  size_t capacity = 4096;
  char *buffer = malloc(capacity);
  if (!buffer) {
    return NULL;
  }

  size_t total = 0;
  for (;;) {
    if (total + 1 == capacity) {
      if (capacity >= kMaxRequestBodySize) {
        free(buffer);
        return NULL;
      }
      char *grown = realloc(buffer, capacity * 2);
      if (!grown) {
        free(buffer);
        return NULL;
      }
      buffer = grown;
      capacity *= 2;
    }

    int n = mg_read(conn, buffer + total, capacity - total - 1);
    if (n < 0) {
      free(buffer);
      return NULL;
    }
    if (n == 0) {
      break;
    }
    total += (size_t)n;
  }

  buffer[total] = '\0';
  return buffer;
}

/* Reads the body as a JSON object. NULL (and a 400 already sent) otherwise. */
static cJSON *read_json_body(struct mg_connection *conn, int *status) {
  char *text = read_request_body(conn);
  cJSON *body = text ? cJSON_Parse(text) : NULL;
  free(text);
  if (!body || !cJSON_IsObject(body)) {
    cJSON_Delete(body);
    *status = send_error(conn, 400, "invalid json body");
    return NULL;
  }
  return body;
}

static int send_response(struct mg_connection *conn, UrsatzGuiCompositionResponse response) {
  int status = send_json_status(conn, response.status, reason_phrase(response.status),
                                response.body ? response.body : "{\"error\":\"out of memory\"}");
  ursatz_gui_composition_response_destroy(&response);
  return status;
}

/* Sends file bytes. attachment_name, when not NULL, makes it a download. */
static int send_bytes(struct mg_connection *conn, const char *content_type, const char *attachment_name,
                      const char *data, size_t size) {
  if (attachment_name) {
    mg_printf(conn,
              "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Disposition: attachment; filename=\"%s\"\r\n"
              "Content-Length: %zu\r\nConnection: close\r\n\r\n",
              content_type, attachment_name, size);
  } else {
    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n",
              content_type, size);
  }
  mg_write(conn, data, size);
  return 200;
}

/* The file a download name stands for: plan, musicxml or midi (a download). */
static bool file_kind(const char *what, UrsatzGuiCompositionFile *file, const char **content_type,
                      const char **extension, bool *attachment) {
  *attachment = false;
  if (strcmp(what, "plan") == 0) {
    *file = URSATZ_GUI_COMPOSITION_FILE_PLAN;
    *content_type = "application/json";
    *extension = "json";
  } else if (strcmp(what, "musicxml") == 0) {
    *file = URSATZ_GUI_COMPOSITION_FILE_MUSICXML;
    *content_type = "application/xml";
    *extension = "musicxml";
  } else if (strcmp(what, "midi") == 0) {
    *file = URSATZ_GUI_COMPOSITION_FILE_MIDI;
    *content_type = "audio/midi";
    *extension = "mid";
    *attachment = true;
  } else {
    return false;
  }
  return true;
}

/* Sends data (malloc'd, freed here) as the file `what`, named name_stem.<ext>. */
static int send_file(struct mg_connection *conn, char *data, size_t size, const char *what, const char *name_stem) {
  UrsatzGuiCompositionFile file;
  const char *content_type;
  const char *extension;
  bool attachment;
  if (!data || !file_kind(what, &file, &content_type, &extension, &attachment)) {
    free(data);
    return send_error(conn, 404, "not found");
  }
  char name[200];
  snprintf(name, sizeof(name), "%s.%s", name_stem, extension);
  int status = send_bytes(conn, content_type, attachment ? name : NULL, data, size);
  free(data);
  return status;
}

/* GET .../compositions/<id>/versions/<n>/plan | musicxml | midi. */
static int handle_version_file(struct mg_connection *conn, const char *id, const char *version, const char *what) {
  UrsatzGuiCompositionFile file;
  const char *content_type;
  const char *extension;
  bool attachment;
  if (!file_kind(what, &file, &content_type, &extension, &attachment)) {
    return send_error(conn, 404, "not found");
  }
  size_t size = 0;
  char *data = ursatz_gui_composition_read_version_file(kCompositionsDir, id, version, file, &size);
  char stem[160];
  snprintf(stem, sizeof(stem), "%s-v%s", id, version);
  return send_file(conn, data, size, what, stem);
}

/* GET .../compositions/<id>/variations/<k>/<i>/plan | musicxml | midi. */
static int handle_variation_file(struct mg_connection *conn, const char *id, const char *set, const char *index,
                                 const char *what) {
  UrsatzGuiCompositionFile file;
  const char *content_type;
  const char *extension;
  bool attachment;
  if (!file_kind(what, &file, &content_type, &extension, &attachment)) {
    return send_error(conn, 404, "not found");
  }
  size_t size = 0;
  char *data = ursatz_gui_composition_read_variation_file(kCompositionsDir, id, set, index, file, &size);
  char stem[160];
  snprintf(stem, sizeof(stem), "%s-variation-%s-%s", id, set, index);
  return send_file(conn, data, size, what, stem);
}

/* Splits rest at '/' into segments (in place). Returns the count, or 0 when
 * there are more than MAX_SEGMENTS or an empty one. */
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

static int handle_compositions(struct mg_connection *conn, const char *method, char *segments[], size_t count) {
  if (count == 1) {
    if (strcmp(method, "GET") != 0) {
      return send_error(conn, 405, "method not allowed");
    }
    return send_response(conn, ursatz_gui_composition_list(kCompositionsDir));
  }

  const char *id = segments[1];
  if (count == 2) {
    if (strcmp(method, "GET") == 0) {
      return send_response(conn, ursatz_gui_composition_get(kCompositionsDir, id));
    }
    if (strcmp(method, "DELETE") == 0) {
      return send_response(conn, ursatz_gui_composition_delete(kCompositionsDir, id));
    }
    if (strcmp(method, "PATCH") == 0) {
      int status = 0;
      cJSON *body = read_json_body(conn, &status);
      if (!body) {
        return status;
      }
      UrsatzGuiCompositionResponse response = ursatz_gui_composition_rename(kCompositionsDir, id, body);
      cJSON_Delete(body);
      return send_response(conn, response);
    }
    return send_error(conn, 405, "method not allowed");
  }

  if (count == 3 && (strcmp(segments[2], "lock") == 0 || strcmp(segments[2], "regenerate") == 0 ||
                     strcmp(segments[2], "variations") == 0)) {
    if (strcmp(method, "POST") != 0) {
      return send_error(conn, 405, "method not allowed");
    }
    int status = 0;
    cJSON *body = read_json_body(conn, &status);
    if (!body) {
      return status;
    }
    UrsatzGuiCompositionResponse response;
    if (strcmp(segments[2], "lock") == 0) {
      response = ursatz_gui_composition_lock(kCompositionsDir, id, body);
    } else if (strcmp(segments[2], "regenerate") == 0) {
      response = ursatz_gui_composition_regenerate(kCompositionsDir, id, body);
    } else {
      response = ursatz_gui_composition_variations(kCompositionsDir, id, body);
    }
    cJSON_Delete(body);
    return send_response(conn, response);
  }

  if (count == 5 && strcmp(segments[2], "versions") == 0) {
    if (strcmp(method, "GET") != 0) {
      return send_error(conn, 405, "method not allowed");
    }
    return handle_version_file(conn, id, segments[3], segments[4]);
  }

  /* .../variations/<k>/<i>/choose, or the files of variation i of set k. */
  if (count == 6 && strcmp(segments[2], "variations") == 0) {
    if (strcmp(segments[5], "choose") == 0) {
      if (strcmp(method, "POST") != 0) {
        return send_error(conn, 405, "method not allowed");
      }
      return send_response(conn, ursatz_gui_composition_choose(kCompositionsDir, id, segments[3], segments[4]));
    }
    if (strcmp(method, "GET") != 0) {
      return send_error(conn, 405, "method not allowed");
    }
    return handle_variation_file(conn, id, segments[3], segments[4], segments[5]);
  }

  return send_error(conn, 404, "not found");
}

/* POST /api/composer/grow and /api/composer/plan. */
static int handle_create(struct mg_connection *conn, bool grow) {
  int status = 0;
  cJSON *body = read_json_body(conn, &status);
  if (!body) {
    return status;
  }
  UrsatzGuiCompositionResponse response =
      grow ? ursatz_gui_composition_grow(kCompositionsDir, body) : ursatz_gui_composition_plan(kCompositionsDir, body);
  cJSON_Delete(body);
  return send_response(conn, response);
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
  if (count == 1 && (strcmp(segments[0], "grow") == 0 || strcmp(segments[0], "plan") == 0)) {
    if (strcmp(method, "POST") != 0) {
      return send_error(conn, 405, "method not allowed");
    }
    return handle_create(conn, strcmp(segments[0], "grow") == 0);
  }
  if (strcmp(segments[0], "compositions") == 0) {
    return handle_compositions(conn, method, segments, count);
  }

  return send_error(conn, 404, "not found");
}

void ursatz_gui_composer_register(struct mg_context *ctx) {
  mg_set_request_handler(ctx, kComposerPrefix, composer_handler, NULL);
}
