#include "analyzer.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <civetweb.h>
#include <cJSON.h>

#include "route_util.h"
#include "../service/analysis_service.h"
#include "../service/collection_service.h"
#include "../service/piece_metadata.h"
#include "../service/subject_service.h"
#include "materials.h"

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

static void send_musicxml_export_error(struct mg_connection *conn, const char *message) {
  char body[192];
  snprintf(body, sizeof(body), "{\"ok\":false,\"message\":\"failed to export piece\",\"code\":\"%s\"}",
           message[0] != '\0' ? message : "ANALYZER_ERR_LIBRARY");
  ursatz_gui_send_json_status(conn, 500, "Internal Server Error", body);
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
    ursatz_gui_send_json(conn, "{\"ok\":false,\"message\":\"failed to read request body\"}");
    return 400;
  }

  char *filename = read_filename_header(conn);
  if (!filename) {
    free(body_data);
    ursatz_gui_send_json(conn, "{\"ok\":false,\"message\":\"out of memory\"}");
    return 200;
  }

  char *id = NULL;
  UrsatzGuiAnalysisResult result =
      ursatz_gui_analysis_import_and_save(kPiecesDir, body_data, body_size, filename, &id);
  free(body_data);

  if (result == URSATZ_GUI_ANALYSIS_ERR_UNSUPPORTED_FORMAT) {
    free(filename);
    return ursatz_gui_send_json_status(conn, 415, "Unsupported Media Type", "{\"error\": \"unsupported format\"}");
  }
  if (result == URSATZ_GUI_ANALYSIS_ERR_IMPORT_FAILED) {
    free(filename);
    return ursatz_gui_send_json_status(conn, 422, "Unprocessable Entity", "{\"error\": \"import failed\"}");
  }
  if (result != URSATZ_GUI_ANALYSIS_OK) {
    free(filename);
    ursatz_gui_send_json(conn, "{\"ok\":false,\"message\":\"failed to save piece\"}");
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
    ursatz_gui_send_json(conn, "{\"ok\":false,\"message\":\"out of memory\"}");
    return 200;
  }

  ursatz_gui_send_json(conn, buf.data);
  json_buffer_destroy(&buf);
  return 200;
}

static void free_pieces(UrsatzGuiPieceSummary *pieces, size_t count) {
  for (size_t i = 0; i < count; i++) {
    free(pieces[i].id);
    free(pieces[i].filename);
    free(pieces[i].format);
    free(pieces[i].uploaded_at);
    free(pieces[i].composer);
    free(pieces[i].source);
    ursatz_gui_meta_tags_free(pieces[i].tags, pieces[i].tag_count);
  }
  free(pieces);
}

/* Appends a piece's notation override as JSON: null when it has none. */
static void append_notation(JsonBuffer *buf, bool has_notation, const UrsatzGuiNotation *notation) {
  if (!has_notation) {
    json_buffer_append(buf, "null");
    return;
  }
  json_buffer_append(buf, "{\"meter\":");
  json_buffer_append_string(buf, notation->meter);
  json_buffer_append(buf, ",\"value_scale\":");
  json_buffer_append_string(buf, notation->value_scale);
  json_buffer_append(buf, ",\"pickup\":");
  json_buffer_append_string(buf, notation->pickup);
  json_buffer_append(buf, "}");
}

/* Appends one piece as the list route and the PATCH answer show it. */
static void append_piece_entry(JsonBuffer *buf, const UrsatzGuiPieceSummary *piece) {
  json_buffer_append(buf, "{\"id\":");
  json_buffer_append_string(buf, piece->id);
  json_buffer_append(buf, ",\"filename\":");
  json_buffer_append_string(buf, piece->filename);
  json_buffer_append(buf, ",\"format\":");
  json_buffer_append_string(buf, piece->format);
  json_buffer_append(buf, ",\"uploaded_at\":");
  json_buffer_append_string(buf, piece->uploaded_at);
  json_buffer_append(buf, ",\"composer\":");
  json_buffer_append_string(buf, piece->composer);
  json_buffer_append(buf, ",\"source\":");
  json_buffer_append_string(buf, piece->source);
  json_buffer_append(buf, ",\"tags\":[");
  for (size_t i = 0; i < piece->tag_count; i++) {
    if (i != 0) {
      json_buffer_append(buf, ",");
    }
    json_buffer_append_string(buf, piece->tags[i]);
  }
  json_buffer_append(buf, "],\"notation\":");
  append_notation(buf, piece->has_notation, &piece->notation);
  json_buffer_append(buf, ",\"analysis_status\":");
  json_buffer_append_string(buf, piece->analysis_status ? piece->analysis_status : "unreadable");
  json_buffer_append(buf, "}");
}

