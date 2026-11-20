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
  return QUERY(conn, "project.list", failed_material, false, NULL);
}

static int handle_create_project(struct mg_connection *conn) {
  return command_with_body(conn, "project.create", "", failed_material_cmd);
}

static int handle_get_project(struct mg_connection *conn, const char *id) {
  return QUERY(conn, "project.get", failed_material, false, "id", id);
}

static int handle_patch_project(struct mg_connection *conn, const char *id) {
  return command_with_body(conn, "project.update", id, failed_material_cmd);
}

static int handle_delete_project(struct mg_connection *conn, const char *id) {
  return ursatz_gui_answer_command(conn, "project.delete", id, NULL, NULL, failed_material_cmd, false);
}

/* --- items and versions -------------------------------------------------- */

static int handle_list_items(struct mg_connection *conn) {
  return QUERY(conn, "item.list", failed_material, false, NULL);
}

/* ---- motif import from a MIDI file ------------------------------------ */




static int handle_create_item(struct mg_connection *conn) {
  return command_with_body(conn, "item.create", "", failed_material_cmd);
}

/* The item answer: {"ok":true,"item":meta,"body":current,"layers":...,"uses_status":[...],"used_by":[...],"subjects":[...]}. */
static int handle_get_item(struct mg_connection *conn, const char *id) {
  return QUERY(conn, "item.get", failed_material, false, "id", id);
}

static int handle_patch_item(struct mg_connection *conn, const char *id) {
  return command_with_body(conn, "item.update", id, failed_material_cmd);
}

static int handle_delete_item(struct mg_connection *conn, const char *id) {
  char force[8];
  char base[16];
  cJSON *params = cJSON_CreateObject();
  if (params && query_value(conn, "force", force, sizeof(force))) {
    cJSON_AddStringToObject(params, "force", force);
  }
  if (params && query_value(conn, "base_version", base, sizeof(base))) {
    cJSON_AddStringToObject(params, "base_version", base);
  }
  WorkshopReply reply;
  UrsatzError error = {{0}, {0}};
  UrsatzStatus status = ursatz_gui_run_command("item.delete", id, params, NULL, &reply, &error);
  if (status == URSATZ_ERR_CONFLICT) {
    /* A delete another item blocks answers 409 with {"ok": false, "error", "used_by": [...]}. */
    cJSON *used_by = reply.json ? cJSON_DetachItemFromObjectCaseSensitive(reply.json, "used_by") : NULL;
    cJSON *object = cJSON_CreateObject();
    workshop_reply_destroy(&reply);
    if (!object || !used_by || !cJSON_AddBoolToObject(object, "ok", 0) || !cJSON_AddStringToObject(object, "error", error.message)) {
      cJSON_Delete(used_by);
      cJSON_Delete(object);
      return send_failure(conn, 500, "out of memory");
    }
    cJSON_AddItemToObject(object, "used_by", used_by);
    return ursatz_gui_send_object(conn, 409, object);
  }
  if (status == URSATZ_ERR_CONFLICT && reply.json && cJSON_GetObjectItemCaseSensitive(reply.json, "current_version")) {
    cJSON *object = cJSON_CreateObject();
    cJSON *current = cJSON_DetachItemFromObjectCaseSensitive(reply.json, "current_version");
    workshop_reply_destroy(&reply);
    if (!object || !cJSON_AddBoolToObject(object, "ok", 0) || !cJSON_AddStringToObject(object, "error", error.message)) {
      cJSON_Delete(object);
      cJSON_Delete(current);
      return send_failure(conn, 500, "out of memory");
    }
    cJSON_AddItemToObject(object, "current_version", current);
    return ursatz_gui_send_object(conn, 409, object);
  }
  if (status != URSATZ_OK) {
    workshop_reply_destroy(&reply);
    return failed_material_cmd(conn, status, &error);
  }
  return ursatz_gui_send_reply_ok(conn, 200, &reply);
}

static int handle_add_version(struct mg_connection *conn, const char *id) {
  return command_with_body(conn, "item.save", id, failed_material_cmd);
}

/* POST /api/materials/items/<id>/reanalyze: analyzes the current version of a motif again and saves the result as a
   new version (versions are never rewritten, R-130). A refusal writes nothing. */
static int handle_reanalyze(struct mg_connection *conn, const char *id) {
  return command_with_optional_body(conn, "item.reanalyze", id, failed_material_cmd);
}

static int handle_get_version(struct mg_connection *conn, const char *id, const char *number_text) {
  return QUERY(conn, "item.version", failed_version, false, "id", id, "version", number_text);
}

/* The failures of a read of a version or one of its files: a missing version or file is the error shape, the rest the store's message. */
static int failed_version(struct mg_connection *conn, UrsatzStatus status, const UrsatzError *error) {
  return status == URSATZ_ERR_NOT_FOUND ? send_failure(conn, 404, error->message) : failed_material(conn, status, error);
}

