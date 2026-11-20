#include "library_routes.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <civetweb.h>
#include <cJSON.h>

#include "route_util.h"


/* Runs a Library query for the route's id and query variables and answers {"ok": true, ...} or the failure envelope. */
static int run_library_query(struct mg_connection *conn, const char *name, const char *id) {
  return ursatz_gui_answer_query(conn, name, ursatz_gui_request_params(conn, id ? "id" : NULL, id, NULL), NULL, ursatz_gui_failed, false);
}

static int handle_get(struct mg_connection *conn) {
  return run_library_query(conn, "library.list", NULL);
}

/* GET /api/analyzer/groups: the distinct composers, sources and tags with piece counts. */
static int handle_get_groups(struct mg_connection *conn) {
  return run_library_query(conn, "library.groups", NULL);
}

static int handle_get_piece(struct mg_connection *conn, const char *id) {
  return run_library_query(conn, "library.piece", id);
}

/* GET /api/analyzer/pieces/<id>/document?sections=a,b: only those sections of the stored analysis document, read without importing the
   source. */
static int handle_get_piece_document(struct mg_connection *conn, const char *id) {
  return run_library_query(conn, "library.document", id);
}

/* A measure number of the export range from the query: digits only, at most 6. The first of a repeated variable. */
static bool query_measure(const cJSON *params, const char *name, int *out, bool *present) {
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(params, name);
  if (cJSON_IsArray(item)) {
    item = cJSON_GetArrayItem(item, 0);
  }
  *present = item != NULL;
  if (!item) {
    return true;
  }
  if (!cJSON_IsString(item) || item->valuestring[0] == '\0' || strlen(item->valuestring) > 6) {
    return false;
  }
  int value = 0;
  for (const char *p = item->valuestring; *p; p++) {
    if (*p < '0' || *p > '9') {
      return false;
    }
    value = value * 10 + (*p - '0');
  }
  *out = value;
  return true;
}

/* GET /api/analyzer/pieces/<id>/export[?from=<m>&to=<m>]: the Analyzer's text export of the piece framed as its reading view (library.view,
 * view reading), answered as text/plain. */
static int handle_get_piece_export(struct mg_connection *conn, const char *id) {
  cJSON *query = ursatz_gui_request_params(conn, NULL);
  UrsatzError error = {{0}, {0}};
  int first = 0, last = 0;
  bool has_first = false, has_last = false;
  if (!query || !query_measure(query, "from", &first, &has_first) || !query_measure(query, "to", &last, &has_last) ||
      (has_first && has_last && first > last)) {
    cJSON_Delete(query);
    snprintf(error.message, sizeof(error.message), "from and to must be measure numbers, from not after to");
    snprintf(error.path, sizeof(error.path), "/params/measures");
    return ursatz_gui_failed(conn, URSATZ_ERR_REQUEST, &error);
  }
  cJSON_Delete(query);
  cJSON *params = cJSON_CreateObject();
  cJSON *measures = params ? cJSON_AddObjectToObject(params, "measures") : NULL;
  if (!measures || !cJSON_AddStringToObject(params, "id", id) || !cJSON_AddStringToObject(params, "view", "reading") ||
      (has_first && !cJSON_AddNumberToObject(measures, "first", first)) || (has_last && !cJSON_AddNumberToObject(measures, "last", last))) {
    cJSON_Delete(params);
    return ursatz_gui_send_failure(conn, 500, "out of memory");
  }
  WorkshopReply reply;
  UrsatzStatus status = ursatz_gui_run_query("library.view", params, NULL, &reply, &error);
  const cJSON *text = status == URSATZ_OK ? cJSON_GetObjectItemCaseSensitive(reply.json, "text") : NULL;
  if (status != URSATZ_OK || !cJSON_IsString(text)) {
    workshop_reply_destroy(&reply);
    return status != URSATZ_OK ? ursatz_gui_failed(conn, status, &error) : ursatz_gui_send_failure(conn, 500, "the reading view has no text");
  }
  WorkshopReply plain;
  memset(&plain, 0, sizeof(plain));
  size_t length = strlen(text->valuestring);
  plain.bytes = malloc(length + 1);
  if (!plain.bytes) {
    workshop_reply_destroy(&reply);
    return ursatz_gui_send_failure(conn, 500, "out of memory");
  }
  memcpy(plain.bytes, text->valuestring, length + 1);
  plain.size = length;
  snprintf(plain.content_type, sizeof(plain.content_type), "text/plain; charset=utf-8");
  workshop_reply_destroy(&reply);
  int sent = ursatz_gui_send_reply_bytes(conn, &plain);
  workshop_reply_destroy(&plain);
  return sent;
}

static int handle_get_piece_musicxml(struct mg_connection *conn, const char *id) {
  return run_library_query(conn, "library.musicxml", id);
}

static int handle_get_piece_musicxml_meta(struct mg_connection *conn, const char *id) {
  return run_library_query(conn, "library.meter-defaulted", id);
}

/* The playback document is the answer as the Analyzer made it (it holds its own "ok"). */
static int handle_get_piece_playback(struct mg_connection *conn, const char *id) {
  return run_library_query(conn, "library.playback", id);
}

#define ARG(args, i) ((args)->values[i])

static int route_list(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { (void)args; return handle_get(conn); }
static int route_groups(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { (void)args; return handle_get_groups(conn); }
static int route_piece(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_get_piece(conn, ARG(args, 0)); }
static int route_document(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_get_piece_document(conn, ARG(args, 0)); }
static int route_export(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_get_piece_export(conn, ARG(args, 0)); }
static int route_musicxml(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_get_piece_musicxml(conn, ARG(args, 0)); }
static int route_meter(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_get_piece_musicxml_meta(conn, ARG(args, 0)); }
static int route_playback(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_get_piece_playback(conn, ARG(args, 0)); }

static const UrsatzGuiRoute kRoutes[] = {
    {"GET", "/api/analyzer/pieces", route_list},
    {"GET", "/api/analyzer/groups", route_groups},
    {"GET", "/api/analyzer/pieces/<id>", route_piece},
    {"GET", "/api/analyzer/pieces/<id>/document", route_document},
    {"GET", "/api/analyzer/pieces/<id>/export", route_export},
    {"GET", "/api/analyzer/pieces/<id>/musicxml", route_musicxml},
    {"GET", "/api/analyzer/pieces/<id>/musicxml/meta", route_meter},
    {"GET", "/api/analyzer/pieces/<id>/playback", route_playback},
};

const UrsatzGuiRoute *ursatz_gui_library_routes(size_t *count) {
  *count = sizeof(kRoutes) / sizeof(kRoutes[0]);
  return kRoutes;
}
