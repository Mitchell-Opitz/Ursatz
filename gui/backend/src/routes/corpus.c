#include "corpus.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <civetweb.h>

#include "route_util.h"
#include "workshop_internal.h"

static const char *kCorpusPrefix = "/api/corpus";

static void send_ok_false(struct mg_connection *conn, const char *message) {
  ursatz_gui_send_failure(conn, 200, message);
}

/* A missing collection is a 404; every other failure keeps the route's
 * 200 with {"ok":false,...}. */
static int send_collection_failure(struct mg_connection *conn, UrsatzStatus result) {
  return ursatz_gui_send_failure(conn, result == URSATZ_ERR_NOT_FOUND ? 404 : 200,
                                 ursatz_gui_failure_message("collection operation failed"));
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
  WorkshopCollection *collection = NULL;
  UrsatzStatus result = workshop_collection_get(id, &collection);
  if (result != URSATZ_OK) {
    return send_collection_failure(conn, result);
  }
  cJSON *object = cJSON_CreateObject();
  bool built = object && cJSON_AddBoolToObject(object, "ok", 1) &&
               add_summary_members(object, collection->id, collection->name, collection->description,
                                   collection->piece_count, collection->updated_at);
  workshop_collection_destroy(collection);
  if (!built) {
    cJSON_Delete(object);
    return ursatz_gui_send_failure(conn, 500, "out of memory");
  }
  return ursatz_gui_send_object(conn, 200, object);
}

static int handle_create_corpus(struct mg_connection *conn) {
  cJSON *body = ursatz_gui_read_json_body(conn, NULL);
  if (!body) {
    return ursatz_gui_send_failure(conn, 200, "invalid request body");
  }
  char *id = NULL;
  UrsatzError error = {{0}, {0}};
  UrsatzStatus result = workshop_collection_create_request(body, &id, &error);
  cJSON_Delete(body);
  if (result != URSATZ_OK) {
    return ursatz_gui_send_failure(conn, result == URSATZ_ERR_NOT_FOUND ? 404 : 200, error.message);
  }
  int status = send_collection_summary(conn, id);
  free(id);
  return status;
}

