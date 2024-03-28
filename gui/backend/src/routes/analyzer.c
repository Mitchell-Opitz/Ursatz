#include "analyzer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <civetweb.h>

#include "../service/analysis_service.h"

static const char *kPiecesDir = "pieces";
static const size_t kReadChunkSize = 65536;
static const size_t kMaxUploadSize = 64 * 1024 * 1024;

/* Reads the full request body into a newly allocated buffer (caller
 * frees). Grows in kReadChunkSize steps since a raw binary upload's
 * Content-Length is not required to be trusted ahead of read. Returns
 * NULL on allocation failure or if the body exceeds kMaxUploadSize. */
static unsigned char *read_request_body(struct mg_connection *conn, size_t *out_size) {
  size_t capacity = kReadChunkSize;
  unsigned char *buffer = malloc(capacity);
  if (!buffer) {
    return NULL;
  }

  size_t total = 0;
  for (;;) {
    if (total == capacity) {
      if (capacity >= kMaxUploadSize) {
        free(buffer);
        return NULL;
      }
      size_t new_capacity = capacity * 2;
      unsigned char *grown = realloc(buffer, new_capacity);
      if (!grown) {
        free(buffer);
        return NULL;
      }
      buffer = grown;
      capacity = new_capacity;
    }

    int n = mg_read(conn, buffer + total, capacity - total);
    if (n < 0) {
      free(buffer);
      return NULL;
    }
    if (n == 0) {
      break;
    }
    total += (size_t)n;
  }

  *out_size = total;
  return buffer;
}

static void send_json(struct mg_connection *conn, const char *body) {
  size_t len = strlen(body);
  mg_send_http_ok(conn, "application/json", (long)len);
  mg_write(conn, body, len);
}

static int handle_post(struct mg_connection *conn) {
  size_t midi_size = 0;
  unsigned char *midi_data = read_request_body(conn, &midi_size);
  if (!midi_data) {
    send_json(conn, "{\"ok\":false,\"message\":\"failed to read request body\"}");
    return 400;
  }

  char *id = NULL;
  UrsatzGuiAnalysisResult result = ursatz_gui_analysis_import_and_save(kPiecesDir, midi_data, midi_size, &id);
  free(midi_data);

  if (result != URSATZ_GUI_ANALYSIS_OK) {
    const char *message = (result == URSATZ_GUI_ANALYSIS_ERR_IMPORT_FAILED)
                               ? "failed to import MIDI file"
                               : "failed to save piece";
    char body[256];
    snprintf(body, sizeof(body), "{\"ok\":false,\"message\":\"%s\"}", message);
    send_json(conn, body);
    return 200;
  }

  char body[320];
  snprintf(body, sizeof(body), "{\"ok\":true,\"id\":\"%s\"}", id);
  free(id);
  send_json(conn, body);
  return 200;
}

static int handle_get(struct mg_connection *conn) {
  char **ids = NULL;
  size_t count = 0;
  UrsatzGuiAnalysisResult result = ursatz_gui_analysis_list(kPiecesDir, &ids, &count);

  if (result != URSATZ_GUI_ANALYSIS_OK) {
    send_json(conn, "{\"ok\":false,\"message\":\"failed to list pieces\"}");
    return 200;
  }

  size_t capacity = 64;
  for (size_t i = 0; i < count; i++) {
    capacity += strlen(ids[i]) + 16;
  }
  char *body = malloc(capacity);
  if (!body) {
    for (size_t i = 0; i < count; i++) {
      free(ids[i]);
    }
    free(ids);
    send_json(conn, "{\"ok\":false,\"message\":\"out of memory\"}");
    return 200;
  }

  size_t offset = (size_t)snprintf(body, capacity, "{\"ok\":true,\"pieces\":[");
  for (size_t i = 0; i < count; i++) {
    offset += (size_t)snprintf(body + offset, capacity - offset, "%s{\"id\":\"%s\"}", i == 0 ? "" : ",", ids[i]);
    free(ids[i]);
  }
  free(ids);
  offset += (size_t)snprintf(body + offset, capacity - offset, "]}");

  send_json(conn, body);
  free(body);
  return 200;
}

static int pieces_handler(struct mg_connection *conn, void *cbdata) {
  (void)cbdata;
  const struct mg_request_info *info = mg_get_request_info(conn);
  if (strcmp(info->request_method, "POST") == 0) {
    return handle_post(conn);
  }
  if (strcmp(info->request_method, "GET") == 0) {
    return handle_get(conn);
  }

  mg_send_http_error(conn, 405, "Method Not Allowed");
  return 405;
}

void ursatz_gui_analyzer_register(struct mg_context *ctx) {
  mg_set_request_handler(ctx, "/api/analyzer/pieces", pieces_handler, NULL);
}
