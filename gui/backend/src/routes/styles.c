#include "styles.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <civetweb.h>

#include "../service/collection_service.h"
#include "../service/style_service.h"

static const char *kStylesPrefix = "/api/styles";
static const size_t kMaxRequestBodySize = 1024 * 1024;
/* A style built from ids names every piece of a filtered Library. */
static const size_t kMaxPieceIds = 5000;

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

/* Sends {"ok":false,"message":...} with the given status. The message goes through cJSON, so it is escaped. */
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
  const char *reason = status == 404   ? "Not Found"
                       : status == 422 ? "Unprocessable Entity"
                       : status == 500 ? "Internal Server Error"
                                       : "Bad Request";
  send_json_status(conn, status, reason, text);
  free(text);
  return status;
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

/* Reads the request body into a newly allocated, NUL-terminated buffer (caller frees). NULL on allocation
 * failure, a read error, or a body larger than kMaxRequestBodySize. */
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

/* The request body as a JSON object; NULL when it cannot be read, is not JSON, or is not an object. */
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

static cJSON *string_or_null(const char *text) {
  return text && text[0] != '\0' ? cJSON_CreateString(text) : cJSON_CreateNull();
}

static cJSON *string_array(char *const *items, size_t count) {
  cJSON *array = cJSON_CreateArray();
  for (size_t i = 0; array && i < count; i++) {
    cJSON *item = cJSON_CreateString(items[i]);
    if (!item) {
      cJSON_Delete(array);
      return NULL;
    }
    cJSON_AddItemToArray(array, item);
  }
  return array;
}

static const char *source_kind_label(UrsatzGuiStyleSourceKind kind) {
  switch (kind) {
    case URSATZ_GUI_STYLE_SOURCE_FILTER:
      return "filter";
    case URSATZ_GUI_STYLE_SOURCE_COLLECTION:
      return "collection";
    case URSATZ_GUI_STYLE_SOURCE_PIECES:
    default:
      return "pieces";
  }
}

/* {"kind","filter","collection_id","piece_ids"}: the members that do not belong to the kind are null. */
static cJSON *source_json(const UrsatzGuiStyle *style) {
  cJSON *source = cJSON_CreateObject();
  cJSON *filter = NULL;
  cJSON *piece_ids = NULL;
  bool ok = source && cJSON_AddStringToObject(source, "kind", source_kind_label(style->source_kind));
  if (ok && style->source_kind == URSATZ_GUI_STYLE_SOURCE_FILTER) {
    filter = cJSON_CreateObject();
    ok = filter && cJSON_AddItemToObject(filter, "composer", string_or_null(style->filter_composer)) &&
         cJSON_AddItemToObject(filter, "source", string_or_null(style->filter_source)) &&
         cJSON_AddItemToObject(filter, "tag", string_or_null(style->filter_tag));
  } else if (ok) {
    filter = cJSON_CreateNull();
    ok = filter != NULL;
  }
  if (ok && style->source_kind == URSATZ_GUI_STYLE_SOURCE_PIECES) {
    piece_ids = string_array(style->source_piece_ids, style->source_piece_count);
  } else if (ok) {
    piece_ids = cJSON_CreateNull();
  }
  if (!ok || !piece_ids) {
    cJSON_Delete(filter);
    cJSON_Delete(piece_ids);
    cJSON_Delete(source);
    return NULL;
  }
  cJSON_AddItemToObject(source, "filter", filter);
  if (!cJSON_AddItemToObject(source, "collection_id", string_or_null(style->collection_id))) {
    cJSON_Delete(piece_ids);
    cJSON_Delete(source);
    return NULL;
  }
  cJSON_AddItemToObject(source, "piece_ids", piece_ids);
  return source;
}

/* The list entry: {"id","name","piece_count","source","updated_at"}. */
static cJSON *entry_json(const UrsatzGuiStyle *style) {
  cJSON *entry = cJSON_CreateObject();
  cJSON *source = entry ? source_json(style) : NULL;
  if (!source || !cJSON_AddStringToObject(entry, "id", style->id) ||
      !cJSON_AddStringToObject(entry, "name", style->name) ||
      !cJSON_AddNumberToObject(entry, "piece_count", (double)style->piece_count)) {
    cJSON_Delete(source);
    cJSON_Delete(entry);
    return NULL;
  }
  cJSON_AddItemToObject(entry, "source", source);
  if (!cJSON_AddStringToObject(entry, "updated_at", style->updated_at)) {
    cJSON_Delete(entry);
    return NULL;
  }
  return entry;
}