/* Reads one query parameter (URL-decoded) into out; false when absent or empty. */
static bool query_value(const struct mg_request_info *info, const char *name, int occurrence, char *out,
                        size_t out_size) {
  if (!info->query_string) {
    return false;
  }
  int length = mg_get_var2(info->query_string, strlen(info->query_string), name, out, out_size, (size_t)occurrence);
  return length > 0;
}

/* Builds the metadata filter from composer, source, tag, unassigned (composer
 * or source, may repeat) and untagged=1. The strings live in the buffers. */
static void read_filter(const struct mg_request_info *info, UrsatzGuiMetaFilter *filter, char *composer,
                        char *source, char *tag, size_t size) {
  memset(filter, 0, sizeof(*filter));
  filter->composer = query_value(info, "composer", 0, composer, size) ? composer : NULL;
  filter->source = query_value(info, "source", 0, source, size) ? source : NULL;
  filter->tag = query_value(info, "tag", 0, tag, size) ? tag : NULL;
  for (int occurrence = 0; occurrence < 2; occurrence++) {
    char which[16];
    if (!query_value(info, "unassigned", occurrence, which, sizeof(which))) {
      continue;
    }
    if (strcmp(which, "composer") == 0) {
      filter->composer_unassigned = true;
    } else if (strcmp(which, "source") == 0) {
      filter->source_unassigned = true;
    }
  }
  char untagged[8];
  filter->untagged = query_value(info, "untagged", 0, untagged, sizeof(untagged)) && strcmp(untagged, "1") == 0;
}

static int handle_get(struct mg_connection *conn) {
  UrsatzGuiPieceSummary *pieces = NULL;
  size_t count = 0;
  UrsatzGuiAnalysisResult result = ursatz_gui_analysis_list(kPiecesDir, &pieces, &count);

  if (result != URSATZ_GUI_ANALYSIS_OK) {
    ursatz_gui_send_json(conn, "{\"ok\":false,\"message\":\"failed to list pieces\"}");
    return 200;
  }

  char composer[256], source[256], tag[256];
  UrsatzGuiMetaFilter filter;
  read_filter(mg_get_request_info(conn), &filter, composer, source, tag, sizeof(composer));

  JsonBuffer buf;
  json_buffer_init(&buf);
  json_buffer_append(&buf, "{\"ok\":true,\"pieces\":[");
  bool first = true;
  for (size_t i = 0; i < count; i++) {
    if (!ursatz_gui_meta_matches(&pieces[i], &filter)) {
      continue;
    }
    if (!first) {
      json_buffer_append(&buf, ",");
    }
    first = false;
    append_piece_entry(&buf, &pieces[i]);
  }
  json_buffer_append(&buf, "]}");
  free_pieces(pieces, count);

  if (!buf.ok) {
    json_buffer_destroy(&buf);
    ursatz_gui_send_json(conn, "{\"ok\":false,\"message\":\"out of memory\"}");
    return 200;
  }

  ursatz_gui_send_json(conn, buf.data);
  json_buffer_destroy(&buf);
  return 200;
}

static void append_group_array(JsonBuffer *buf, const char *name, const UrsatzGuiMetaGroup *groups, size_t count) {
  json_buffer_append(buf, "\"");
  json_buffer_append(buf, name);
  json_buffer_append(buf, "\":[");
  for (size_t i = 0; i < count; i++) {
    char number[32];
    if (i != 0) {
      json_buffer_append(buf, ",");
    }
    json_buffer_append(buf, "{\"value\":");
    json_buffer_append_string(buf, groups[i].value);
    snprintf(number, sizeof(number), ",\"count\":%zu}", groups[i].count);
    json_buffer_append(buf, number);
  }
  json_buffer_append(buf, "],");
}

