#include "corpus.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <civetweb.h>

#include "../service/corpus_service.h"

static const char *kCorpusPrefix = "/api/corpus";
static const size_t kMaxRequestBodySize = 64 * 1024;

static void send_json(struct mg_connection *conn, const char *body) {
  size_t len = strlen(body);
  mg_send_http_ok(conn, "application/json", (long)len);
  mg_write(conn, body, len);
}

static void send_ok_false(struct mg_connection *conn, const char *message) {
  char body[320];
  snprintf(body, sizeof(body), "{\"ok\":false,\"message\":\"%s\"}", message);
  send_json(conn, body);
}

/* Reads the full request body into a newly allocated, NUL-terminated
 * buffer (caller frees). Returns NULL on allocation failure, a read
 * error, or a body larger than kMaxRequestBodySize -- corpus request
 * bodies are small JSON objects, unlike the Analyzer upload route. */
static char *read_request_body(struct mg_connection *conn) {
  size_t capacity = 4096;
  char *buffer = malloc(capacity);
  if (!buffer) {
    return NULL;
  }

  size_t total = 0;
  for (;;) {
    if (total == capacity) {
      if (capacity >= kMaxRequestBodySize) {
        free(buffer);
        return NULL;
      }
      size_t new_capacity = capacity * 2;
      char *grown = realloc(buffer, new_capacity);
      if (!grown) {
        free(buffer);
        return NULL;
      }
      buffer = grown;
      capacity = new_capacity;
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

/* Extracts the string value of a top-level "field":"value" pair from a
 * flat JSON object body. Minimal by design (no nesting, no escapes) --
 * every request body this route accepts is a flat object of plain id/name
 * strings, same assumption the rest of this codebase's JSON handling
 * makes. Returns a newly allocated, caller-owned string, or NULL if the
 * field is absent or malformed. */
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

static const char *result_message(UrsatzGuiCorpusResult result) {
  switch (result) {
    case URSATZ_GUI_CORPUS_ERR_INVALID_ID:
      return "invalid id";
    case URSATZ_GUI_CORPUS_ERR_INVALID_NAME:
      return "invalid name";
    case URSATZ_GUI_CORPUS_ERR_ALREADY_MEMBER:
      return "piece is already in this corpus";
    case URSATZ_GUI_CORPUS_ERR_NOT_MEMBER:
      return "piece is not in this corpus";
    case URSATZ_GUI_CORPUS_ERR_ALLOC:
      return "out of memory";
    case URSATZ_GUI_CORPUS_ERR_NOT_INITIALIZED:
      return "corpus store is not available";
    case URSATZ_GUI_CORPUS_ERR_FAILED:
    default:
      return "corpus operation failed";
  }
}

static int handle_create_corpus(struct mg_connection *conn) {
  char *body = read_request_body(conn);
  if (!body) {
    send_ok_false(conn, "failed to read request body");
    return 200;
  }

  char *id = extract_json_string_field(body, "id");
  char *name = extract_json_string_field(body, "name");
  free(body);

  if (!id || !ursatz_gui_corpus_is_valid_id(id)) {
    free(id);
    free(name);
    send_ok_false(conn, "invalid id");
    return 200;
  }
  if (!name) {
    free(id);
    send_ok_false(conn, "invalid name");
    return 200;
  }

  UrsatzGuiCorpusResult result = ursatz_gui_corpus_create(id, name);
  free(id);
  free(name);

  if (result != URSATZ_GUI_CORPUS_OK) {
    send_ok_false(conn, result_message(result));
    return 200;
  }

  send_json(conn, "{\"ok\":true}");
  return 200;
}

static int handle_list_corpora(struct mg_connection *conn) {
  UrsatzGuiCorpusEntry *entries = NULL;
  size_t count = 0;
  UrsatzGuiCorpusResult result = ursatz_gui_corpus_list(&entries, &count);

  if (result != URSATZ_GUI_CORPUS_OK) {
    send_ok_false(conn, result_message(result));
    return 200;
  }

  size_t capacity = 64;
  for (size_t i = 0; i < count; i++) {
    capacity += strlen(entries[i].id) + strlen(entries[i].name) + 32;
  }
  char *out_body = malloc(capacity);
  if (!out_body) {
    ursatz_gui_corpus_free_entries(entries, count);
    send_ok_false(conn, "out of memory");
    return 200;
  }

  size_t offset = (size_t)snprintf(out_body, capacity, "{\"ok\":true,\"corpora\":[");
  for (size_t i = 0; i < count; i++) {
    offset += (size_t)snprintf(out_body + offset, capacity - offset, "%s{\"id\":\"%s\",\"name\":\"%s\"}",
                                i == 0 ? "" : ",", entries[i].id, entries[i].name);
  }
  ursatz_gui_corpus_free_entries(entries, count);
  offset += (size_t)snprintf(out_body + offset, capacity - offset, "]}");

  send_json(conn, out_body);
  free(out_body);
  return 200;
}

static int handle_add_piece(struct mg_connection *conn, const char *corpus_id) {
  char *body = read_request_body(conn);
  if (!body) {
    send_ok_false(conn, "failed to read request body");
    return 200;
  }

  char *piece_id = extract_json_string_field(body, "piece_id");
  free(body);

  if (!piece_id || !ursatz_gui_corpus_is_valid_id(piece_id)) {
    free(piece_id);
    send_ok_false(conn, "invalid piece_id");
    return 200;
  }

  UrsatzGuiCorpusResult result = ursatz_gui_corpus_add_piece(corpus_id, piece_id);
  free(piece_id);

  if (result != URSATZ_GUI_CORPUS_OK) {
    send_ok_false(conn, result_message(result));
    return 200;
  }

  send_json(conn, "{\"ok\":true}");
  return 200;
}

static int handle_remove_piece(struct mg_connection *conn, const char *corpus_id, const char *piece_id) {
  UrsatzGuiCorpusResult result = ursatz_gui_corpus_remove_piece(corpus_id, piece_id);

  if (result != URSATZ_GUI_CORPUS_OK) {
    send_ok_false(conn, result_message(result));
    return 200;
  }

  send_json(conn, "{\"ok\":true}");
  return 200;
}

static int handle_list_pieces(struct mg_connection *conn, const char *corpus_id) {
  char **ids = NULL;
  size_t count = 0;
  UrsatzGuiCorpusResult result = ursatz_gui_corpus_list_pieces(corpus_id, &ids, &count);

  if (result != URSATZ_GUI_CORPUS_OK) {
    send_ok_false(conn, result_message(result));
    return 200;
  }

  size_t capacity = 64;
  for (size_t i = 0; i < count; i++) {
    capacity += strlen(ids[i]) + 16;
  }
  char *out_body = malloc(capacity);
  if (!out_body) {
    for (size_t i = 0; i < count; i++) {
      free(ids[i]);
    }
    free(ids);
    send_ok_false(conn, "out of memory");
    return 200;
  }

  size_t offset = (size_t)snprintf(out_body, capacity, "{\"ok\":true,\"pieces\":[");
  for (size_t i = 0; i < count; i++) {
    offset += (size_t)snprintf(out_body + offset, capacity - offset, "%s{\"id\":\"%s\"}", i == 0 ? "" : ",", ids[i]);
    free(ids[i]);
  }
  free(ids);
  offset += (size_t)snprintf(out_body + offset, capacity - offset, "]}");

  send_json(conn, out_body);
  free(out_body);
  return 200;
}

/*
 * Single handler registered at kCorpusPrefix ("/api/corpus"). civetweb
 * routes both the exact prefix and any "prefix/something" URI to a
 * handler registered at prefix (see the identical note in analyzer.c), so
 * every corpus route -- "/api/corpus", ".../<id>/pieces",
 * ".../<id>/pieces/<piece_id>" -- arrives here and is dispatched by
 * local_uri.
 */
static int corpus_handler(struct mg_connection *conn, void *cbdata) {
  (void)cbdata;
  const struct mg_request_info *info = mg_get_request_info(conn);
  const char *uri = info->local_uri;
  const char *method = info->request_method;
  size_t prefix_len = strlen(kCorpusPrefix);

  if (strcmp(uri, kCorpusPrefix) == 0) {
    if (strcmp(method, "POST") == 0) {
      return handle_create_corpus(conn);
    }
    if (strcmp(method, "GET") == 0) {
      return handle_list_corpora(conn);
    }
    mg_send_http_error(conn, 405, "Method Not Allowed");
    return 405;
  }

  if (strncmp(uri, kCorpusPrefix, prefix_len) != 0 || uri[prefix_len] != '/') {
    mg_send_http_error(conn, 404, "Not Found");
    return 404;
  }

  /* rest is "<corpus_id>/pieces" or "<corpus_id>/pieces/<piece_id>". */
  const char *rest = uri + prefix_len + 1;
  const char *pieces_start = strstr(rest, "/pieces");
  if (!pieces_start || (pieces_start[strlen("/pieces")] != '\0' && pieces_start[strlen("/pieces")] != '/')) {
    mg_send_http_error(conn, 404, "Not Found");
    return 404;
  }

  size_t corpus_id_len = (size_t)(pieces_start - rest);
  if (corpus_id_len == 0 || corpus_id_len >= 256) {
    mg_send_http_error(conn, 404, "Not Found");
    return 404;
  }
  char corpus_id[256];
  memcpy(corpus_id, rest, corpus_id_len);
  corpus_id[corpus_id_len] = '\0';

  if (!ursatz_gui_corpus_is_valid_id(corpus_id)) {
    send_ok_false(conn, "invalid id");
    return 200;
  }

  const char *after_pieces = pieces_start + strlen("/pieces");

  if (after_pieces[0] == '\0') {
    if (strcmp(method, "POST") == 0) {
      return handle_add_piece(conn, corpus_id);
    }
    if (strcmp(method, "GET") == 0) {
      return handle_list_pieces(conn, corpus_id);
    }
    mg_send_http_error(conn, 405, "Method Not Allowed");
    return 405;
  }

  if (after_pieces[0] == '/' && after_pieces[1] != '\0') {
    const char *piece_id = after_pieces + 1;
    if (!ursatz_gui_corpus_is_valid_id(piece_id)) {
      send_ok_false(conn, "invalid piece_id");
      return 200;
    }
    if (strcmp(method, "DELETE") == 0) {
      return handle_remove_piece(conn, corpus_id, piece_id);
    }
    mg_send_http_error(conn, 405, "Method Not Allowed");
    return 405;
  }

  mg_send_http_error(conn, 404, "Not Found");
  return 404;
}

void ursatz_gui_corpus_register(struct mg_context *ctx) {
  mg_set_request_handler(ctx, kCorpusPrefix, corpus_handler, NULL);
}