/* A version number from a path segment, 1 to 1000000. */
static bool parse_version_number(const char *text, int *out) {
  char *end = NULL;
  long number = strtol(text, &end, 10);
  if (end == text || *end != '\0' || number < 1 || number > 1000000) {
    return false;
  }
  *out = (int)number;
  return true;
}

/* POST /api/materials/items/<thm-id or pce-id>/compose {seed?, update_uses?}: composes the theme or piece into a new
   version with its files (R-167, R-174). With update_uses, every updated use moves to its source's current version first. */
static int handle_compose(struct mg_connection *conn, const char *id) {
  return command_with_body(conn, "item.compose", id, failed_material_cmd);
}

/* POST /api/materials/items/<pce-id>/versions/<n>/regenerate {section_id, lane?, seed?}: writes one section of a composed
   piece version again, as the next version (R-173, R-174). */
static int handle_piece_regenerate(struct mg_connection *conn, const char *id, const char *number_text) {
  int version = 0;
  if (!parse_version_number(number_text, &version)) {
    return send_failure(conn, 404, "version not found");
  }
  int status = 0;
  cJSON *body = read_json_body(conn, &status);
  if (!body) {
    return status;
  }
  cJSON_AddNumberToObject(body, "version", version);
  return ursatz_gui_answer_command(conn, "item.regenerate", id, body, NULL, failed_material_cmd, false);
}

/* POST /api/materials/items/<pce-id>/versions/<n>/variations {section_id, lane, count, seed?}: makes variations of a
   section of a composed piece version into a new set; the answer lists them. */
static int handle_piece_variations(struct mg_connection *conn, const char *id, const char *number_text) {
  int version = 0;
  if (!parse_version_number(number_text, &version)) {
    return send_failure(conn, 404, "version not found");
  }
  int status = 0;
  cJSON *body = read_json_body(conn, &status);
  if (!body) {
    return status;
  }
  cJSON_AddNumberToObject(body, "version", version);
  return ursatz_gui_answer_command(conn, "item.variations", id, body, NULL, failed_material_cmd, false);
}

/* GET /api/materials/items/<pce-id>/versions/<n>/variations/<set>/<file>: a file of a variations set. */
static int handle_piece_variation_file(struct mg_connection *conn, const char *id, const char *number_text, const char *set_text,
                                       const char *name) {
  return QUERY(conn, "item.variation-file", failed_material_error, false, "id", id, "version", number_text, "set", set_text, "name", name);
}

/* POST /api/materials/items/<pce-id>/versions/<n>/variations/<set>/choose {index}: makes a variation the next version. */
static int handle_piece_choose(struct mg_connection *conn, const char *id, const char *number_text, const char *set_text) {
  int version = 0;
  int set = 0;
  if (!parse_version_number(number_text, &version) || !parse_version_number(set_text, &set)) {
    return send_failure(conn, 404, "variations not found");
  }
  int status = 0;
  cJSON *body = read_json_body(conn, &status);
  if (!body) {
    return status;
  }
  cJSON_AddNumberToObject(body, "version", version);
  cJSON_AddNumberToObject(body, "set", set);
  return ursatz_gui_answer_command(conn, "item.choose", id, body, NULL, failed_material_cmd, false);
}

/* GET /api/materials/items/<id>/versions/<n>/files/<name>: one of a theme or piece version's files. */
static int handle_version_file(struct mg_connection *conn, const char *id, const char *number_text, const char *name) {
  return QUERY(conn, "item.file", failed_version, false, "id", id, "version", number_text, "name", name);
}

/* GET /api/materials/items/<pce-id>/versions/<n>/variations/<set>/<file>/playback: the playback of a variation's MIDI
   file (variation-<i>.mid). */
static int handle_piece_variation_playback(struct mg_connection *conn, const char *id, const char *number_text,
                                           const char *set_text, const char *name) {
  return QUERY(conn, "item.variation-playback", failed_material_error, true, "id", id, "version", number_text, "set", set_text, "name", name);
}

/* GET /api/materials/items/<id>/versions/<n>/score and .../playback, with ?key= for the key to sound it in (default: the motif's own
   key). With ?file= the playback route answers for a theme or piece version's own MIDI file instead. */
static int handle_version_render(struct mg_connection *conn, const char *id, const char *number_text, bool playback_only) {
  return QUERY(conn, playback_only ? "item.playback" : "item.score", failed_material_error, true, "id", id, "version", number_text);
}

/* ---- variants ---------------------------------------------------------- */