/* GET /api/analyzer/groups: the distinct composers, sources and tags with piece counts. */
static int handle_get_groups(struct mg_connection *conn) {
  UrsatzGuiPieceSummary *pieces = NULL;
  size_t count = 0;
  if (ursatz_gui_analysis_list(kPiecesDir, &pieces, &count) != URSATZ_GUI_ANALYSIS_OK) {
    ursatz_gui_send_json(conn, "{\"ok\":false,\"message\":\"failed to list pieces\"}");
    return 200;
  }
  UrsatzGuiMetaGroups groups;
  UrsatzGuiMetaResult grouped = ursatz_gui_meta_group(pieces, count, &groups);
  free_pieces(pieces, count);
  if (grouped != URSATZ_GUI_META_OK) {
    ursatz_gui_send_json(conn, "{\"ok\":false,\"message\":\"out of memory\"}");
    return 200;
  }

  JsonBuffer buf;
  json_buffer_init(&buf);
  json_buffer_append(&buf, "{\"ok\":true,");
  append_group_array(&buf, "composers", groups.composers, groups.composer_count);
  append_group_array(&buf, "sources", groups.sources, groups.source_count);
  append_group_array(&buf, "tags", groups.tags, groups.tag_count);
  char tail[128];
  snprintf(tail, sizeof(tail), "\"unassigned\":{\"composer\":%zu,\"source\":%zu},\"untagged\":%zu,\"total\":%zu}",
           groups.unassigned_composer, groups.unassigned_source, groups.untagged, groups.total);
  json_buffer_append(&buf, tail);
  ursatz_gui_meta_groups_destroy(&groups);

  if (!buf.ok) {
    json_buffer_destroy(&buf);
    ursatz_gui_send_json(conn, "{\"ok\":false,\"message\":\"out of memory\"}");
    return 200;
  }
  ursatz_gui_send_json(conn, buf.data);
  json_buffer_destroy(&buf);
  return 200;
}

static int groups_handler(struct mg_connection *conn, void *cbdata) {
  (void)cbdata;
  const struct mg_request_info *info = mg_get_request_info(conn);
  if (strcmp(info->request_method, "GET") == 0) {
    return handle_get_groups(conn);
  }
  mg_send_http_error(conn, 405, "Method Not Allowed");
  return 405;
}

static const char *kPieceDetailPrefix = "/api/analyzer/pieces/";

/* The members of the document a summary keeps: what the header and the Provenance list read. */
static const char *const kSummarySections[] = {"schema_version", "metadata", "meter"};