static cJSON *skipped_json(const UrsatzGuiStyleSkipped *skipped, size_t count) {
  cJSON *array = cJSON_CreateArray();
  for (size_t i = 0; array && i < count; i++) {
    cJSON *item = cJSON_CreateObject();
    if (!item || !cJSON_AddStringToObject(item, "id", skipped[i].id) ||
        !cJSON_AddStringToObject(item, "reason", skipped[i].reason)) {
      cJSON_Delete(item);
      cJSON_Delete(array);
      return NULL;
    }
    cJSON_AddItemToArray(array, item);
  }
  return array;
}

/* {dimension: {category: {"labels": [{"label","count","total"}], "more": n}}} */
static cJSON *statistics_json(const UrsatzGuiStyle *style) {
  cJSON *statistics = cJSON_CreateObject();
  for (size_t i = 0; statistics && i < style->category_count; i++) {
    const UrsatzGuiStyleCategory *category = &style->categories[i];
    cJSON *dimension = cJSON_GetObjectItemCaseSensitive(statistics, category->dimension);
    if (!dimension) {
      dimension = cJSON_AddObjectToObject(statistics, category->dimension);
    }
    cJSON *entry = dimension ? cJSON_AddObjectToObject(dimension, category->category) : NULL;
    cJSON *labels = entry ? cJSON_AddArrayToObject(entry, "labels") : NULL;
    bool ok = labels != NULL;
    for (size_t j = 0; ok && j < category->label_count; j++) {
      cJSON *label = cJSON_CreateObject();
      ok = label && cJSON_AddStringToObject(label, "label", category->labels[j].label) &&
           cJSON_AddNumberToObject(label, "count", (double)category->labels[j].count) &&
           cJSON_AddNumberToObject(label, "total", (double)category->labels[j].total);
      if (ok) {
        cJSON_AddItemToArray(labels, label);
      } else {
        cJSON_Delete(label);
      }
    }
    if (!ok || !cJSON_AddNumberToObject(entry, "more", (double)category->more)) {
      cJSON_Delete(statistics);
      return NULL;
    }
  }
  return statistics;
}

/* The full style: the list entry plus "created_at", "pieces", "skipped" and "statistics". */
static cJSON *style_json(const UrsatzGuiStyle *style) {
  cJSON *object = entry_json(style);
  cJSON *pieces = object ? string_array(style->pieces, style->piece_count) : NULL;
  cJSON *skipped = pieces ? skipped_json(style->skipped, style->skipped_count) : NULL;
  cJSON *statistics = skipped ? statistics_json(style) : NULL;
  if (!statistics || !cJSON_AddStringToObject(object, "created_at", style->created_at)) {
    cJSON_Delete(pieces);
    cJSON_Delete(skipped);
    cJSON_Delete(statistics);
    cJSON_Delete(object);
    return NULL;
  }
  cJSON_AddItemToObject(object, "pieces", pieces);
  cJSON_AddItemToObject(object, "skipped", skipped);
  cJSON_AddItemToObject(object, "statistics", statistics);
  return object;
}

static int send_style_failure(struct mg_connection *conn, UrsatzGuiStyleResult result) {
  switch (result) {
    case URSATZ_GUI_STYLE_ERR_INVALID_NAME:
      return send_failure(conn, 400, "invalid name");
    case URSATZ_GUI_STYLE_ERR_INVALID_SOURCE:
      return send_failure(conn, 400, "invalid source");
    case URSATZ_GUI_STYLE_ERR_NOT_FOUND:
      return send_failure(conn, 404, "style not found");
    case URSATZ_GUI_STYLE_ERR_NO_PIECES:
      return send_failure(conn, 422, "no analyzable pieces");
    case URSATZ_GUI_STYLE_ERR_SOURCE_MISSING:
      return send_failure(conn, 422, "collection not found");
    case URSATZ_GUI_STYLE_ERR_ALLOC:
      return send_failure(conn, 500, "out of memory");
    case URSATZ_GUI_STYLE_ERR_FAILED:
    case URSATZ_GUI_STYLE_OK:
    default:
      return send_failure(conn, 500, "style operation failed");
  }
}

/* {"ok":true,"style":{list entry},"skipped":[...]} for the stored style id. */
static int send_built_style(struct mg_connection *conn, const char *id) {
  UrsatzGuiStyle *style = NULL;
  UrsatzGuiStyleResult result = ursatz_gui_style_get(id, &style);
  if (result != URSATZ_GUI_STYLE_OK) {
    return send_style_failure(conn, result);
  }
  cJSON *object = cJSON_CreateObject();
  cJSON *entry = object ? entry_json(style) : NULL;
  cJSON *skipped = entry ? skipped_json(style->skipped, style->skipped_count) : NULL;
  ursatz_gui_style_destroy(style);
  if (!skipped || !cJSON_AddBoolToObject(object, "ok", 1)) {
    cJSON_Delete(entry);
    cJSON_Delete(skipped);
    cJSON_Delete(object);
    return send_failure(conn, 500, "out of memory");
  }
  cJSON_AddItemToObject(object, "style", entry);
  cJSON_AddItemToObject(object, "skipped", skipped);
  return send_object(conn, object);
}

