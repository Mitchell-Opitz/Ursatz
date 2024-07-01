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

/* Sends body as application/json with a non-200 status line. */
static int send_json_status(struct mg_connection *conn, int status, const char *reason, const char *body) {
  size_t len = strlen(body);
  mg_printf(conn,
            "HTTP/1.1 %d %s\r\nContent-Type: application/json\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n",
            status, reason, len);
  mg_write(conn, body, len);
  return status;
}

/* Raw MusicXmlExportResult enum name, for the API's "code" field -- kept
 * unshortened so it is directly greppable against musicxml_export.h. */
static const char *musicxml_export_error_code(MusicXmlExportResult error) {
  switch (error) {
    case MUSICXML_EXPORT_ERR_NULL_ARG:
      return "MUSICXML_EXPORT_ERR_NULL_ARG";
    case MUSICXML_EXPORT_ERR_LOAD_FAILED:
      return "MUSICXML_EXPORT_ERR_LOAD_FAILED";
    case MUSICXML_EXPORT_ERR_MISSING_SPELLING:
      return "MUSICXML_EXPORT_ERR_MISSING_SPELLING";
    case MUSICXML_EXPORT_ERR_OVERLAPPING_NOTES:
      return "MUSICXML_EXPORT_ERR_OVERLAPPING_NOTES";
    case MUSICXML_EXPORT_ERR_ALLOC:
      return "MUSICXML_EXPORT_ERR_ALLOC";
    case MUSICXML_EXPORT_OK:
    default:
      return "MUSICXML_EXPORT_UNKNOWN";
  }
}

