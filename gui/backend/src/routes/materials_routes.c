#include "materials_routes.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <civetweb.h>
#include <ctype.h>
#include <math.h>
#include <unistd.h>

#include <pthread.h>

#include <common/files.h>
#include <common/json.h>
#include <common/status.h>

#include "materials_util.h"
#include "route_table.h"


static int failed_version(struct mg_connection *conn, UrsatzStatus status, const UrsatzError *error);
static int failed_export(struct mg_connection *conn, UrsatzStatus status, const UrsatzError *error);

/* --- projects ------------------------------------------------------------ */

static int handle_list_projects(struct mg_connection *conn) {
  return QUERY(conn, "project.list", ursatz_gui_failed, false, NULL);
}

static int handle_get_project(struct mg_connection *conn, const char *id) {
  return QUERY(conn, "project.get", ursatz_gui_failed, false, "id", id);
}

/* --- items and versions -------------------------------------------------- */

static int handle_list_items(struct mg_connection *conn) {
  return QUERY(conn, "item.list", ursatz_gui_failed, false, NULL);
}

/* The item answer: {"ok":true,"item":meta,"body":current,"layers":...,"uses_status":[...],"used_by":[...],"subjects":[...]}. */
static int handle_get_item(struct mg_connection *conn, const char *id) {
  return QUERY(conn, "item.get", ursatz_gui_failed, false, "id", id);
}

static int handle_get_version(struct mg_connection *conn, const char *id, const char *number_text) {
  return QUERY(conn, "item.version", failed_version, false, "id", id, "version", number_text);
}

/* The failures of a read of a version or one of its files: a missing version or file is the error shape, the rest the store's message. */
static int failed_version(struct mg_connection *conn, UrsatzStatus status, const UrsatzError *error) {
  return status == URSATZ_ERR_NOT_FOUND ? send_failure(conn, 404, error->message) : ursatz_gui_failed(conn, status, error);
}

/* GET /api/materials/items/<pce-id>/versions/<n>/variations/<set>/<file>: a file of a variations set. */
static int handle_piece_variation_file(struct mg_connection *conn, const char *id, const char *number_text, const char *set_text,
                                       const char *name) {
  return QUERY(conn, "item.variation-file", ursatz_gui_failed, false, "id", id, "version", number_text, "set", set_text, "name", name);
}

/* GET /api/materials/items/<id>/versions/<n>/files/<name>: one of a theme or piece version's files. */
static int handle_version_file(struct mg_connection *conn, const char *id, const char *number_text, const char *name) {
  return QUERY(conn, "item.file", failed_version, false, "id", id, "version", number_text, "name", name);
}

/* GET /api/materials/items/<pce-id>/versions/<n>/variations/<set>/<file>/playback: the playback of a variation's MIDI
   file (variation-<i>.mid). */
static int handle_piece_variation_playback(struct mg_connection *conn, const char *id, const char *number_text,
                                           const char *set_text, const char *name) {
  return QUERY(conn, "item.variation-playback", ursatz_gui_failed, true, "id", id, "version", number_text, "set", set_text, "name", name);
}

/* GET /api/materials/items/<id>/versions/<n>/score and .../playback, with ?key= for the key to sound it in (default: the motif's own
   key). With ?file= the playback route answers for a theme or piece version's own MIDI file instead. */
static int handle_version_render(struct mg_connection *conn, const char *id, const char *number_text, bool playback_only) {
  return QUERY(conn, playback_only ? "item.playback" : "item.score", ursatz_gui_failed, true, "id", id, "version", number_text);
}

/* The failures of an item export: a motif the Composer cannot render is a 422, a version that is no number a 400. */
static int failed_export(struct mg_connection *conn, UrsatzStatus status, const UrsatzError *error) {
  if (status == URSATZ_ERR_REQUEST) {
    return send_failure(conn, strncmp(error->path, "/params/", 8) == 0 ? 400 : 422, error->message);
  }
  return failed_version(conn, status, error);
}

/* GET /api/materials/items/<id>/export[?version=<n>]: the Analyzer's text export of a motif, theme or piece (text/plain).
   The current version unless version is given. */
static int handle_get_item_export(struct mg_connection *conn, const char *id) {
  return QUERY(conn, "item.export", failed_export, false, "id", id);
}

#define ARG(args, i) ((args)->values[i])

static int route_projects(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { (void)args; return handle_list_projects(conn); }
static int route_project(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_get_project(conn, ARG(args, 0)); }
static int route_items(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { (void)args; return handle_list_items(conn); }
static int route_item(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_get_item(conn, ARG(args, 0)); }
static int route_export(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_get_item_export(conn, ARG(args, 0)); }
static int route_version(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_get_version(conn, ARG(args, 0), ARG(args, 1)); }
static int route_score(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_version_render(conn, ARG(args, 0), ARG(args, 1), false); }
static int route_playback(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_version_render(conn, ARG(args, 0), ARG(args, 1), true); }
static int route_variation_file(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) {
  return handle_piece_variation_file(conn, ARG(args, 0), ARG(args, 1), ARG(args, 2), ARG(args, 3));
}
static int route_variation_playback(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) {
  return handle_piece_variation_playback(conn, ARG(args, 0), ARG(args, 1), ARG(args, 2), ARG(args, 3));
}
static int route_file(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_version_file(conn, ARG(args, 0), ARG(args, 1), ARG(args, 2)); }

static const UrsatzGuiRoute kRoutes[] = {
    {"GET", "/api/materials/projects", route_projects},
    {"GET", "/api/materials/projects/<id>", route_project},
    {"GET", "/api/materials/items", route_items},
    {"GET", "/api/materials/items/<id>", route_item},
    {"GET", "/api/materials/items/<id>/export", route_export},
    {"GET", "/api/materials/items/<id>/versions/<n>", route_version},
    {"GET", "/api/materials/items/<id>/versions/<n>/score", route_score},
    {"GET", "/api/materials/items/<id>/versions/<n>/playback", route_playback},
    {"GET", "/api/materials/items/<id>/versions/<n>/variations/<n>/<name>", route_variation_file},
    {"GET", "/api/materials/items/<id>/versions/<n>/variations/<n>/<name>/playback", route_variation_playback},
    {"GET", "/api/materials/items/<id>/versions/<n>/files/<name>", route_file},
};

const UrsatzGuiRoute *ursatz_gui_materials_routes(size_t *count) {
  *count = sizeof(kRoutes) / sizeof(kRoutes[0]);
  return kRoutes;
}
