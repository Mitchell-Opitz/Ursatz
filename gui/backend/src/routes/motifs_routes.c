#include "motifs_routes.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "materials_util.h"

/* Reads the raw request body (a file) of up to kMaxRequestBodySize bytes. NULL after sending the failure. */
static unsigned char *read_file_body(struct mg_connection *conn, size_t *size, int *status) {
  size_t capacity = 4096;
  unsigned char *buffer = malloc(capacity);
  size_t total = 0;
  if (!buffer) {
    *status = send_failure(conn, 500, "out of memory");
    return NULL;
  }
  for (;;) {
    if (total == capacity) {
      unsigned char *grown = realloc(buffer, capacity * 2);
      if (!grown) {
        free(buffer);
        *status = send_failure(conn, 500, "out of memory");
        return NULL;
      }
      buffer = grown;
      capacity *= 2;
    }
    int n = mg_read(conn, buffer + total, capacity - total);
    if (n < 0) {
      free(buffer);
      *status = send_failure(conn, 400, "cannot read the request body");
      return NULL;
    }
    if (n == 0) {
      break;
    }
    total += (size_t)n;
    if (total > kMaxRequestBodySize) {
      free(buffer);
      *status = send_failure(conn, 413, "request body too large");
      return NULL;
    }
  }
  if (total == 0) {
    free(buffer);
    *status = send_failure(conn, 400, "a MIDI file is required");
    return NULL;
  }
  *size = total;
  *status = 200;
  return buffer;
}

/* POST /api/materials/motifs/import: the body is a MIDI file; ?meter= and ?key= are optional. Answers {"ok":true,"notes_text":...,
   "meter":"4/4","key":"C minor","material":...,"moved_from":"D minor"|null,"pickup":"0","analysis":{...}} and saves nothing. */
static int motifs_import_motif(struct mg_connection *conn) {
  int status = 0;
  size_t size = 0;
  unsigned char *bytes = read_file_body(conn, &size, &status);
  if (!bytes) {
    return status;
  }
  WorkshopBytes file = {bytes, size, NULL};
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

/* POST /api/materials/items/<id>/reroll: reruns the stored chain on the source's stored version with a new seed and stores the
   result as a new version. Runs on the job queue. */
static int motifs_reroll(struct mg_connection *conn, const char *id) {
  return command_with_optional_body(conn, "item.reroll", id, ursatz_gui_failed);
}

/* POST .../items/<id>/{variants/save, flag, promote, link-form}: a body in, the item out. */

static int motifs_item_command(struct mg_connection *conn, const char *id, const char *command) {
  return command_with_body(conn, command, id, ursatz_gui_failed);
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

/* The failures of the op-checks write: the store's message with the status of the table, the path of a refusal counted (422). */
static int failed_op_checks(struct mg_connection *conn, UrsatzStatus status, const UrsatzError *error) {
  return send_failure(conn, ursatz_gui_http_status(status, error), error->message);
}

/* PUT /api/materials/items/<id>/op-checks {checks}: replaces the marks. The file is not a version. */
static int motifs_op_checks_put(struct mg_connection *conn, const char *id) {
  return command_with_body(conn, "item.set-op-checks", id, failed_op_checks);
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
static int route_reroll(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return motifs_reroll(conn, ARG(args, 0)); }
static int route_tree(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return motifs_variant_tree(conn, ARG(args, 0)); }
static int route_checks_get(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return motifs_op_checks_get(conn, ARG(args, 0)); }
static int route_checks_put(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return motifs_op_checks_put(conn, ARG(args, 0)); }
static int route_save_variant(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return motifs_item_command(conn, ARG(args, 0), "item.save-variant"); }
static int route_link_form(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return motifs_item_command(conn, ARG(args, 0), "item.link-form"); }
static int route_promote(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return motifs_item_command(conn, ARG(args, 0), "item.promote"); }
static int route_flag(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { return motifs_item_command(conn, ARG(args, 0), "item.flag"); }

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
    {"POST", "/api/materials/items/<id>/reroll", route_reroll},
    {"GET", "/api/materials/items/<id>/variant-tree", route_tree},
    {"GET", "/api/materials/items/<id>/op-checks", route_checks_get},
    {"PUT", "/api/materials/items/<id>/op-checks", route_checks_put},
    {"POST", "/api/materials/items/<id>/variants/save", route_save_variant},
    {"POST", "/api/materials/items/<id>/link-form", route_link_form},
    {"POST", "/api/materials/items/<id>/promote", route_promote},
    {"POST", "/api/materials/items/<id>/flag", route_flag},
};

const UrsatzGuiRoute *ursatz_gui_motifs_routes(size_t *count) {
  *count = sizeof(kRoutes) / sizeof(kRoutes[0]);
  return kRoutes;
}