static void send_musicxml_export_error(struct mg_connection *conn, MusicXmlExportResult export_error) {
  char body[160];
  snprintf(body, sizeof(body), "{\"ok\":false,\"message\":\"failed to export piece\",\"code\":\"%s\"}",
           musicxml_export_error_code(export_error));
  send_json(conn, body);
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

static char *empty_string_copy(void) {
  char *copy = malloc(1);
  if (copy) {
    copy[0] = '\0';
  }
  return copy;
}

/* Reads the X-Filename request header (URL-encoded by the frontend
 * since header values can't safely carry arbitrary filename bytes) and
 * decodes it into a newly allocated, caller-owned string. Returns an
 * allocated empty string if the header is absent, NULL only on
 * allocation failure. */
static char *read_filename_header(struct mg_connection *conn) {
  const char *raw = mg_get_header(conn, "X-Filename");
  if (!raw) {
    return empty_string_copy();
  }
  size_t raw_len = strlen(raw);
  char *decoded = malloc(raw_len + 1);
  if (!decoded) {
    return NULL;
  }
  int decoded_len = mg_url_decode(raw, (int)raw_len, decoded, (int)(raw_len + 1), 0);
  if (decoded_len < 0) {
    free(decoded);
    return empty_string_copy();
  }
  decoded[decoded_len] = '\0';
  return decoded;
}

static int handle_post(struct mg_connection *conn) {
  size_t body_size = 0;
  unsigned char *body_data = read_request_body(conn, &body_size);
  if (!body_data) {
    send_json(conn, "{\"ok\":false,\"message\":\"failed to read request body\"}");
    return 400;
  }

  char *filename = read_filename_header(conn);
  if (!filename) {
    free(body_data);
    send_json(conn, "{\"ok\":false,\"message\":\"out of memory\"}");
    return 200;
  }

  char *id = NULL;
  UrsatzGuiAnalysisResult result =
      ursatz_gui_analysis_import_and_save(kPiecesDir, body_data, body_size, filename, &id);
  free(body_data);

  if (result == URSATZ_GUI_ANALYSIS_ERR_UNSUPPORTED_FORMAT) {
    free(filename);
    return send_json_status(conn, 415, "Unsupported Media Type", "{\"error\": \"unsupported format\"}");
  }
  if (result == URSATZ_GUI_ANALYSIS_ERR_IMPORT_FAILED) {
    free(filename);
    return send_json_status(conn, 422, "Unprocessable Entity", "{\"error\": \"import failed\"}");
  }
  if (result != URSATZ_GUI_ANALYSIS_OK) {
    free(filename);
    send_json(conn, "{\"ok\":false,\"message\":\"failed to save piece\"}");
    return 200;
  }

  JsonBuffer buf;
  json_buffer_init(&buf);
  json_buffer_append(&buf, "{\"ok\":true,\"id\":");
  json_buffer_append_string(&buf, id);
  json_buffer_append(&buf, ",\"filename\":");
  json_buffer_append_string(&buf, filename);
  json_buffer_append(&buf, "}");
  free(id);
  free(filename);

  if (!buf.ok) {
    json_buffer_destroy(&buf);
    send_json(conn, "{\"ok\":false,\"message\":\"out of memory\"}");
    return 200;
  }

  send_json(conn, buf.data);
  json_buffer_destroy(&buf);
  return 200;
}

static void free_pieces(UrsatzGuiPieceSummary *pieces, size_t count) {
  for (size_t i = 0; i < count; i++) {
    free(pieces[i].id);
    free(pieces[i].filename);
  }
  free(pieces);
}

static int handle_get(struct mg_connection *conn) {
  UrsatzGuiPieceSummary *pieces = NULL;
  size_t count = 0;
  UrsatzGuiAnalysisResult result = ursatz_gui_analysis_list(kPiecesDir, &pieces, &count);

  if (result != URSATZ_GUI_ANALYSIS_OK) {
    send_json(conn, "{\"ok\":false,\"message\":\"failed to list pieces\"}");
    return 200;
  }

  JsonBuffer buf;
  json_buffer_init(&buf);
  json_buffer_append(&buf, "{\"ok\":true,\"pieces\":[");
  for (size_t i = 0; i < count; i++) {
    if (i != 0) {
      json_buffer_append(&buf, ",");
    }
    json_buffer_append(&buf, "{\"id\":");
    json_buffer_append_string(&buf, pieces[i].id);
    json_buffer_append(&buf, ",\"filename\":");
    json_buffer_append_string(&buf, pieces[i].filename);
    json_buffer_append(&buf, "}");
  }
  json_buffer_append(&buf, "]}");
  free_pieces(pieces, count);

  if (!buf.ok) {
    json_buffer_destroy(&buf);
    send_json(conn, "{\"ok\":false,\"message\":\"out of memory\"}");
    return 200;
  }

  send_json(conn, buf.data);
  json_buffer_destroy(&buf);
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
  json_buffer_append(&buf, ",\"filename\":");
  json_buffer_append_string(&buf, detail->filename);
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
    json_buffer_append(&buf, ",\"highlighted_note_ids\":[");
    for (size_t j = 0; j < summary->highlighted_note_id_count; j++) {
      if (j != 0) {
        json_buffer_append(&buf, ",");
      }
      json_buffer_append_string(&buf, summary->highlighted_note_ids[j]);
    }
    json_buffer_append(&buf, "]}");
  }
  json_buffer_append(&buf, "],\"relationships\":[");

  for (size_t i = 0; i < detail->relationship_count; i++) {
    const UrsatzGuiRelationshipSummary *relationship = &detail->relationships[i];
    if (i != 0) {
      json_buffer_append(&buf, ",");
    }
    json_buffer_append(&buf, "{\"relationship_id\":");
    json_buffer_append_string(&buf, relationship->relationship_id);
    json_buffer_append(&buf, ",\"source_id\":");
    json_buffer_append_string(&buf, relationship->source_id);
    json_buffer_append(&buf, ",\"predicate_name\":");
    json_buffer_append_string(&buf, relationship->predicate_name);
    json_buffer_append(&buf, ",\"target_id\":");
    json_buffer_append_string(&buf, relationship->target_id);
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

/* Extracts the string value of a top-level "field":"value" pair from a
 * flat JSON object body. Minimal by design (no nesting, no escapes) --
 * mirrors corpus.c's extract_json_string_field; this route's PATCH body
 * is the same kind of flat single-field object. Returns a newly
 * allocated, caller-owned string, or NULL if the field is absent or
 * malformed. */
static char *extract_json_string_field(const char *body, const char *field) {
  char needle[64];
  snprintf(needle, sizeof(needle), "\"%s\"", field);

  const char *key = strstr(body, needle);
  if (!key) {
    return NULL;
  }

  const char *colon = strchr(key + strlen(needle), ':');
  if (!colon) {
    return NULL;
  }

  const char *quote_start = strchr(colon, '"');
  if (!quote_start) {
    return NULL;
  }
  quote_start++;

  const char *quote_end = strchr(quote_start, '"');
  if (!quote_end) {
    return NULL;
  }

  size_t len = (size_t)(quote_end - quote_start);
  char *value = malloc(len + 1);
  if (!value) {
    return NULL;
  }
  memcpy(value, quote_start, len);
  value[len] = '\0';
  return value;
}

static char *read_patch_body(struct mg_connection *conn) {
  size_t capacity = 4096;
  char *buffer = malloc(capacity);
  if (!buffer) {
    return NULL;
  }

  size_t total = 0;
  for (;;) {
    if (total == capacity) {
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

static int handle_patch_piece(struct mg_connection *conn, const char *id) {
  char *body = read_patch_body(conn);
  if (!body) {
    send_json(conn, "{\"ok\":false,\"message\":\"failed to read request body\"}");
    return 200;
  }

  char *filename = extract_json_string_field(body, "filename");
  free(body);

  if (!filename || filename[0] == '\0') {
    free(filename);
    send_json(conn, "{\"ok\":false,\"message\":\"invalid filename\"}");
    return 200;
  }

  UrsatzGuiAnalysisResult result = ursatz_gui_analysis_rename_piece(kPiecesDir, id, filename);

  if (result == URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND) {
    free(filename);
    mg_send_http_error(conn, 404, "{\"ok\":false,\"message\":\"piece not found\"}");
    return 404;
  }
  if (result != URSATZ_GUI_ANALYSIS_OK) {
    free(filename);
    send_json(conn, "{\"ok\":false,\"message\":\"failed to rename piece\"}");
    return 200;
  }

  JsonBuffer buf;
  json_buffer_init(&buf);
  json_buffer_append(&buf, "{\"ok\":true,\"id\":");
  json_buffer_append_string(&buf, id);
  json_buffer_append(&buf, ",\"filename\":");
  json_buffer_append_string(&buf, filename);
  json_buffer_append(&buf, "}");
  free(filename);

  if (!buf.ok) {
    json_buffer_destroy(&buf);
    send_json(conn, "{\"ok\":false,\"message\":\"out of memory\"}");
    return 200;
  }

  send_json(conn, buf.data);
  json_buffer_destroy(&buf);
  return 200;
}

static int handle_delete_piece(struct mg_connection *conn, const char *id) {
  UrsatzGuiAnalysisResult result = ursatz_gui_analysis_delete_piece(kPiecesDir, id);

  if (result == URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND) {
    mg_send_http_error(conn, 404, "{\"ok\":false,\"message\":\"piece not found\"}");
    return 404;
  }
  if (result != URSATZ_GUI_ANALYSIS_OK) {
    send_json(conn, "{\"ok\":false,\"message\":\"failed to delete piece\"}");
    return 200;
  }

  send_json(conn, "{\"ok\":true}");
  return 200;
}

static const char *kMusicXmlSuffix = "/musicxml";
static const char *kMusicXmlMetaSuffix = "/musicxml/meta";

static int handle_get_piece_musicxml(struct mg_connection *conn, const char *id) {
  char *xml = NULL;
  bool cosmetic_meter = false;
  MusicXmlExportResult export_error = MUSICXML_EXPORT_OK;
  UrsatzGuiAnalysisResult result =
      ursatz_gui_analysis_get_piece_musicxml(kPiecesDir, id, &xml, &cosmetic_meter, &export_error);

  if (result == URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND) {
    mg_send_http_error(conn, 404, "{\"ok\":false,\"message\":\"piece not found\"}");
    return 404;
  }
  if (result != URSATZ_GUI_ANALYSIS_OK) {
    send_musicxml_export_error(conn, export_error);
    return 200;
  }

  size_t len = strlen(xml);
  mg_send_http_ok(conn, "application/xml", (long)len);
  mg_write(conn, xml, len);
  free(xml);
  return 200;
}

static int handle_get_piece_musicxml_meta(struct mg_connection *conn, const char *id) {
  char *xml = NULL;
  bool cosmetic_meter = false;
  MusicXmlExportResult export_error = MUSICXML_EXPORT_OK;
  UrsatzGuiAnalysisResult result =
      ursatz_gui_analysis_get_piece_musicxml(kPiecesDir, id, &xml, &cosmetic_meter, &export_error);

  if (result == URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND) {
    mg_send_http_error(conn, 404, "{\"ok\":false,\"message\":\"piece not found\"}");
    return 404;
  }
  if (result != URSATZ_GUI_ANALYSIS_OK) {
    send_musicxml_export_error(conn, export_error);
    return 200;
  }
  free(xml);

  send_json(conn, cosmetic_meter ? "{\"ok\":true,\"cosmetic_meter\":true}" : "{\"ok\":true,\"cosmetic_meter\":false}");
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
    const char *rest = uri + prefix_len;
    size_t rest_len = strlen(rest);
    size_t meta_suffix_len = strlen(kMusicXmlMetaSuffix);
    size_t xml_suffix_len = strlen(kMusicXmlSuffix);

    if (strcmp(info->request_method, "GET") == 0 && rest_len > meta_suffix_len &&
        strcmp(rest + rest_len - meta_suffix_len, kMusicXmlMetaSuffix) == 0) {
      char id[256];
      size_t id_len = rest_len - meta_suffix_len;
      if (id_len >= sizeof(id)) {
        mg_send_http_error(conn, 404, "{\"ok\":false,\"message\":\"piece not found\"}");
        return 404;
      }
      memcpy(id, rest, id_len);
      id[id_len] = '\0';
      return handle_get_piece_musicxml_meta(conn, id);
    }

    if (strcmp(info->request_method, "GET") == 0 && rest_len > xml_suffix_len &&
        strcmp(rest + rest_len - xml_suffix_len, kMusicXmlSuffix) == 0) {
      char id[256];
      size_t id_len = rest_len - xml_suffix_len;
      if (id_len >= sizeof(id)) {
        mg_send_http_error(conn, 404, "{\"ok\":false,\"message\":\"piece not found\"}");
        return 404;
      }
      memcpy(id, rest, id_len);
      id[id_len] = '\0';
      return handle_get_piece_musicxml(conn, id);
    }

    if (strcmp(info->request_method, "GET") == 0) {
      return handle_get_piece(conn, uri + prefix_len);
    }
    if (strcmp(info->request_method, "DELETE") == 0) {
      return handle_delete_piece(conn, uri + prefix_len);
    }
    if (strcmp(info->request_method, "PATCH") == 0) {
      return handle_patch_piece(conn, uri + prefix_len);
    }
    mg_send_http_error(conn, 405, "Method Not Allowed");
    return 405;
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