static int handle_build_style(struct mg_connection *conn) {
  cJSON *body = read_json_body(conn);
  if (!body) {
    return send_failure(conn, 400, "invalid request body");
  }

  const cJSON *name_item = cJSON_GetObjectItemCaseSensitive(body, "name");
  const cJSON *filter_item = cJSON_GetObjectItemCaseSensitive(body, "filter");
  const cJSON *collection_item = cJSON_GetObjectItemCaseSensitive(body, "collection_id");
  const cJSON *ids_item = cJSON_GetObjectItemCaseSensitive(body, "piece_ids");
  if (name_item && !cJSON_IsString(name_item)) {
    cJSON_Delete(body);
    return send_failure(conn, 400, "invalid name");
  }
  int sources = (filter_item != NULL) + (collection_item != NULL) + (ids_item != NULL);
  if (sources != 1) {
    cJSON_Delete(body);
    return send_failure(conn, 400, "give exactly one of filter, collection_id and piece_ids");
  }

  UrsatzGuiStyleSource source;
  memset(&source, 0, sizeof(source));
  const char **ids = NULL;
  const char *failure = NULL;

  if (filter_item) {
    source.kind = URSATZ_GUI_STYLE_SOURCE_FILTER;
    const cJSON *composer = cJSON_GetObjectItemCaseSensitive(filter_item, "composer");
    const cJSON *filter_source = cJSON_GetObjectItemCaseSensitive(filter_item, "source");
    const cJSON *tag = cJSON_GetObjectItemCaseSensitive(filter_item, "tag");
    if (!cJSON_IsObject(filter_item) || (composer && !cJSON_IsString(composer) && !cJSON_IsNull(composer)) ||
        (filter_source && !cJSON_IsString(filter_source) && !cJSON_IsNull(filter_source)) ||
        (tag && !cJSON_IsString(tag) && !cJSON_IsNull(tag))) {
      failure = "invalid filter";
    } else {
      source.composer = cJSON_IsString(composer) ? composer->valuestring : NULL;
      source.source = cJSON_IsString(filter_source) ? filter_source->valuestring : NULL;
      source.tag = cJSON_IsString(tag) ? tag->valuestring : NULL;
    }
  } else if (collection_item) {
    source.kind = URSATZ_GUI_STYLE_SOURCE_COLLECTION;
    if (!cJSON_IsString(collection_item) || !ursatz_gui_collection_is_valid_id(collection_item->valuestring)) {
      failure = "invalid collection_id";
    } else {
      source.collection_id = collection_item->valuestring;
    }
  } else {
    source.kind = URSATZ_GUI_STYLE_SOURCE_PIECES;
    int length = cJSON_IsArray(ids_item) ? cJSON_GetArraySize(ids_item) : 0;
    if (length <= 0) {
      failure = "invalid piece_ids";
    } else if ((size_t)length > kMaxPieceIds) {
      failure = "too many pieces";
    } else {
      ids = calloc((size_t)length, sizeof(*ids));
      if (!ids) {
        cJSON_Delete(body);
        return send_failure(conn, 500, "out of memory");
      }
      const cJSON *item = NULL;
      cJSON_ArrayForEach(item, ids_item) {
        if (!cJSON_IsString(item) || !ursatz_gui_collection_is_valid_id(item->valuestring)) {
          failure = "invalid piece_ids";
          break;
        }
        ids[source.piece_count++] = item->valuestring;
      }
      source.piece_ids = ids;
    }
  }

  if (failure) {
    free(ids);
    cJSON_Delete(body);
    return send_failure(conn, 400, failure);
  }

  char *id = NULL;
  UrsatzGuiStyleResult result =
      ursatz_gui_style_build(name_item ? name_item->valuestring : NULL, &source, &id, NULL, NULL);
  free(ids);
  cJSON_Delete(body);
  if (result != URSATZ_GUI_STYLE_OK) {
    return source.kind == URSATZ_GUI_STYLE_SOURCE_COLLECTION && result == URSATZ_GUI_STYLE_ERR_NOT_FOUND
               ? send_failure(conn, 404, "collection not found")
               : send_style_failure(conn, result);
  }
  int status = send_built_style(conn, id);
  free(id);
  return status;
}