static int handle_get_piece(struct mg_connection *conn, const char *id) {
  char document_mode[16];
  bool summary = query_value(mg_get_request_info(conn), "document", 0, document_mode, sizeof(document_mode)) &&
                 strcmp(document_mode, "summary") == 0;
  UrsatzGuiPieceDetail *detail = NULL;
  UrsatzGuiAnalysisResult result = ursatz_gui_analysis_get_piece(kPiecesDir, id, &detail);

  if (result == URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND) {
    ursatz_gui_send_json(conn, "{\"ok\":false,\"message\":\"piece not found\"}");
    return 200;
  }
  if (result != URSATZ_GUI_ANALYSIS_OK) {
    ursatz_gui_send_json(conn, "{\"ok\":false,\"message\":\"failed to load piece\"}");
    return 200;
  }

  JsonBuffer buf;
  json_buffer_init(&buf);

  json_buffer_append(&buf, "{\"ok\":true,\"id\":");
  json_buffer_append_string(&buf, detail->id);
  json_buffer_append(&buf, ",\"filename\":");
  json_buffer_append_string(&buf, detail->filename);
  json_buffer_append(&buf, ",\"format\":");
  json_buffer_append_string(&buf, detail->format);
  json_buffer_append(&buf, ",\"uploaded_at\":");
  json_buffer_append_string(&buf, detail->uploaded_at);
  json_buffer_append(&buf, ",\"notation\":");
  append_notation(&buf, detail->has_notation, &detail->notation);
  json_buffer_append(&buf, ",\"analysis_status\":");
  json_buffer_append_string(&buf, detail->analysis_status ? detail->analysis_status : "unreadable");
  if (detail->analysis_status && strcmp(detail->analysis_status, "current") != 0) {
    /* The stored document is served as it is: a re-analysis failed (or the piece was written by a build without
       this check); the status names why it is stale. */
    json_buffer_append(&buf, ",\"stale\":");
    json_buffer_append_string(&buf, detail->analysis_status);
  }
  json_buffer_append(&buf, ",\"note_xml_ids\":[");
  for (size_t i = 0; i < detail->note_count; i++) {
    if (i != 0) {
      json_buffer_append(&buf, ",");
    }
    json_buffer_append_string(&buf, detail->note_xml_ids[i]);
  }
  /* The stored document is already JSON; it is inlined as is, or as its summary with the names of all its
     sections when ?document=summary asks for it. */
  json_buffer_append(&buf, "],\"document\":");
  if (summary) {
    char *summary_json = NULL;
    char *names_json = NULL;
    if (!ursatz_gui_document_subset(detail->document_json, kSummarySections,
                                    sizeof(kSummarySections) / sizeof(kSummarySections[0]), &summary_json,
                                    &names_json)) {
      ursatz_gui_piece_detail_destroy(detail);
      json_buffer_destroy(&buf);
      ursatz_gui_send_json(conn, "{\"ok\":false,\"message\":\"failed to load piece\"}");
      return 200;
    }
    json_buffer_append(&buf, summary_json);
    json_buffer_append(&buf, ",\"document_sections\":");
    json_buffer_append(&buf, names_json);
    free(summary_json);
    free(names_json);
  } else {
    json_buffer_append(&buf, detail->document_json);
  }

  /* The subjects that link this piece (R-117). */
  cJSON *subjects = ursatz_gui_materials_subjects_for("pieces", detail->id);
  char *subjects_text = subjects ? cJSON_PrintUnformatted(subjects) : NULL;
  cJSON_Delete(subjects);
  json_buffer_append(&buf, ",\"subjects\":");
  json_buffer_append(&buf, subjects_text ? subjects_text : "[]");
  free(subjects_text);
  json_buffer_append(&buf, "}");

  ursatz_gui_piece_detail_destroy(detail);

  if (!buf.ok) {
    json_buffer_destroy(&buf);
    ursatz_gui_send_json(conn, "{\"ok\":false,\"message\":\"out of memory\"}");
    return 200;
  }

  ursatz_gui_send_json(conn, buf.data);
  json_buffer_destroy(&buf);
  return 200;
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

/* Points *out at the strings of a JSON array of strings (valid while the
 * array is). False when item is not an array of strings. */
static bool string_array_members(const cJSON *item, const char ***out, size_t *out_count) {
  *out = NULL;
  *out_count = 0;
  if (!cJSON_IsArray(item)) {
    return false;
  }
  int size = cJSON_GetArraySize(item);
  const char **members = size > 0 ? calloc((size_t)size, sizeof(char *)) : NULL;
  if (size > 0 && !members) {
    return false;
  }
  size_t count = 0;
  const cJSON *member = NULL;
  cJSON_ArrayForEach(member, item) {
    if (!cJSON_IsString(member) || !member->valuestring) {
      free(members);
      return false;
    }
    members[count++] = member->valuestring;
  }
  *out = members;
  *out_count = count;
  return true;
}

static int send_invalid_member(struct mg_connection *conn, const char *member) {
  char body[96];
  snprintf(body, sizeof(body), "{\"ok\":false,\"message\":\"invalid %s\"}", member);
  return ursatz_gui_send_json_status(conn, 400, "Bad Request", body);
}

/* Answers a successful metadata change with the piece as the list shows it. */
static int send_updated_piece(struct mg_connection *conn, const char *id) {
  UrsatzGuiPieceSummary *pieces = NULL;
  size_t count = 0;
  if (ursatz_gui_analysis_list(kPiecesDir, &pieces, &count) != URSATZ_GUI_ANALYSIS_OK) {
    ursatz_gui_send_json(conn, "{\"ok\":false,\"message\":\"failed to list pieces\"}");
    return 200;
  }
  JsonBuffer buf;
  json_buffer_init(&buf);
  json_buffer_append(&buf, "{\"ok\":true,\"piece\":");
  bool found = false;
  for (size_t i = 0; i < count && !found; i++) {
    if (strcmp(pieces[i].id, id) == 0) {
      append_piece_entry(&buf, &pieces[i]);
      found = true;
    }
  }
  json_buffer_append(&buf, found ? "}" : "null}");
  free_pieces(pieces, count);

  if (!buf.ok) {
    json_buffer_destroy(&buf);
    ursatz_gui_send_json(conn, "{\"ok\":false,\"message\":\"out of memory\"}");
    return 200;
  }
  ursatz_gui_send_json(conn, buf.data);
  json_buffer_destroy(&buf);
  return 200;
}

/* Reads a PATCH "notation" member: an object whose meter, value_scale and
 * pickup are strings (each optional, absent = not set). False for anything else. */
static bool read_notation_member(const cJSON *item, UrsatzGuiNotation *out) {
  static const struct {
    const char *name;
    size_t offset;
    size_t size;
  } kMembers[] = {
      {"meter", offsetof(UrsatzGuiNotation, meter), sizeof(((UrsatzGuiNotation *)0)->meter)},
      {"value_scale", offsetof(UrsatzGuiNotation, value_scale), sizeof(((UrsatzGuiNotation *)0)->value_scale)},
      {"pickup", offsetof(UrsatzGuiNotation, pickup), sizeof(((UrsatzGuiNotation *)0)->pickup)},
  };
  memset(out, 0, sizeof(*out));
  if (!cJSON_IsObject(item)) {
    return false;
  }
  for (size_t i = 0; i < sizeof(kMembers) / sizeof(kMembers[0]); i++) {
    const cJSON *member = cJSON_GetObjectItemCaseSensitive(item, kMembers[i].name);
    if (!member) {
      continue;
    }
    if (!cJSON_IsString(member) || !member->valuestring || strlen(member->valuestring) >= kMembers[i].size) {
      return false;
    }
    strcpy((char *)out + kMembers[i].offset, member->valuestring);
  }
  return true;
}

/* PATCH /api/analyzer/pieces/<id>: filename, composer, source, tags, add_tags,
 * remove_tags, notation (an object, or null to clear; all optional, unknown
 * members ignored). A changed notation analyzes the piece again before the
 * answer. */
static int handle_patch_piece(struct mg_connection *conn, const char *id) {
  char *body = read_patch_body(conn);
  if (!body) {
    ursatz_gui_send_json(conn, "{\"ok\":false,\"message\":\"failed to read request body\"}");
    return 200;
  }
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!cJSON_IsObject(root)) {
    cJSON_Delete(root);
    return ursatz_gui_send_json_status(conn, 400, "Bad Request", "{\"ok\":false,\"message\":\"invalid request body\"}");
  }

  UrsatzGuiMetaUpdate update = {0};
  const char **tags = NULL;
  const char **add_tags = NULL;
  const char **remove_tags = NULL;
  const char *bad_member = NULL;
  bool any = false;

  const cJSON *filename = cJSON_GetObjectItemCaseSensitive(root, "filename");
  const cJSON *composer = cJSON_GetObjectItemCaseSensitive(root, "composer");
  const cJSON *source = cJSON_GetObjectItemCaseSensitive(root, "source");
  const cJSON *tags_item = cJSON_GetObjectItemCaseSensitive(root, "tags");
  const cJSON *add_item = cJSON_GetObjectItemCaseSensitive(root, "add_tags");
  const cJSON *remove_item = cJSON_GetObjectItemCaseSensitive(root, "remove_tags");

  if (filename) {
    if (!cJSON_IsString(filename) || !filename->valuestring || filename->valuestring[0] == '\0') {
      cJSON_Delete(root);
      ursatz_gui_send_json(conn, "{\"ok\":false,\"message\":\"invalid filename\"}");
      return 200;
    }
    update.filename = filename->valuestring;
    any = true;
  }
  if (composer) {
    if (cJSON_IsString(composer) && composer->valuestring) {
      update.composer = composer->valuestring;
      any = true;
    } else {
      bad_member = "composer";
    }
  }
  if (!bad_member && source) {
    if (cJSON_IsString(source) && source->valuestring) {
      update.source = source->valuestring;
      any = true;
    } else {
      bad_member = "source";
    }
  }
  if (!bad_member && tags_item) {
    update.has_tags = string_array_members(tags_item, &tags, &update.tag_count);
    bad_member = update.has_tags ? NULL : "tags";
    any = any || update.has_tags;
  }
  if (!bad_member && add_item) {
    if (string_array_members(add_item, &add_tags, &update.add_tag_count)) {
      any = true;
    } else {
      bad_member = "add_tags";
    }
  }
  if (!bad_member && remove_item) {
    if (string_array_members(remove_item, &remove_tags, &update.remove_tag_count)) {
      any = true;
    } else {
      bad_member = "remove_tags";
    }
  }
  UrsatzGuiNotation notation;
  memset(&notation, 0, sizeof(notation));
  const cJSON *notation_item = cJSON_GetObjectItemCaseSensitive(root, "notation");
  if (!bad_member && notation_item) {
    if (cJSON_IsNull(notation_item)) {
      update.has_notation = true;
      any = true;
    } else if (read_notation_member(notation_item, &notation)) {
      update.has_notation = true;
      update.notation = &notation;
      any = true;
    } else {
      bad_member = "notation";
    }
  }
  update.tags = tags;
  update.add_tags = add_tags;
  update.remove_tags = remove_tags;

  int status = 200;
  if (bad_member) {
    status = send_invalid_member(conn, bad_member);
  } else if (!any) {
    ursatz_gui_send_json(conn, "{\"ok\":false,\"message\":\"nothing to update\"}");
  } else {
    char message[96] = "";
    UrsatzGuiAnalysisResult result =
        ursatz_gui_analysis_update_piece(kPiecesDir, id, &update, message, sizeof(message));
    if (result == URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND) {
      mg_send_http_error(conn, 404, "{\"ok\":false,\"message\":\"piece not found\"}");
      status = 404;
    } else if (result == URSATZ_GUI_ANALYSIS_ERR_INVALID_METADATA) {
      cJSON *answer = cJSON_CreateObject();
      char *text = NULL;
      if (answer) {
        cJSON_AddFalseToObject(answer, "ok");
        cJSON_AddStringToObject(answer, "message", message);
        text = cJSON_PrintUnformatted(answer);
        cJSON_Delete(answer);
      }
      status = ursatz_gui_send_json_status(conn, 400, "Bad Request", text ? text : "{\"ok\":false,\"message\":\"invalid metadata\"}");
      free(text);
    } else if (result != URSATZ_GUI_ANALYSIS_OK) {
      ursatz_gui_send_json(conn, "{\"ok\":false,\"message\":\"failed to update piece\"}");
    } else {
      status = send_updated_piece(conn, id);
    }
  }

  free(tags);
  free(add_tags);
  free(remove_tags);
  cJSON_Delete(root);
  return status;
}