static int handle_update_use(struct mg_connection *conn, const char *id, const char *source_id) {
  int status = 0;
  cJSON *params = NULL;
  if (mg_get_request_info(conn)->content_length > 0) {
    params = read_json_body(conn, &status);
    if (!params) {
      return status;
    }
  } else {
    params = cJSON_CreateObject();
  }
  if (params) {
    cJSON_DeleteItemFromObjectCaseSensitive(params, "source_id");
    cJSON_AddStringToObject(params, "source_id", source_id);
  }
  return ursatz_gui_answer_command(conn, "item.update-use", id, params, NULL, failed_material_cmd, false);
}


/* ---- validate ----------------------------------------------------------- */




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
static int route_project_create(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { (void)args; return handle_create_project(conn); }
static int route_project(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_get_project(conn, ARG(args, 0)); }
static int route_project_patch(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_patch_project(conn, ARG(args, 0)); }
static int route_project_delete(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_delete_project(conn, ARG(args, 0)); }
static int route_items(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { (void)args; return handle_list_items(conn); }
static int route_item_create(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { (void)args; return handle_create_item(conn); }
static int route_item(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_get_item(conn, ARG(args, 0)); }
static int route_item_patch(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_patch_item(conn, ARG(args, 0)); }
static int route_item_delete(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_delete_item(conn, ARG(args, 0)); }
static int route_save(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_add_version(conn, ARG(args, 0)); }
static int route_export(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_get_item_export(conn, ARG(args, 0)); }
static int route_reanalyze(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_reanalyze(conn, ARG(args, 0)); }
static int route_version(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_get_version(conn, ARG(args, 0), ARG(args, 1)); }
static int route_score(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_version_render(conn, ARG(args, 0), ARG(args, 1), false); }
static int route_playback(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_version_render(conn, ARG(args, 0), ARG(args, 1), true); }
static int route_regenerate(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_piece_regenerate(conn, ARG(args, 0), ARG(args, 1)); }
static int route_variations(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_piece_variations(conn, ARG(args, 0), ARG(args, 1)); }
static int route_choose(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_piece_choose(conn, ARG(args, 0), ARG(args, 1), ARG(args, 2)); }
static int route_variation_file(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) {
  return handle_piece_variation_file(conn, ARG(args, 0), ARG(args, 1), ARG(args, 2), ARG(args, 3));
}
static int route_variation_playback(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) {
  return handle_piece_variation_playback(conn, ARG(args, 0), ARG(args, 1), ARG(args, 2), ARG(args, 3));
}
static int route_file(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_version_file(conn, ARG(args, 0), ARG(args, 1), ARG(args, 2)); }
static int route_compose(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_compose(conn, ARG(args, 0)); }
static int route_update_use(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return handle_update_use(conn, ARG(args, 0), ARG(args, 1)); }

static const UrsatzGuiRoute kRoutes[] = {
    {"GET", "/api/materials/projects", route_projects},
    {"POST", "/api/materials/projects", route_project_create},
    {"GET", "/api/materials/projects/<id>", route_project},
    {"PATCH", "/api/materials/projects/<id>", route_project_patch},
    {"DELETE", "/api/materials/projects/<id>", route_project_delete},
    {"GET", "/api/materials/items", route_items},
    {"POST", "/api/materials/items", route_item_create},
    {"GET", "/api/materials/items/<id>", route_item},
    {"PATCH", "/api/materials/items/<id>", route_item_patch},
    {"DELETE", "/api/materials/items/<id>", route_item_delete},
    {"POST", "/api/materials/items/<id>/versions", route_save},
    {"GET", "/api/materials/items/<id>/export", route_export},
    {"POST", "/api/materials/items/<id>/reanalyze", route_reanalyze},
    {"GET", "/api/materials/items/<id>/versions/<n>", route_version},
    {"GET", "/api/materials/items/<id>/versions/<n>/score", route_score},
    {"GET", "/api/materials/items/<id>/versions/<n>/playback", route_playback},
    {"POST", "/api/materials/items/<id>/versions/<n>/regenerate", route_regenerate},
    {"POST", "/api/materials/items/<id>/versions/<n>/variations", route_variations},
    {"POST", "/api/materials/items/<id>/versions/<n>/variations/<n>/choose", route_choose},
    {"GET", "/api/materials/items/<id>/versions/<n>/variations/<n>/<name>", route_variation_file},
    {"GET", "/api/materials/items/<id>/versions/<n>/variations/<n>/<name>/playback", route_variation_playback},
    {"GET", "/api/materials/items/<id>/versions/<n>/files/<name>", route_file},
    {"POST", "/api/materials/items/<id>/compose", route_compose},
    {"POST", "/api/materials/items/<id>/uses/<id>/update", route_update_use},
};

const UrsatzGuiRoute *ursatz_gui_materials_routes(size_t *count) {
  *count = sizeof(kRoutes) / sizeof(kRoutes[0]);
  return kRoutes;
}
