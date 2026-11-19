#include "motifs.h"

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
int ursatz_gui_motifs_import_motif(struct mg_connection *conn) {
  int status = 0;
  size_t size = 0;
  unsigned char *bytes = read_file_body(conn, &size, &status);
  if (!bytes) {
    return status;
  }
  WorkshopBytes file = {bytes, size, NULL};
  status = ursatz_gui_answer_query(conn, "composer.import-motif", ursatz_gui_request_params(conn, NULL), &file, failed_material_error, true);
  free(bytes);
  return status;
}

/* POST /api/materials/render {form, degrees, meter, key?, pickup?, render_key?, tempo?, role?}: nothing is saved. */
int ursatz_gui_motifs_render(struct mg_connection *conn) {
  return query_with_body(conn, "composer.render", failed_material_error, true, NULL);
}

/* GET /api/materials/variant-operations: the Composer's operation list, as the Composer API gives it. */
int ursatz_gui_motifs_variant_operations(struct mg_connection *conn) {
  return QUERY(conn, "composer.variant-operations", failed_material_error, false, NULL);
}


/* POST /api/materials/items/<id>/variants {chain?, count?, seed?, functions?, distance?} and .../each-op {seed?}: candidates from the
   item's current degrees; nothing is saved. The answer is the job. */
int ursatz_gui_motifs_variants(struct mg_connection *conn, const char *id) {
  return query_with_body(conn, "item.variants", failed_material_error, false, id);
}

int ursatz_gui_motifs_each_op(struct mg_connection *conn, const char *id) {
  return query_with_body(conn, "item.each-op", failed_material_error, false, id);
}

/* POST /api/materials/items/<id>/reroll: reruns the stored chain on the source's stored version with a new seed and stores the
   result as a new version. Runs on the job queue. */
int ursatz_gui_motifs_reroll(struct mg_connection *conn, const char *id) {
  return ursatz_gui_answer_command(conn, "item.reroll", id, NULL, NULL, failed_material_error, false);
}

/* POST .../items/<id>/{variants/save, flag, promote, link-form}: a body in, the item out. */

int ursatz_gui_motifs_item_command(struct mg_connection *conn, const char *id, const char *command) {
  return command_with_body(conn, command, id, failed_material_error);
}

/* POST /api/materials/recognize {source: {item, version}, target: {item, version}}: how the target derives from the source. */
int ursatz_gui_motifs_recognize(struct mg_connection *conn) {
  return query_with_body(conn, "composer.recognize", failed_material_error, true, NULL);
}

/* GET /api/materials/items/<id>/variant-tree: the item and, recursively, every motif derived from it. */
int ursatz_gui_motifs_variant_tree(struct mg_connection *conn, const char *id) {
  return QUERY(conn, "item.variant-tree", failed_material_error, false, "id", id);
}

/* GET /api/materials/items/<id>/op-checks: the marks of a motif from Try every operation, {"checks": {...}} (R-190). */
int ursatz_gui_motifs_op_checks_get(struct mg_connection *conn, const char *id) {
  return QUERY(conn, "item.op-checks", failed_material, false, "id", id);
}

/* The failures of the op-checks write: the store's message with the status of the table, the path of a refusal counted (422). */
static int failed_op_checks(struct mg_connection *conn, UrsatzStatus status, const UrsatzError *error) {
  return send_failure(conn, workshop_http_status(status, error), error->message);
}

/* PUT /api/materials/items/<id>/op-checks {checks}: replaces the marks. The file is not a version. */
int ursatz_gui_motifs_op_checks_put(struct mg_connection *conn, const char *id) {
  return command_with_body(conn, "item.set-op-checks", id, failed_op_checks);
}

/* POST /api/materials/motifs/random {material, meter, key?, form?, style_id?, count?, seed?}: random motives from the Composer; nothing
   is saved. The answer is the Composer's document plus "ok". */
int ursatz_gui_motifs_random_motifs(struct mg_connection *conn) {
  return query_with_body(conn, "composer.random-motifs", failed_material_error, true, NULL);
}

/* POST /api/materials/items/validate {type: "motif"|"theme"|"piece", body}: the Composer reads the request the body
   would compose (or analyze) with and composes nothing (R-276). Answers {"ok":true,"complete":true,"request"} with the
   Composer's defaults filled, {"ok":true,"complete":false,"message"} when the body is not composable yet, or the
   refusal (400) with the request member at fault as "path". */
int ursatz_gui_motifs_validate(struct mg_connection *conn) {
  return query_with_body(conn, "item.validate", failed_material_error, false, NULL);
}