static int handle_delete_piece(struct mg_connection *conn, const char *id) {
  UrsatzGuiAnalysisResult result = ursatz_gui_analysis_delete_piece(kPiecesDir, id);

  if (result == URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND) {
    mg_send_http_error(conn, 404, "{\"ok\":false,\"message\":\"piece not found\"}");
    return 404;
  }
  if (result != URSATZ_GUI_ANALYSIS_OK) {
    ursatz_gui_send_json(conn, "{\"ok\":false,\"message\":\"failed to delete piece\"}");
    return 200;
  }

  /* A collection never keeps an id whose piece is gone. The piece itself is
   * already deleted, so a failure here is logged, not answered. */
  if (ursatz_gui_collection_remove_piece_everywhere(id) != URSATZ_GUI_COLLECTION_OK) {
    fprintf(stderr, "analyzer: piece %s deleted, but it could not be removed from every collection\n", id);
  }

  /* The same goes for the subjects that linked the piece. */
  if (ursatz_gui_subject_forget_piece(id) != URSATZ_GUI_SUBJECT_OK) {
    fprintf(stderr, "analyzer: piece %s deleted, but it could not be removed from every subject\n", id);
  }

  ursatz_gui_send_json(conn, "{\"ok\":true}");
  return 200;
}

static const char *kReanalyzeSuffix = "/reanalyze";