static int handle_list_corpora(struct mg_connection *conn) {
  WorkshopCollectionEntry *entries = NULL;
  size_t count = 0;
  UrsatzStatus result = workshop_collection_list(&entries, &count);
  if (result != URSATZ_OK) {
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
  workshop_collection_free_entries(entries, count);
  if (!built) {
    cJSON_Delete(object);
    return ursatz_gui_send_failure(conn, 500, "out of memory");
  }
  return ursatz_gui_send_object(conn, 200, object);
}

static int handle_get_corpus(struct mg_connection *conn, const char *id) {
  WorkshopCollection *collection = NULL;
  UrsatzStatus result = workshop_collection_get(id, &collection);
  if (result != URSATZ_OK) {
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
  workshop_collection_destroy(collection);
  if (!built) {
    cJSON_Delete(object);
    return ursatz_gui_send_failure(conn, 500, "out of memory");
  }
  return ursatz_gui_send_object(conn, 200, object);
}

static int handle_patch_corpus(struct mg_connection *conn, const char *id) {
  cJSON *body = ursatz_gui_read_json_body(conn, NULL);
  if (!body) {
    return ursatz_gui_send_failure(conn, 200, "invalid request body");
  }

  const cJSON *name_item = cJSON_GetObjectItemCaseSensitive(body, "name");
  const cJSON *description_item = cJSON_GetObjectItemCaseSensitive(body, "description");
  if (!name_item && !description_item) {
    cJSON_Delete(body);
    return ursatz_gui_send_failure(conn, 200, "nothing to update");
  }
  if (name_item && !cJSON_IsString(name_item)) {
    cJSON_Delete(body);
    return ursatz_gui_send_failure(conn, 200, "invalid name");
  }
  if (description_item && !cJSON_IsString(description_item)) {
    cJSON_Delete(body);
    return ursatz_gui_send_failure(conn, 200, "invalid description");
  }

  UrsatzStatus result = workshop_collection_update(
      id, name_item ? name_item->valuestring : NULL, description_item ? description_item->valuestring : NULL);
  cJSON_Delete(body);
  if (result != URSATZ_OK) {
    return send_collection_failure(conn, result);
  }
  return send_collection_summary(conn, id);
}

static int handle_delete_corpus(struct mg_connection *conn, const char *id) {
  UrsatzStatus result = workshop_collection_delete(id);
  if (result != URSATZ_OK) {
    return send_collection_failure(conn, result);
  }
  ursatz_gui_send_json(conn, "{\"ok\":true}");
  return 200;
}

/* Adds the ids of {"piece_id":"..."} or {"piece_ids":["...", ...]} to the
 * collection. A piece already in it is counted, not an error:
 * {"ok":true,"added":n,"already":n}. */
static int handle_add_pieces(struct mg_connection *conn, const char *corpus_id) {
  cJSON *body = ursatz_gui_read_json_body(conn, NULL);
  if (!body) {
    return ursatz_gui_send_failure(conn, 200, "invalid request body");
  }
  size_t added = 0;
  size_t already = 0;
  int status = 200;
  UrsatzError error = {{0}, {0}};
  UrsatzStatus result = workshop_collection_add_request(corpus_id, body, &added, &already, &status, &error);
  cJSON_Delete(body);
  if (result != URSATZ_OK) {
    return ursatz_gui_send_failure(conn, status, error.message);
  }
  cJSON *object = cJSON_CreateObject();
  if (!object || !cJSON_AddBoolToObject(object, "ok", 1) || !cJSON_AddNumberToObject(object, "added", (double)added) ||
      !cJSON_AddNumberToObject(object, "already", (double)already)) {
    cJSON_Delete(object);
    return ursatz_gui_send_failure(conn, 500, "out of memory");
  }
  return ursatz_gui_send_object(conn, 200, object);
}

static int handle_remove_piece(struct mg_connection *conn, const char *corpus_id, const char *piece_id) {
  UrsatzStatus result = workshop_collection_remove_piece(corpus_id, piece_id);
  if (result != URSATZ_OK) {
    return send_collection_failure(conn, result);
  }
  ursatz_gui_send_json(conn, "{\"ok\":true}");
  return 200;
}

static int handle_list_pieces(struct mg_connection *conn, const char *corpus_id) {
  WorkshopCollection *collection = NULL;
  UrsatzStatus result = workshop_collection_get(corpus_id, &collection);
  if (result != URSATZ_OK) {
    return send_collection_failure(conn, result);
  }

  cJSON *object = cJSON_CreateObject();
  cJSON *pieces = object ? cJSON_AddArrayToObject(object, "pieces") : NULL;
  bool built = pieces && cJSON_AddBoolToObject(object, "ok", 1);
  for (size_t i = 0; built && i < collection->piece_count; i++) {
    cJSON *piece = cJSON_CreateObject();
    built = piece && cJSON_AddStringToObject(piece, "id", collection->pieces[i]);
    if (built) {
      cJSON_AddItemToArray(pieces, piece);
    } else {
      cJSON_Delete(piece);
    }
  }
  workshop_collection_destroy(collection);
  if (!built) {
    cJSON_Delete(object);
    return ursatz_gui_send_failure(conn, 500, "out of memory");
  }
  return ursatz_gui_send_object(conn, 200, object);
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

  if (!workshop_collection_is_valid_id(corpus_id)) {
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
      return handle_add_pieces(conn, corpus_id);
    }
    if (strcmp(method, "GET") == 0) {
      return handle_list_pieces(conn, corpus_id);
    }
    mg_send_http_error(conn, 405, "Method Not Allowed");
    return 405;
  }

  if (after_pieces[0] == '/' && after_pieces[1] != '\0') {
    const char *piece_id = after_pieces + 1;
    if (!workshop_collection_is_valid_id(piece_id)) {
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
