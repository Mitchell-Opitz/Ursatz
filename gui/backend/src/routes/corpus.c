#include "corpus.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <civetweb.h>

#include "../service/collection_service.h"
#include "../service/corpus_service.h"

static const char *kCorpusPrefix = "/api/corpus";
static const size_t kMaxRequestBodySize = 64 * 1024;

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

/* Sends {"ok":false,"message":...} with the given status (200 for the
 * failures this route has always answered with 200). The message goes
 * through cJSON, so it is escaped. */
static int send_failure(struct mg_connection *conn, int status, const char *message) {
  cJSON *object = cJSON_CreateObject();
  char *text = NULL;
  if (object && cJSON_AddBoolToObject(object, "ok", 0) && cJSON_AddStringToObject(object, "message", message)) {
    text = cJSON_PrintUnformatted(object);
  }
  cJSON_Delete(object);
  if (!text) {
    return send_json_status(conn, 500, "Internal Server Error", "{\"ok\":false,\"message\":\"out of memory\"}");
  }
  if (status == 200) {
    send_json(conn, text);
  } else {
    const char *reason = status == 404 ? "Not Found" : status == 500 ? "Internal Server Error" : "Bad Request";
    send_json_status(conn, status, reason, text);
  }
  free(text);
  return status;
}

static void send_ok_false(struct mg_connection *conn, const char *message) {
  send_failure(conn, 200, message);
}