static int handle_reanalyze_piece(struct mg_connection *conn, const char *id) {
  UrsatzGuiAnalysisResult result = ursatz_gui_analysis_reanalyze(kPiecesDir, id);

  if (result == URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND) {
    mg_send_http_error(conn, 404, "{\"ok\":false,\"message\":\"piece not found\"}");
    return 404;
  }
  if (result == URSATZ_GUI_ANALYSIS_ERR_IMPORT_FAILED) {
    return ursatz_gui_send_json_status(conn, 422, "Unprocessable Entity", "{\"error\": \"import failed\"}");
  }
  if (result != URSATZ_GUI_ANALYSIS_OK) {
    ursatz_gui_send_json(conn, "{\"ok\":false,\"message\":\"failed to analyze piece again\"}");
    return 200;
  }

  JsonBuffer buf;
  json_buffer_init(&buf);
  json_buffer_append(&buf, "{\"ok\":true,\"id\":");
  json_buffer_append_string(&buf, id);
  json_buffer_append(&buf, "}");
  if (!buf.ok) {
    json_buffer_destroy(&buf);
    ursatz_gui_send_json(conn, "{\"ok\":false,\"message\":\"out of memory\"}");
    return 200;
  }
  ursatz_gui_send_json(conn, buf.data);
  json_buffer_destroy(&buf);
  return 200;
}