static int handle_list_styles(struct mg_connection *conn) {
  UrsatzGuiStyle *styles = NULL;
  size_t count = 0;
  UrsatzGuiStyleResult result = ursatz_gui_style_list(&styles, &count);
  if (result != URSATZ_GUI_STYLE_OK) {
    return send_style_failure(conn, result);
  }

  cJSON *object = cJSON_CreateObject();
  cJSON *list = object ? cJSON_AddArrayToObject(object, "styles") : NULL;
  bool built = list && cJSON_AddBoolToObject(object, "ok", 1);
  for (size_t i = 0; built && i < count; i++) {
    cJSON *entry = entry_json(&styles[i]);
    built = entry != NULL;
    if (built) {
      cJSON_AddItemToArray(list, entry);
    }
  }
  ursatz_gui_style_free_list(styles, count);
  if (!built) {
    cJSON_Delete(object);
    return send_failure(conn, 500, "out of memory");
  }
  return send_object(conn, object);
}

static int handle_get_style(struct mg_connection *conn, const char *id) {
  UrsatzGuiStyle *style = NULL;
  UrsatzGuiStyleResult result = ursatz_gui_style_get(id, &style);
  if (result != URSATZ_GUI_STYLE_OK) {
    return send_style_failure(conn, result);
  }
  cJSON *object = cJSON_CreateObject();
  cJSON *body = object ? style_json(style) : NULL;
  ursatz_gui_style_destroy(style);
  if (!body || !cJSON_AddBoolToObject(object, "ok", 1)) {
    cJSON_Delete(body);
    cJSON_Delete(object);
    return send_failure(conn, 500, "out of memory");
  }
  cJSON_AddItemToObject(object, "style", body);
  return send_object(conn, object);
}

static int handle_rebuild_style(struct mg_connection *conn, const char *id) {
  UrsatzGuiStyleResult result = ursatz_gui_style_rebuild(id);
  if (result != URSATZ_GUI_STYLE_OK) {
    return send_style_failure(conn, result);
  }
  return send_built_style(conn, id);
}

static int handle_delete_style(struct mg_connection *conn, const char *id) {
  UrsatzGuiStyleResult result = ursatz_gui_style_delete(id);
  if (result != URSATZ_GUI_STYLE_OK) {
    return send_style_failure(conn, result);
  }
  send_json(conn, "{\"ok\":true}");
  return 200;
}

/*
 * Single handler registered at kStylesPrefix ("/api/styles"). civetweb routes
 * both the exact prefix and any "prefix/something" URI to a handler
 * registered at prefix (see the identical note in corpus.c), so every styles
 * route arrives here and is dispatched by local_uri.
 */
static int styles_handler(struct mg_connection *conn, void *cbdata) {
  (void)cbdata;
  const struct mg_request_info *info = mg_get_request_info(conn);
  const char *uri = info->local_uri;
  const char *method = info->request_method;
  size_t prefix_len = strlen(kStylesPrefix);

  if (strcmp(uri, kStylesPrefix) == 0) {
    if (strcmp(method, "POST") == 0) {
      return handle_build_style(conn);
    }
    if (strcmp(method, "GET") == 0) {
      return handle_list_styles(conn);
    }
    mg_send_http_error(conn, 405, "Method Not Allowed");
    return 405;
  }

  if (strncmp(uri, kStylesPrefix, prefix_len) != 0 || uri[prefix_len] != '/') {
    mg_send_http_error(conn, 404, "Not Found");
    return 404;
  }

  /* rest is "<id>" or "<id>/rebuild". */
  const char *rest = uri + prefix_len + 1;
  const char *slash = strchr(rest, '/');
  size_t id_len = slash ? (size_t)(slash - rest) : strlen(rest);
  if (id_len == 0 || id_len >= 256) {
    mg_send_http_error(conn, 404, "Not Found");
    return 404;
  }
  char id[256];
  memcpy(id, rest, id_len);
  id[id_len] = '\0';

  /* An id that cannot name a style is an unknown style. */
  if (!ursatz_gui_collection_is_valid_id(id)) {
    return send_failure(conn, 404, "style not found");
  }

  if (!slash) {
    if (strcmp(method, "GET") == 0) {
      return handle_get_style(conn, id);
    }
    if (strcmp(method, "DELETE") == 0) {
      return handle_delete_style(conn, id);
    }
    mg_send_http_error(conn, 405, "Method Not Allowed");
    return 405;
  }

  if (strcmp(slash, "/rebuild") == 0) {
    if (strcmp(method, "POST") == 0) {
      return handle_rebuild_style(conn, id);
    }
    mg_send_http_error(conn, 405, "Method Not Allowed");
    return 405;
  }

  mg_send_http_error(conn, 404, "Not Found");
  return 404;
}

void ursatz_gui_styles_register(struct mg_context *ctx) {
  mg_set_request_handler(ctx, kStylesPrefix, styles_handler, NULL);
}
