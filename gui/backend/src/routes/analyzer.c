#include "analyzer.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <civetweb.h>
#include <cJSON.h>

#include "route_util.h"
#include "workshop_internal.h"
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

static void send_musicxml_export_error(struct mg_connection *conn, UrsatzStatus status) {
  char body[192];
  snprintf(body, sizeof(body), "{\"ok\":false,\"message\":\"failed to export piece\",\"code\":\"%s\"}",
           ursatz_status_name(status));
  ursatz_gui_send_json_status(conn, 500, "Internal Server Error", body);
}

/* Answers {"ok": true, <name>: value}; value (or the members of object, when name is NULL) is moved into the answer. */
static int send_ok_with(struct mg_connection *conn, const char *name, cJSON *value) {
  cJSON *answer = name ? cJSON_CreateObject() : value;
  if (!answer || !value || (name && (!cJSON_AddBoolToObject(answer, "ok", 1) || !cJSON_AddItemToObject(answer, name, value)))) {
    if (name) {
      cJSON_Delete(value);
    }
    cJSON_Delete(answer);
    ursatz_gui_send_json(conn, "{\"ok\":false,\"message\":\"out of memory\"}");
    return 200;
  }
  if (!name && !cJSON_AddBoolToObject(answer, "ok", 1)) {
    cJSON_Delete(answer);
    ursatz_gui_send_json(conn, "{\"ok\":false,\"message\":\"out of memory\"}");
    return 200;
  }
  return ursatz_gui_send_object(conn, 200, answer);
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
  UrsatzStatus result =
      workshop_analysis_import_and_save(kPiecesDir, body_data, body_size, filename, &id);
  free(body_data);

  if (result == URSATZ_ERR_REQUEST) {
    free(filename);
    return ursatz_gui_send_json_status(conn, 415, "Unsupported Media Type", "{\"error\": \"unsupported format\"}");
  }
  if (result == URSATZ_ERR_FORMAT) {
    free(filename);
    return ursatz_gui_send_json_status(conn, 422, "Unprocessable Entity", "{\"error\": \"import failed\"}");
  }
  if (result != URSATZ_OK) {
    free(filename);
    ursatz_gui_send_json(conn, "{\"ok\":false,\"message\":\"failed to save piece\"}");
    return 200;
  }

  cJSON *answer = cJSON_CreateObject();
  bool built = answer && cJSON_AddStringToObject(answer, "id", id) && cJSON_AddStringToObject(answer, "filename", filename);
  free(id);
  free(filename);
  if (!built) {
    cJSON_Delete(answer);
    ursatz_gui_send_json(conn, "{\"ok\":false,\"message\":\"out of memory\"}");
    return 200;
  }
  return send_ok_with(conn, NULL, answer);
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
static void read_filter(const struct mg_request_info *info, WorkshopPieceFilter *filter, char *composer,
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
  char composer[256], source[256], tag[256];
  WorkshopPieceFilter filter;
  read_filter(mg_get_request_info(conn), &filter, composer, source, tag, sizeof(composer));
  cJSON *pieces = NULL;
  UrsatzStatus result = workshop_library_list_json(kPiecesDir, &filter, &pieces);
  if (result == URSATZ_ERR_ALLOC) {
    ursatz_gui_send_json(conn, "{\"ok\":false,\"message\":\"out of memory\"}");
    return 200;
  }
  if (result != URSATZ_OK) {
    ursatz_gui_send_json(conn, "{\"ok\":false,\"message\":\"failed to list pieces\"}");
    return 200;
  }
  return send_ok_with(conn, "pieces", pieces);
}

/* GET /api/analyzer/groups: the distinct composers, sources and tags with piece counts. */
static int handle_get_groups(struct mg_connection *conn) {
  cJSON *groups = NULL;
  UrsatzStatus result = workshop_library_groups_json(kPiecesDir, &groups);
  if (result == URSATZ_ERR_ALLOC) {
    ursatz_gui_send_json(conn, "{\"ok\":false,\"message\":\"out of memory\"}");
    return 200;
  }
  if (result != URSATZ_OK) {
    ursatz_gui_send_json(conn, "{\"ok\":false,\"message\":\"failed to list pieces\"}");
    return 200;
  }
  return send_ok_with(conn, NULL, groups);
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

static int handle_get_piece(struct mg_connection *conn, const char *id) {
  char document_mode[16];
  bool summary = query_value(mg_get_request_info(conn), "document", 0, document_mode, sizeof(document_mode)) &&
                 strcmp(document_mode, "summary") == 0;
  cJSON *piece = NULL;
  UrsatzStatus result = workshop_library_piece_json(kPiecesDir, id, summary, &piece);

  if (result == URSATZ_ERR_NOT_FOUND) {
    ursatz_gui_send_json(conn, "{\"ok\":false,\"message\":\"piece not found\"}");
    return 200;
  }
  if (result != URSATZ_OK) {
    ursatz_gui_send_json(conn, "{\"ok\":false,\"message\":\"failed to load piece\"}");
    return 200;
  }
  return send_ok_with(conn, NULL, piece);
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

/* PATCH /api/analyzer/pieces/<id>: filename, composer, source, tags, add_tags, remove_tags, notation (an object, or null to clear; all
 * optional, unknown members ignored). A changed notation analyzes the piece again before the answer. */
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
  cJSON *piece = NULL;
  UrsatzError failure = {{0}, {0}};
  UrsatzStatus result = workshop_library_patch(kPiecesDir, id, root, &piece, &failure);
  cJSON_Delete(root);
  int status = 200;
  if (result == URSATZ_ERR_NOT_FOUND) {
    mg_send_http_error(conn, 404, "{\"ok\":false,\"message\":\"piece not found\"}");
    status = 404;
  } else if (result == URSATZ_ERR_REQUEST && strcmp(failure.path, "filename") == 0) {
    ursatz_gui_send_json(conn, "{\"ok\":false,\"message\":\"invalid filename\"}");
  } else if (result == URSATZ_ERR_REQUEST) {
    cJSON *answer = cJSON_CreateObject();
    char *text = NULL;
    if (answer) {
      cJSON_AddFalseToObject(answer, "ok");
      cJSON_AddStringToObject(answer, "message", failure.message);
      text = cJSON_PrintUnformatted(answer);
      cJSON_Delete(answer);
    }
    status = ursatz_gui_send_json_status(conn, 400, "Bad Request", text ? text : "{\"ok\":false,\"message\":\"invalid metadata\"}");
    free(text);
  } else if (result == URSATZ_ERR_UNSATISFIABLE) {
    ursatz_gui_send_json(conn, "{\"ok\":false,\"message\":\"nothing to update\"}");
  } else if (result != URSATZ_OK) {
    ursatz_gui_send_json(conn, "{\"ok\":false,\"message\":\"failed to update piece\"}");
  } else {
    status = send_ok_with(conn, "piece", piece);
  }
  return status;
}

static int handle_delete_piece(struct mg_connection *conn, const char *id) {
  UrsatzStatus result = workshop_library_delete(kPiecesDir, id);

  if (result == URSATZ_ERR_NOT_FOUND) {
    mg_send_http_error(conn, 404, "{\"ok\":false,\"message\":\"piece not found\"}");
    return 404;
  }
  if (result != URSATZ_OK) {
    ursatz_gui_send_json(conn, "{\"ok\":false,\"message\":\"failed to delete piece\"}");
    return 200;
  }
  ursatz_gui_send_json(conn, "{\"ok\":true}");
  return 200;
}

static const char *kReanalyzeSuffix = "/reanalyze";

static int handle_reanalyze_piece(struct mg_connection *conn, const char *id) {
  UrsatzStatus result = workshop_analysis_reanalyze(kPiecesDir, id);

  if (result == URSATZ_ERR_NOT_FOUND) {
    mg_send_http_error(conn, 404, "{\"ok\":false,\"message\":\"piece not found\"}");
    return 404;
  }
  if (result == URSATZ_ERR_FORMAT) {
    return ursatz_gui_send_json_status(conn, 422, "Unprocessable Entity", "{\"error\": \"import failed\"}");
  }
  if (result != URSATZ_OK) {
    ursatz_gui_send_json(conn, "{\"ok\":false,\"message\":\"failed to analyze piece again\"}");
    return 200;
  }

  cJSON *answer = cJSON_CreateObject();
  if (!answer || !cJSON_AddStringToObject(answer, "id", id)) {
    cJSON_Delete(answer);
    ursatz_gui_send_json(conn, "{\"ok\":false,\"message\":\"out of memory\"}");
    return 200;
  }
  return send_ok_with(conn, NULL, answer);
}

static const char *kMusicXmlSuffix = "/musicxml";
static const char *kMusicXmlMetaSuffix = "/musicxml/meta";
static const char *kPlaybackSuffix = "/playback";
static const char *kDocumentSuffix = "/document";
static const char *kExportSuffix = "/export";

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
  UrsatzStatus result = workshop_analysis_get_document(kPiecesDir, id, names, count, &sections);
  if (result == URSATZ_ERR_NOT_FOUND) {
    mg_send_http_error(conn, 404, "{\"ok\":false,\"message\":\"piece not found\"}");
    return 404;
  }
  if (result != URSATZ_OK) {
    return ursatz_gui_send_json_status(conn, 500, "Internal Server Error",
                                       "{\"ok\":false,\"message\":\"failed to load the document\"}");
  }
  cJSON *tree = cJSON_Parse(sections);
  free(sections);
  if (!tree) {
    return ursatz_gui_send_json_status(conn, 500, "Internal Server Error", "{\"ok\":false,\"message\":\"out of memory\"}");
  }
  return send_ok_with(conn, "sections", tree);
}

/* Reads a measure number of the export range: digits only, at most 6. False when it is not one. */
static bool parse_export_measure(const char *text, int *out) {
  size_t length = strlen(text);
  if (length == 0 || length > 6) {
    return false;
  }
  int value = 0;
  for (size_t i = 0; i < length; i++) {
    if (text[i] < '0' || text[i] > '9') {
      return false;
    }
    value = value * 10 + (text[i] - '0');
  }
  *out = value;
  return true;
}

/* GET /api/analyzer/pieces/<id>/export[?from=<m>&to=<m>]: the Analyzer's text export of the piece (text/plain). */
static int handle_get_piece_export(struct mg_connection *conn, const char *id) {
  const struct mg_request_info *info = mg_get_request_info(conn);
  const char *bad_range = "{\"ok\":false,\"message\":\"from and to must be measure numbers, from not after to\"}";
  char from_text[16];
  char to_text[16];
  bool has_from = query_value(info, "from", 0, from_text, sizeof(from_text));
  bool has_to = query_value(info, "to", 0, to_text, sizeof(to_text));
  int first = 0;
  int last = 0;
  if ((has_from && !parse_export_measure(from_text, &first)) || (has_to && !parse_export_measure(to_text, &last))) {
    return ursatz_gui_send_json_status(conn, 400, "Bad Request", bad_range);
  }
  if (has_from || has_to) {
    if (!has_from) {
      first = 1;
    }
    if (!has_to) {
      last = 999999;
    }
    if (first > last) {
      return ursatz_gui_send_json_status(conn, 400, "Bad Request", bad_range);
    }
  }

  char *text = NULL;
  UrsatzStatus result = workshop_analysis_get_piece_export(kPiecesDir, id, first, last, &text);
  if (result == URSATZ_ERR_NOT_FOUND) {
    mg_send_http_error(conn, 404, "{\"ok\":false,\"message\":\"piece not found\"}");
    return 404;
  }
  if (result == URSATZ_ERR_FORMAT) {
    return ursatz_gui_send_json_status(conn, 422, "Unprocessable Entity",
                            "{\"ok\":false,\"message\":\"could not import the stored source\"}");
  }
  if (result == URSATZ_ERR_REQUEST) {
    return ursatz_gui_send_json_status(conn, 400, "Bad Request",
                            "{\"ok\":false,\"message\":\"no measure in that range\"}");
  }
  if (result != URSATZ_OK) {
    return ursatz_gui_send_json_status(conn, 500, "Internal Server Error",
                            "{\"ok\":false,\"message\":\"failed to export the analysis\"}");
  }

  size_t len = strlen(text);
  mg_send_http_ok(conn, "text/plain; charset=utf-8", (long)len);
  mg_write(conn, text, len);
  free(text);
  return 200;
}

static int handle_get_piece_musicxml(struct mg_connection *conn, const char *id) {
  char *xml = NULL;
  bool meter_defaulted = false;
  UrsatzStatus result = workshop_analysis_get_piece_musicxml(kPiecesDir, id, &xml, &meter_defaulted);

  if (result == URSATZ_ERR_NOT_FOUND) {
    mg_send_http_error(conn, 404, "{\"ok\":false,\"message\":\"piece not found\"}");
    return 404;
  }
  if (result == URSATZ_ERR_FORMAT) {
    return ursatz_gui_send_json_status(conn, 422, "Unprocessable Entity",
                            "{\"ok\":false,\"message\":\"could not import the stored source\"}");
  }
  if (result == URSATZ_ERR_REQUEST || result == URSATZ_ERR_INTERNAL) {
    send_musicxml_export_error(conn, result);
    return 500;
  }
  if (result != URSATZ_OK) {
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
  UrsatzStatus result = workshop_analysis_get_piece_meter_defaulted(kPiecesDir, id, &meter_defaulted);

  if (result == URSATZ_ERR_NOT_FOUND) {
    mg_send_http_error(conn, 404, "{\"ok\":false,\"message\":\"piece not found\"}");
    return 404;
  }
  if (result == URSATZ_ERR_FORMAT) {
    return ursatz_gui_send_json_status(conn, 422, "Unprocessable Entity",
                            "{\"ok\":false,\"message\":\"could not import the stored source\"}");
  }
  if (result != URSATZ_OK) {
    return ursatz_gui_send_json_status(conn, 500, "Internal Server Error", "{\"ok\":false,\"message\":\"failed to export piece\"}");
  }

  ursatz_gui_send_json(conn, meter_defaulted ? "{\"ok\":true,\"meter_defaulted\":true}" : "{\"ok\":true,\"meter_defaulted\":false}");
  return 200;
}

static int handle_get_piece_playback(struct mg_connection *conn, const char *id) {
  char *json = NULL;
  UrsatzStatus result = workshop_analysis_get_piece_playback(kPiecesDir, id, &json);

  if (result == URSATZ_ERR_NOT_FOUND) {
    mg_send_http_error(conn, 404, "{\"ok\":false,\"message\":\"piece not found\"}");
    return 404;
  }
  if (result == URSATZ_ERR_FORMAT) {
    return ursatz_gui_send_json_status(conn, 422, "Unprocessable Entity",
                            "{\"ok\":false,\"message\":\"could not import the stored source\"}");
  }
  if (result != URSATZ_OK) {
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
    size_t export_suffix_len = strlen(kExportSuffix);

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

    if (strcmp(info->request_method, "GET") == 0 && rest_len > export_suffix_len &&
        strcmp(rest + rest_len - export_suffix_len, kExportSuffix) == 0) {
      char id[256];
      size_t id_len = rest_len - export_suffix_len;
      if (id_len >= sizeof(id)) {
        mg_send_http_error(conn, 404, "{\"ok\":false,\"message\":\"piece not found\"}");
        return 404;
      }
      memcpy(id, rest, id_len);
      id[id_len] = '\0';
      return handle_get_piece_export(conn, id);
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