static const char *kMusicXmlSuffix = "/musicxml";
static const char *kMusicXmlMetaSuffix = "/musicxml/meta";
static const char *kPlaybackSuffix = "/playback";
static const char *kDocumentSuffix = "/document";

#define DOCUMENT_SECTIONS_MAX 64
#define DOCUMENT_SECTION_NAME_MAX 64

/* GET /api/analyzer/pieces/<id>/document?sections=a,b: only those sections of the stored analysis document, read
   without importing the source. */
static int handle_get_piece_document(struct mg_connection *conn, const char *id) {
  char list[DOCUMENT_SECTIONS_MAX * (DOCUMENT_SECTION_NAME_MAX + 1) + 1];
  const char *bad_request = "{\"ok\":false,\"message\":\"sections must name one or more document sections\"}";
  if (!query_value(mg_get_request_info(conn), "sections", 0, list, sizeof(list))) {
    return ursatz_gui_send_json_status(conn, 400, "Bad Request", bad_request);
  }
  char names_text[DOCUMENT_SECTIONS_MAX][DOCUMENT_SECTION_NAME_MAX + 1];
  const char *names[DOCUMENT_SECTIONS_MAX];
  size_t count = 0;
  const char *cursor = list;
  while (*cursor) {
    size_t length = 0;
    while (cursor[length] && cursor[length] != ',') {
      char c = cursor[length];
      if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) {
        return ursatz_gui_send_json_status(conn, 400, "Bad Request", bad_request);
      }
      length++;
    }
    if (length == 0 || length > DOCUMENT_SECTION_NAME_MAX || count == DOCUMENT_SECTIONS_MAX) {
      return ursatz_gui_send_json_status(conn, 400, "Bad Request", bad_request);
    }
    memcpy(names_text[count], cursor, length);
    names_text[count][length] = '\0';
    names[count] = names_text[count];
    count++;
    cursor += length;
    if (*cursor == ',') {
      cursor++;
      if (!*cursor) {
        return ursatz_gui_send_json_status(conn, 400, "Bad Request", bad_request);
      }
    }
  }

  char *sections = NULL;
  UrsatzGuiAnalysisResult result = ursatz_gui_analysis_get_document(kPiecesDir, id, names, count, &sections);
  if (result == URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND) {
    mg_send_http_error(conn, 404, "{\"ok\":false,\"message\":\"piece not found\"}");
    return 404;
  }
  if (result != URSATZ_GUI_ANALYSIS_OK) {
    return ursatz_gui_send_json_status(conn, 500, "Internal Server Error",
                                       "{\"ok\":false,\"message\":\"failed to load the document\"}");
  }
  JsonBuffer buf;
  json_buffer_init(&buf);
  json_buffer_append(&buf, "{\"ok\":true,\"sections\":");
  json_buffer_append(&buf, sections);
  json_buffer_append(&buf, "}");
  free(sections);
  if (!buf.ok) {
    json_buffer_destroy(&buf);
    return ursatz_gui_send_json_status(conn, 500, "Internal Server Error", "{\"ok\":false,\"message\":\"out of memory\"}");
  }
  ursatz_gui_send_json(conn, buf.data);
  json_buffer_destroy(&buf);
  return 200;
}

static int handle_get_piece_musicxml(struct mg_connection *conn, const char *id) {
  char *xml = NULL;
  bool meter_defaulted = false;
  char export_message[256] = "";
  UrsatzGuiAnalysisResult result =
      ursatz_gui_analysis_get_piece_musicxml(kPiecesDir, id, &xml, &meter_defaulted, export_message);

  if (result == URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND) {
    mg_send_http_error(conn, 404, "{\"ok\":false,\"message\":\"piece not found\"}");
    return 404;
  }
  if (result == URSATZ_GUI_ANALYSIS_ERR_IMPORT_FAILED) {
    return ursatz_gui_send_json_status(conn, 422, "Unprocessable Entity",
                            "{\"ok\":false,\"message\":\"could not import the stored source\"}");
  }
  if (result == URSATZ_GUI_ANALYSIS_ERR_PERSISTENCE_FAILED) {
    send_musicxml_export_error(conn, export_message);
    return 500;
  }
  if (result != URSATZ_GUI_ANALYSIS_OK) {
    return ursatz_gui_send_json_status(conn, 500, "Internal Server Error", "{\"ok\":false,\"message\":\"failed to export piece\"}");
  }

  size_t len = strlen(xml);
  mg_send_http_ok(conn, "application/xml", (long)len);
  mg_write(conn, xml, len);
  free(xml);
  return 200;
}

