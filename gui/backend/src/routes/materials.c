#include "materials.h"
#include "workshop_internal.h"

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
#include "motifs.h"

static const char *kMaterialsPrefix = "/api/materials";

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




/* The text of a refusal to build a playback; every one is a 500 (the file came from the Composer, not the request). */
static const char *playback_failure_text(UrsatzStatus result) {
  return result == URSATZ_ERR_ALLOC ? "out of memory" : "could not build the playback from the MIDI file";
}

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
  cJSON *params = cJSON_CreateObject();
  if (params && query_value(conn, "force", force, sizeof(force))) {
    cJSON_AddStringToObject(params, "force", force);
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
  return ursatz_gui_answer_command(conn, "item.reanalyze", id, NULL, NULL, failed_material_cmd, false);
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
  cJSON *params = cJSON_CreateObject();
  if (params) {
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

static int route_items(struct mg_connection *conn, const char *method, char segments[][256], int count) {
  bool get = strcmp(method, "GET") == 0;
  bool post = strcmp(method, "POST") == 0;
  if (count == 1) {
    if (get) {
      return handle_list_items(conn);
    }
    return post ? handle_create_item(conn) : send_method_not_allowed(conn);
  }
  if (count == 2 && strcmp(segments[1], "validate") == 0) {
    return post ? ursatz_gui_motifs_validate(conn) : send_method_not_allowed(conn);
  }
  if (count == 2) {
    if (get) {
      return handle_get_item(conn, segments[1]);
    }
    if (strcmp(method, "PATCH") == 0) {
      return handle_patch_item(conn, segments[1]);
    }
    return strcmp(method, "DELETE") == 0 ? handle_delete_item(conn, segments[1]) : send_method_not_allowed(conn);
  }
  if (count == 3 && strcmp(segments[2], "versions") == 0) {
    return post ? handle_add_version(conn, segments[1]) : send_method_not_allowed(conn);
  }
  if (count == 3 && strcmp(segments[2], "export") == 0) {
    return get ? handle_get_item_export(conn, segments[1]) : send_method_not_allowed(conn);
  }
  if (count == 3 && strcmp(segments[2], "reanalyze") == 0) {
    return post ? handle_reanalyze(conn, segments[1]) : send_method_not_allowed(conn);
  }
  if (count == 4 && strcmp(segments[2], "versions") == 0) {
    return get ? handle_get_version(conn, segments[1], segments[3]) : send_method_not_allowed(conn);
  }
  if (count == 5 && strcmp(segments[2], "versions") == 0 &&
      (strcmp(segments[4], "score") == 0 || strcmp(segments[4], "playback") == 0)) {
    return get ? handle_version_render(conn, segments[1], segments[3], strcmp(segments[4], "playback") == 0)
               : send_method_not_allowed(conn);
  }
  if (count == 5 && strcmp(segments[2], "versions") == 0 && strcmp(segments[4], "regenerate") == 0) {
    return post ? handle_piece_regenerate(conn, segments[1], segments[3]) : send_method_not_allowed(conn);
  }
  if (count == 5 && strcmp(segments[2], "versions") == 0 && strcmp(segments[4], "variations") == 0) {
    return post ? handle_piece_variations(conn, segments[1], segments[3]) : send_method_not_allowed(conn);
  }
  if (count == 8 && strcmp(segments[2], "versions") == 0 && strcmp(segments[4], "variations") == 0 &&
      strcmp(segments[7], "playback") == 0) {
    return get ? handle_piece_variation_playback(conn, segments[1], segments[3], segments[5], segments[6])
               : send_method_not_allowed(conn);
  }
  if (count == 7 && strcmp(segments[2], "versions") == 0 && strcmp(segments[4], "variations") == 0) {
    if (strcmp(segments[6], "choose") == 0) {
      return post ? handle_piece_choose(conn, segments[1], segments[3], segments[5]) : send_method_not_allowed(conn);
    }
    return get ? handle_piece_variation_file(conn, segments[1], segments[3], segments[5], segments[6])
               : send_method_not_allowed(conn);
  }
  if (count == 6 && strcmp(segments[2], "versions") == 0 && strcmp(segments[4], "files") == 0) {
    return get ? handle_version_file(conn, segments[1], segments[3], segments[5]) : send_method_not_allowed(conn);
  }
  if (count == 3 && strcmp(segments[2], "compose") == 0) {
    return post ? handle_compose(conn, segments[1]) : send_method_not_allowed(conn);
  }
  if (count == 3 && strcmp(segments[2], "variants") == 0) {
    return post ? ursatz_gui_motifs_variants(conn, segments[1]) : send_method_not_allowed(conn);
  }
  if (count == 3 && strcmp(segments[2], "op-checks") == 0) {
    if (get) {
      return ursatz_gui_motifs_op_checks_get(conn, segments[1]);
    }
    return strcmp(method, "PUT") == 0 ? ursatz_gui_motifs_op_checks_put(conn, segments[1]) : send_method_not_allowed(conn);
  }
  if (count == 3 && strcmp(segments[2], "each-op") == 0) {
    return post ? ursatz_gui_motifs_each_op(conn, segments[1]) : send_method_not_allowed(conn);
  }
  if (count == 4 && strcmp(segments[2], "variants") == 0 && strcmp(segments[3], "save") == 0) {
    return post ? ursatz_gui_motifs_item_command(conn, segments[1], "item.save-variant") : send_method_not_allowed(conn);
  }
  if (count == 3 && strcmp(segments[2], "variant-tree") == 0) {
    return get ? ursatz_gui_motifs_variant_tree(conn, segments[1]) : send_method_not_allowed(conn);
  }
  if (count == 3 && strcmp(segments[2], "reroll") == 0) {
    return post ? ursatz_gui_motifs_reroll(conn, segments[1]) : send_method_not_allowed(conn);
  }
  if (count == 3 && strcmp(segments[2], "link-form") == 0) {
    return post ? ursatz_gui_motifs_item_command(conn, segments[1], "item.link-form") : send_method_not_allowed(conn);
  }
  if (count == 3 && strcmp(segments[2], "promote") == 0) {
    return post ? ursatz_gui_motifs_item_command(conn, segments[1], "item.promote") : send_method_not_allowed(conn);
  }
  if (count == 3 && strcmp(segments[2], "flag") == 0) {
    return post ? ursatz_gui_motifs_item_command(conn, segments[1], "item.flag") : send_method_not_allowed(conn);
  }
  if (count == 5 && strcmp(segments[2], "uses") == 0 && strcmp(segments[4], "update") == 0) {
    return post ? handle_update_use(conn, segments[1], segments[3]) : send_method_not_allowed(conn);
  }
  return send_not_found(conn);
}


static int route_projects(struct mg_connection *conn, const char *method, char segments[][256], int count) {
  if (count == 1) {
    if (strcmp(method, "GET") == 0) {
      return handle_list_projects(conn);
    }
    if (strcmp(method, "POST") == 0) {
      return handle_create_project(conn);
    }
    return send_method_not_allowed(conn);
  }
  if (count == 2) {
    if (strcmp(method, "GET") == 0) {
      return handle_get_project(conn, segments[1]);
    }
    if (strcmp(method, "PATCH") == 0) {
      return handle_patch_project(conn, segments[1]);
    }
    if (strcmp(method, "DELETE") == 0) {
      return handle_delete_project(conn, segments[1]);
    }
    return send_method_not_allowed(conn);
  }
  return send_not_found(conn);
}



/*
 * Handler registered at "/api/materials". civetweb sends the exact prefix
 * and any "prefix/something" URI here (see the note in styles.c), so every
 * materials route is dispatched by its path segments.
 */
static int materials_handler(struct mg_connection *conn, void *cbdata) {
  (void)cbdata;
  const struct mg_request_info *info = mg_get_request_info(conn);
  const char *method = info->request_method;
  char segments[8][256];
  int count = split_path(info->local_uri + strlen(kMaterialsPrefix), segments);
  if (count < 1) {
    return send_not_found(conn);
  }
  if (strcmp(segments[0], "projects") == 0) {
    return route_projects(conn, method, segments, count);
  }
  if (strcmp(segments[0], "items") == 0) {
    return route_items(conn, method, segments, count);
  }
  if (count == 1 && strcmp(segments[0], "variant-operations") == 0) {
    return strcmp(method, "GET") == 0 ? ursatz_gui_motifs_variant_operations(conn) : send_method_not_allowed(conn);
  }
  if (count == 1 && strcmp(segments[0], "recognize") == 0) {
    return strcmp(method, "POST") == 0 ? ursatz_gui_motifs_recognize(conn) : send_method_not_allowed(conn);
  }
  if (count == 1 && strcmp(segments[0], "render") == 0) {
    return strcmp(method, "POST") == 0 ? ursatz_gui_motifs_render(conn) : send_method_not_allowed(conn);
  }
  if (count == 2 && strcmp(segments[0], "motifs") == 0 && strcmp(segments[1], "random") == 0) {
    return strcmp(method, "POST") == 0 ? ursatz_gui_motifs_random_motifs(conn) : send_method_not_allowed(conn);
  }
  if (count == 2 && strcmp(segments[0], "motifs") == 0 && strcmp(segments[1], "import") == 0) {
    return strcmp(method, "POST") == 0 ? ursatz_gui_motifs_import_motif(conn) : send_method_not_allowed(conn);
  }
  return send_not_found(conn);
}

void ursatz_gui_materials_register(struct mg_context *ctx) {
  mg_set_request_handler(ctx, kMaterialsPrefix, materials_handler, NULL);
}
