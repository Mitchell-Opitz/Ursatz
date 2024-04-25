#include "analyzer.h"

#include <stdbool.h>
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

/* Append-only buffer that grows as needed, used in place of a
 * pre-sized malloc + snprintf/offset accumulation. That older pattern
 * undersized its per-field capacity estimate for handle_get_piece's
 * output: once snprintf started truncating, offset still advanced by
 * the untruncated would-be length, so offset could exceed capacity and
 * the following "capacity - offset" (both size_t) would underflow into
 * a huge value passed as a write size -- a heap overflow that crashed
 * the process on pieces with enough interpretations. */
typedef struct {
  char *data;
  size_t length;
  size_t capacity;
  bool ok;
} JsonBuffer;

static void json_buffer_init(JsonBuffer *buf) {
  buf->data = NULL;
  buf->length = 0;
  buf->capacity = 0;
  buf->ok = true;
}

static void json_buffer_reserve(JsonBuffer *buf, size_t additional) {
  if (!buf->ok) {
    return;
  }
  size_t needed = buf->length + additional + 1; /* +1 for the NUL terminator */
  if (needed <= buf->capacity) {
    return;
  }
  size_t new_capacity = buf->capacity == 0 ? 256 : buf->capacity;
  while (new_capacity < needed) {
    new_capacity *= 2;
  }
  char *grown = realloc(buf->data, new_capacity);
  if (!grown) {
    buf->ok = false;
    return;
  }
  buf->data = grown;
  buf->capacity = new_capacity;
}

static void json_buffer_append(JsonBuffer *buf, const char *text) {
  size_t len = strlen(text);
  json_buffer_reserve(buf, len);
  if (!buf->ok) {
    return;
  }
  memcpy(buf->data + buf->length, text, len);
  buf->length += len;
  buf->data[buf->length] = '\0';
}

/* Appends value as a JSON string literal, including the surrounding
 * quotes, escaping '"', '\\', and control characters so arbitrary
 * piece/interpretation data can never produce invalid JSON. */
static void json_buffer_append_string(JsonBuffer *buf, const char *value) {
  json_buffer_reserve(buf, strlen(value) + 2);
  if (!buf->ok) {
    return;
  }
  json_buffer_append(buf, "\"");
  for (const unsigned char *c = (const unsigned char *)value; *c; c++) {
    switch (*c) {
      case '"':
        json_buffer_append(buf, "\\\"");
        break;
      case '\\':
        json_buffer_append(buf, "\\\\");
        break;
      case '\n':
        json_buffer_append(buf, "\\n");
        break;
      case '\r':
        json_buffer_append(buf, "\\r");
        break;
      case '\t':
        json_buffer_append(buf, "\\t");
        break;
      default:
        if (*c < 0x20) {
          char escaped[8];
          snprintf(escaped, sizeof(escaped), "\\u%04x", *c);
          json_buffer_append(buf, escaped);
        } else {
          char one[2] = {(char)*c, '\0'};
          json_buffer_append(buf, one);
        }
        break;
    }
  }
  json_buffer_append(buf, "\"");
}

static void json_buffer_destroy(JsonBuffer *buf) {
  free(buf->data);
  buf->data = NULL;
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

static const char *kPieceDetailPrefix = "/api/analyzer/pieces/";

static int handle_get_piece(struct mg_connection *conn, const char *id) {
  UrsatzGuiPieceDetail *detail = NULL;
  UrsatzGuiAnalysisResult result = ursatz_gui_analysis_get_piece(kPiecesDir, id, &detail);

  if (result == URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND) {
    send_json(conn, "{\"ok\":false,\"message\":\"piece not found\"}");
    return 200;
  }
  if (result != URSATZ_GUI_ANALYSIS_OK) {
    send_json(conn, "{\"ok\":false,\"message\":\"failed to load piece\"}");
    return 200;
  }

  JsonBuffer buf;
  json_buffer_init(&buf);

  json_buffer_append(&buf, "{\"ok\":true,\"id\":");
  json_buffer_append_string(&buf, detail->id);
  json_buffer_append(&buf, ",\"note_event_count\":");
  char count_text[32];
  snprintf(count_text, sizeof(count_text), "%zu", detail->note_event_count);
  json_buffer_append(&buf, count_text);
  json_buffer_append(&buf, ",\"interpretations\":[");

  for (size_t i = 0; i < detail->interpretation_count; i++) {
    const UrsatzGuiInterpretationSummary *summary = &detail->interpretations[i];
    if (i != 0) {
      json_buffer_append(&buf, ",");
    }
    json_buffer_append(&buf, "{\"interpretation_id\":");
    json_buffer_append_string(&buf, summary->interpretation_id);
    json_buffer_append(&buf, ",\"type\":");
    json_buffer_append_string(&buf, summary->type);
    json_buffer_append(&buf, ",\"label\":");
    json_buffer_append_string(&buf, summary->label);
    json_buffer_append(&buf, ",\"produced_by\":{\"author\":");
    json_buffer_append_string(&buf, summary->produced_by_author);
    json_buffer_append(&buf, ",\"method\":");
    json_buffer_append_string(&buf, summary->produced_by_method);
    json_buffer_append(&buf, "},\"provenance_state\":");
    json_buffer_append_string(&buf, summary->provenance_state);
    json_buffer_append(&buf, "}");
  }
  json_buffer_append(&buf, "]}");

  ursatz_gui_piece_detail_destroy(detail);

  if (!buf.ok) {
    json_buffer_destroy(&buf);
    send_json(conn, "{\"ok\":false,\"message\":\"out of memory\"}");
    return 200;
  }

  send_json(conn, buf.data);
  json_buffer_destroy(&buf);
  return 200;
}

/*
 * civetweb routes both the exact "/api/analyzer/pieces" URI and any
 * "/api/analyzer/pieces/<something>" URI to a single handler registered
 * at "/api/analyzer/pieces" (its request-matching tries an exact match,
 * then a "registered-uri/something" partial match, before ever trying
 * wildcard patterns -- so a separate "/api/analyzer/pieces/*"
 * registration would never actually be reached for a subpath GET).
 * This handler itself distinguishes the two cases by local_uri.
 */
static int pieces_handler(struct mg_connection *conn, void *cbdata) {
  (void)cbdata;
  const struct mg_request_info *info = mg_get_request_info(conn);
  const char *uri = info->local_uri;
  size_t prefix_len = strlen(kPieceDetailPrefix);

  if (strncmp(uri, kPieceDetailPrefix, prefix_len) == 0 && uri[prefix_len] != '\0') {
    if (strcmp(info->request_method, "GET") != 0) {
      mg_send_http_error(conn, 405, "Method Not Allowed");
      return 405;
    }
    return handle_get_piece(conn, uri + prefix_len);
  }

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