static int handle_get_piece_musicxml_meta(struct mg_connection *conn, const char *id) {
  bool meter_defaulted = false;
  UrsatzGuiAnalysisResult result = ursatz_gui_analysis_get_piece_meter_defaulted(kPiecesDir, id, &meter_defaulted);

  if (result == URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND) {
    mg_send_http_error(conn, 404, "{\"ok\":false,\"message\":\"piece not found\"}");
    return 404;
  }
  if (result == URSATZ_GUI_ANALYSIS_ERR_IMPORT_FAILED) {
    return ursatz_gui_send_json_status(conn, 422, "Unprocessable Entity",
                            "{\"ok\":false,\"message\":\"could not import the stored source\"}");
  }
  if (result != URSATZ_GUI_ANALYSIS_OK) {
    return ursatz_gui_send_json_status(conn, 500, "Internal Server Error", "{\"ok\":false,\"message\":\"failed to export piece\"}");
  }

  ursatz_gui_send_json(conn, meter_defaulted ? "{\"ok\":true,\"meter_defaulted\":true}" : "{\"ok\":true,\"meter_defaulted\":false}");
  return 200;
}

static int handle_get_piece_playback(struct mg_connection *conn, const char *id) {
  char *json = NULL;
  UrsatzGuiAnalysisResult result = ursatz_gui_analysis_get_piece_playback(kPiecesDir, id, &json);

  if (result == URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND) {
    mg_send_http_error(conn, 404, "{\"ok\":false,\"message\":\"piece not found\"}");
    return 404;
  }
  if (result == URSATZ_GUI_ANALYSIS_ERR_IMPORT_FAILED) {
    return ursatz_gui_send_json_status(conn, 422, "Unprocessable Entity",
                            "{\"ok\":false,\"message\":\"could not import the stored source\"}");
  }
  if (result != URSATZ_GUI_ANALYSIS_OK) {
    return ursatz_gui_send_json_status(conn, 500, "Internal Server Error", "{\"ok\":false,\"message\":\"failed to build playback\"}");
  }

  ursatz_gui_send_json(conn, json);
  free(json);
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
    size_t reanalyze_suffix_len = strlen(kReanalyzeSuffix);
    size_t playback_suffix_len = strlen(kPlaybackSuffix);
    size_t document_suffix_len = strlen(kDocumentSuffix);

    if (strcmp(info->request_method, "POST") == 0 && rest_len > reanalyze_suffix_len &&
        strcmp(rest + rest_len - reanalyze_suffix_len, kReanalyzeSuffix) == 0) {
      char id[256];
      size_t id_len = rest_len - reanalyze_suffix_len;
      if (id_len >= sizeof(id)) {
        mg_send_http_error(conn, 404, "{\"ok\":false,\"message\":\"piece not found\"}");
        return 404;
      }
      memcpy(id, rest, id_len);
      id[id_len] = '\0';
      return handle_reanalyze_piece(conn, id);
    }

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

    if (strcmp(info->request_method, "GET") == 0 && rest_len > document_suffix_len &&
        strcmp(rest + rest_len - document_suffix_len, kDocumentSuffix) == 0) {
      char id[256];
      size_t id_len = rest_len - document_suffix_len;
      if (id_len >= sizeof(id)) {
        mg_send_http_error(conn, 404, "{\"ok\":false,\"message\":\"piece not found\"}");
        return 404;
      }
      memcpy(id, rest, id_len);
      id[id_len] = '\0';
      return handle_get_piece_document(conn, id);
    }

    if (strcmp(info->request_method, "GET") == 0 && rest_len > playback_suffix_len &&
        strcmp(rest + rest_len - playback_suffix_len, kPlaybackSuffix) == 0) {
      char id[256];
      size_t id_len = rest_len - playback_suffix_len;
      if (id_len >= sizeof(id)) {
        mg_send_http_error(conn, 404, "{\"ok\":false,\"message\":\"piece not found\"}");
        return 404;
      }
      memcpy(id, rest, id_len);
      id[id_len] = '\0';
      return handle_get_piece_playback(conn, id);
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
  mg_set_request_handler(ctx, "/api/analyzer/groups", groups_handler, NULL);
}