/* Sends object as the 200 response and frees it. */
static int send_object(struct mg_connection *conn, cJSON *object) {
  char *text = object ? cJSON_PrintUnformatted(object) : NULL;
  cJSON_Delete(object);
  if (!text) {
    return send_failure(conn, 500, "out of memory");
  }
  send_json(conn, text);
  free(text);
  return 200;
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

/* Reads the request body and parses it as a JSON object. NULL when the
 * body cannot be read, is not valid JSON, or is not an object (caller
 * answers "invalid request body"); caller frees with cJSON_Delete. */
static cJSON *read_json_body(struct mg_connection *conn) {
  char *text = read_request_body(conn);
  if (!text) {
    return NULL;
  }
  cJSON *body = cJSON_Parse(text);
  free(text);
  if (!cJSON_IsObject(body)) {
    cJSON_Delete(body);
    return NULL;
  }
  return body;
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

static const char *collection_message(UrsatzGuiCollectionResult result) {
  switch (result) {
    case URSATZ_GUI_COLLECTION_ERR_INVALID_ID:
      return "invalid id";
    case URSATZ_GUI_COLLECTION_ERR_INVALID_NAME:
      return "invalid name";
    case URSATZ_GUI_COLLECTION_ERR_INVALID_DESCRIPTION:
      return "invalid description";
    case URSATZ_GUI_COLLECTION_ERR_ALREADY_EXISTS:
      return "collection already exists";
    case URSATZ_GUI_COLLECTION_ERR_NOT_FOUND:
      return "collection not found";
    case URSATZ_GUI_COLLECTION_ERR_ALLOC:
      return "out of memory";
    case URSATZ_GUI_COLLECTION_ERR_NOT_INITIALIZED:
      return "collection store is not available";
    case URSATZ_GUI_COLLECTION_ERR_FAILED:
    default:
      return "collection operation failed";
  }
}

/* A missing collection is a 404; every other failure keeps the route's
 * 200 with {"ok":false,...}. */
static int send_collection_failure(struct mg_connection *conn, UrsatzGuiCollectionResult result) {
  return send_failure(conn, result == URSATZ_GUI_COLLECTION_ERR_NOT_FOUND ? 404 : 200, collection_message(result));
}

/* The members every collection answer carries. */
static bool add_summary_members(cJSON *object, const char *id, const char *name, const char *description,
                                size_t piece_count, const char *updated_at) {
  return cJSON_AddStringToObject(object, "id", id) && cJSON_AddStringToObject(object, "name", name) &&
         cJSON_AddStringToObject(object, "description", description) &&
         cJSON_AddNumberToObject(object, "piece_count", (double)piece_count) &&
         cJSON_AddStringToObject(object, "updated_at", updated_at);
}

/* {"ok":true,"id","name","description","piece_count","updated_at"} for the
 * stored collection id, so an answer shows the normalized values. */
static int send_collection_summary(struct mg_connection *conn, const char *id) {
  UrsatzGuiCollection *collection = NULL;
  UrsatzGuiCollectionResult result = ursatz_gui_collection_get(id, &collection);
  if (result != URSATZ_GUI_COLLECTION_OK) {
    return send_collection_failure(conn, result);
  }
  cJSON *object = cJSON_CreateObject();
  bool built = object && cJSON_AddBoolToObject(object, "ok", 1) &&
               add_summary_members(object, collection->id, collection->name, collection->description,
                                   collection->piece_count, collection->updated_at);
  ursatz_gui_collection_destroy(collection);
  if (!built) {
    cJSON_Delete(object);
    return send_failure(conn, 500, "out of memory");
  }
  return send_object(conn, object);
}

static int handle_create_corpus(struct mg_connection *conn) {
  cJSON *body = read_json_body(conn);
  if (!body) {
    return send_failure(conn, 200, "invalid request body");
  }

  const cJSON *id_item = cJSON_GetObjectItemCaseSensitive(body, "id");
  const cJSON *name_item = cJSON_GetObjectItemCaseSensitive(body, "name");
  const cJSON *description_item = cJSON_GetObjectItemCaseSensitive(body, "description");
  if (!cJSON_IsString(name_item)) {
    cJSON_Delete(body);
    return send_failure(conn, 200, "invalid name");
  }
  if (id_item && !cJSON_IsString(id_item)) {
    cJSON_Delete(body);
    return send_failure(conn, 200, "invalid id");
  }
  if (description_item && !cJSON_IsString(description_item)) {
    cJSON_Delete(body);
    return send_failure(conn, 200, "invalid description");
  }

  const char *description = description_item ? description_item->valuestring : "";
  char *id = NULL;
  UrsatzGuiCollectionResult result;
  if (id_item) {
    /* R-098: a client may still name the id, when it is valid and unused. */
    id = malloc(strlen(id_item->valuestring) + 1);
    if (!id) {
      cJSON_Delete(body);
      return send_failure(conn, 200, "out of memory");
    }
    strcpy(id, id_item->valuestring);
    result = ursatz_gui_collection_create_with_id(id, name_item->valuestring, description);
  } else {
    result = ursatz_gui_collection_create(name_item->valuestring, description, &id);
  }
  cJSON_Delete(body);

  if (result != URSATZ_GUI_COLLECTION_OK) {
    free(id);
    return send_collection_failure(conn, result);
  }
  int status = send_collection_summary(conn, id);
  free(id);
  return status;
}

static int handle_list_corpora(struct mg_connection *conn) {
  UrsatzGuiCollectionEntry *entries = NULL;
  size_t count = 0;
  UrsatzGuiCollectionResult result = ursatz_gui_collection_list(&entries, &count);
  if (result != URSATZ_GUI_COLLECTION_OK) {
    return send_collection_failure(conn, result);
  }

  cJSON *object = cJSON_CreateObject();
  cJSON *list = object ? cJSON_AddArrayToObject(object, "corpora") : NULL;
  bool built = list && cJSON_AddBoolToObject(object, "ok", 1);
  for (size_t i = 0; built && i < count; i++) {
    cJSON *item = cJSON_CreateObject();
    built = item && add_summary_members(item, entries[i].id, entries[i].name, entries[i].description,
                                        entries[i].piece_count, entries[i].updated_at);
    if (built) {
      cJSON_AddItemToArray(list, item);
    } else {
      cJSON_Delete(item);
    }
  }
  ursatz_gui_collection_free_entries(entries, count);
  if (!built) {
    cJSON_Delete(object);
    return send_failure(conn, 500, "out of memory");
  }
  return send_object(conn, object);
}

static int handle_get_corpus(struct mg_connection *conn, const char *id) {
  UrsatzGuiCollection *collection = NULL;
  UrsatzGuiCollectionResult result = ursatz_gui_collection_get(id, &collection);
  if (result != URSATZ_GUI_COLLECTION_OK) {
    return send_collection_failure(conn, result);
  }

  cJSON *object = cJSON_CreateObject();
  cJSON *pieces = object ? cJSON_AddArrayToObject(object, "pieces") : NULL;
  bool built = pieces && cJSON_AddBoolToObject(object, "ok", 1) &&
               add_summary_members(object, collection->id, collection->name, collection->description,
                                   collection->piece_count, collection->updated_at) &&
               cJSON_AddStringToObject(object, "created_at", collection->created_at);
  for (size_t i = 0; built && i < collection->piece_count; i++) {
    cJSON *piece_id = cJSON_CreateString(collection->pieces[i]);
    built = piece_id != NULL;
    if (built) {
      cJSON_AddItemToArray(pieces, piece_id);
    }
  }
  ursatz_gui_collection_destroy(collection);
  if (!built) {
    cJSON_Delete(object);
    return send_failure(conn, 500, "out of memory");
  }
  return send_object(conn, object);
}

static int handle_patch_corpus(struct mg_connection *conn, const char *id) {
  cJSON *body = read_json_body(conn);
  if (!body) {
    return send_failure(conn, 200, "invalid request body");
  }

  const cJSON *name_item = cJSON_GetObjectItemCaseSensitive(body, "name");
  const cJSON *description_item = cJSON_GetObjectItemCaseSensitive(body, "description");
  if (!name_item && !description_item) {
    cJSON_Delete(body);
    return send_failure(conn, 200, "nothing to update");
  }
  if (name_item && !cJSON_IsString(name_item)) {
    cJSON_Delete(body);
    return send_failure(conn, 200, "invalid name");
  }
  if (description_item && !cJSON_IsString(description_item)) {
    cJSON_Delete(body);
    return send_failure(conn, 200, "invalid description");
  }

  UrsatzGuiCollectionResult result = ursatz_gui_collection_update(
      id, name_item ? name_item->valuestring : NULL, description_item ? description_item->valuestring : NULL);
  cJSON_Delete(body);
  if (result != URSATZ_GUI_COLLECTION_OK) {
    return send_collection_failure(conn, result);
  }
  return send_collection_summary(conn, id);
}

static int handle_delete_corpus(struct mg_connection *conn, const char *id) {
  UrsatzGuiCollectionResult result = ursatz_gui_collection_delete(id);
  if (result != URSATZ_GUI_COLLECTION_OK) {
    return send_collection_failure(conn, result);
  }
  send_json(conn, "{\"ok\":true}");
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

  /* rest is "<corpus_id>", "<corpus_id>/pieces" or "<corpus_id>/pieces/<piece_id>". */
  const char *rest = uri + prefix_len + 1;
  const char *slash = strchr(rest, '/');
  size_t corpus_id_len = slash ? (size_t)(slash - rest) : strlen(rest);
  if (corpus_id_len == 0 || corpus_id_len >= 256) {
    mg_send_http_error(conn, 404, "Not Found");
    return 404;
  }
  char corpus_id[256];
  memcpy(corpus_id, rest, corpus_id_len);
  corpus_id[corpus_id_len] = '\0';

  if (!ursatz_gui_collection_is_valid_id(corpus_id)) {
    send_ok_false(conn, "invalid id");
    return 200;
  }

  if (!slash) {
    if (strcmp(method, "GET") == 0) {
      return handle_get_corpus(conn, corpus_id);
    }
    if (strcmp(method, "PATCH") == 0) {
      return handle_patch_corpus(conn, corpus_id);
    }
    if (strcmp(method, "DELETE") == 0) {
      return handle_delete_corpus(conn, corpus_id);
    }
    mg_send_http_error(conn, 405, "Method Not Allowed");
    return 405;
  }

  const char *pieces_start = slash;
  if (strncmp(pieces_start, "/pieces", strlen("/pieces")) != 0 ||
      (pieces_start[strlen("/pieces")] != '\0' && pieces_start[strlen("/pieces")] != '/')) {
    mg_send_http_error(conn, 404, "Not Found");
    return 404;
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
