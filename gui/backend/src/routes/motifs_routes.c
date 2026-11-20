#include "motifs_routes.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "materials_util.h"

/* POST /api/materials/motifs/import: the body is a MIDI file; ?meter= and ?key= are optional. Answers {"ok":true,"notes_text":...,
   "meter":"4/4","key":"C minor","material":...,"moved_from":"D minor"|null,"pickup":"0","analysis":{...}} and saves nothing. */
static int motifs_import_motif(struct mg_connection *conn) {
  int status = 0;
  size_t size = 0;
  char *bytes = NULL;
  UrsatzError read_error = {{0}, {0}};
  UrsatzStatus read = ursatz_gui_read_body(conn, URSATZ_GUI_MIDI_MAX_BYTES, &bytes, &size, &read_error);
  if (read == URSATZ_OK && size == 0) {
    free(bytes);
    return ursatz_gui_send_failure(conn, 400, "a MIDI file is required");
  }
  if (read != URSATZ_OK) {
    return ursatz_gui_failed(conn, read, &read_error);
  }
  WorkshopBytes file = {(const uint8_t *)bytes, size, NULL};
  status = ursatz_gui_answer_query(conn, "composer.import-motif", ursatz_gui_request_params(conn, NULL), &file, ursatz_gui_failed, true);
  free(bytes);
  return status;
}

/* POST /api/materials/render {form, degrees, meter, key?, pickup?, render_key?, tempo?, role?}: nothing is saved. */
static int motifs_render(struct mg_connection *conn) {
  return query_with_body(conn, "composer.render", ursatz_gui_failed, true, NULL);
}

/* GET /api/materials/variant-operations: the Composer's operation list, as the Composer API gives it. */
static int motifs_variant_operations(struct mg_connection *conn) {
  return QUERY(conn, "composer.variant-operations", ursatz_gui_failed, false, NULL);
}


/* POST /api/materials/items/<id>/variants {chain?, count?, seed?, functions?, distance?} and .../each-op {seed?}: candidates from the
   item's current degrees; nothing is saved. The answer is the job. */
static int motifs_variants(struct mg_connection *conn, const char *id) {
  return query_with_body(conn, "item.variants", ursatz_gui_failed, false, id);
}

static int motifs_each_op(struct mg_connection *conn, const char *id) {
  return query_with_body(conn, "item.each-op", ursatz_gui_failed, false, id);
}

/* POST /api/materials/recognize {source: {item, version}, target: {item, version}}: how the target derives from the source. */
static int motifs_recognize(struct mg_connection *conn) {
  return query_with_body(conn, "composer.recognize", ursatz_gui_failed, true, NULL);
}

/* GET /api/materials/items/<id>/variant-tree: the item and, recursively, every motif derived from it. */
static int motifs_variant_tree(struct mg_connection *conn, const char *id) {
  return QUERY(conn, "item.variant-tree", ursatz_gui_failed, false, "id", id);
}

/* GET /api/materials/items/<id>/op-checks: the marks of a motif from Try every operation, {"checks": {...}} (R-190). */
static int motifs_op_checks_get(struct mg_connection *conn, const char *id) {
  return QUERY(conn, "item.op-checks", ursatz_gui_failed, false, "id", id);
}

/* POST /api/materials/motifs/random {material, meter, key?, form?, style_id?, count?, seed?}: random motives from the Composer; nothing
   is saved. The answer is the Composer's document plus "ok". */
static int motifs_random_motifs(struct mg_connection *conn) {
  return query_with_body(conn, "composer.random-motifs", ursatz_gui_failed, true, NULL);
}

/* POST /api/materials/items/validate {type: "motif"|"theme"|"piece", body}: the Composer reads the request the body
   would compose (or analyze) with and composes nothing (R-276). Answers {"ok":true,"complete":true,"request"} with the
   Composer's defaults filled, {"ok":true,"complete":false,"message"} when the body is not composable yet, or the
   refusal (400) with the request member at fault as "path". */
static int motifs_validate(struct mg_connection *conn) {
  return query_with_body(conn, "item.validate", ursatz_gui_failed, false, NULL);
}

#define ARG(args, i) ((args)->values[i])

/* "validate" is a word, not an item id: only a POST reads it. */
static int route_validate_only_post(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) {
  (void)args;
  return ursatz_gui_send_failure(conn, 405, "method not allowed");
}

static int route_import(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { (void)args; return motifs_import_motif(conn); }
static int route_render(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { (void)args; return motifs_render(conn); }
static int route_operations(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { (void)args; return motifs_variant_operations(conn); }
static int route_recognize(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { (void)args; return motifs_recognize(conn); }
static int route_random(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { (void)args; return motifs_random_motifs(conn); }
static int route_validate(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { (void)args; return motifs_validate(conn); }
static int route_variants(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return motifs_variants(conn, ARG(args, 0)); }
static int route_each_op(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return motifs_each_op(conn, ARG(args, 0)); }
static int route_tree(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return motifs_variant_tree(conn, ARG(args, 0)); }
static int route_checks_get(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return motifs_op_checks_get(conn, ARG(args, 0)); }

static const UrsatzGuiRoute kRoutes[] = {
    {"GET", "/api/materials/variant-operations", route_operations},
    {"POST", "/api/materials/recognize", route_recognize},
    {"POST", "/api/materials/render", route_render},
    {"POST", "/api/materials/motifs/random", route_random},
    {"POST", "/api/materials/motifs/import", route_import},
    {"POST", "/api/materials/items/validate", route_validate},
    {"GET", "/api/materials/items/validate", route_validate_only_post},
    {"POST", "/api/materials/items/<id>/variants", route_variants},
    {"POST", "/api/materials/items/<id>/each-op", route_each_op},
    {"GET", "/api/materials/items/<id>/variant-tree", route_tree},
    {"GET", "/api/materials/items/<id>/op-checks", route_checks_get},
};

const UrsatzGuiRoute *ursatz_gui_motifs_routes(size_t *count) {
  *count = sizeof(kRoutes) / sizeof(kRoutes[0]);
  return kRoutes;
}
