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
  char meter[16];
  char key[32];
  bool has_meter = query_value(conn, "meter", meter, sizeof(meter));
  bool has_key = query_value(conn, "key", key, sizeof(key));
  int status = 0;
  size_t size = 0;
  unsigned char *bytes = read_file_body(conn, &size, &status);
  if (!bytes) {
    return status;
  }
  cJSON *answer = NULL;
  UrsatzError error = {{0}};
  UrsatzStatus result = workshop_motif_import(bytes, size, has_meter ? meter : NULL, has_key ? key : NULL, &answer, &status, &error);
  free(bytes);
  if (result != URSATZ_OK) {
    return send_failure_at(conn, status, error.message, error.path);
  }
  return ursatz_gui_send_object(conn, 200, answer);
}

/* POST /api/materials/render {form, degrees, meter, key?, pickup?, render_key?, tempo?, role?}: nothing is saved. */
int ursatz_gui_motifs_render(struct mg_connection *conn) {
  int status = 0;
  cJSON *body = read_json_body(conn, &status);
  if (!body) {
    return status;
  }
  cJSON *answer = NULL;
  UrsatzError error = {{0}};
  UrsatzStatus result = workshop_motif_render(body, &answer, &status, &error);
  cJSON_Delete(body);
  if (result != URSATZ_OK) {
    return send_failure_at(conn, status, error.message, error.path);
  }
  return ursatz_gui_send_object(conn, 200, answer);
}

/* GET /api/materials/variant-operations: the Composer's operation list, as the Composer API gives it. */
int ursatz_gui_motifs_variant_operations(struct mg_connection *conn) {
  WorkshopComposerReply reply;
  workshop_motif_variant_operations(&reply);
  cJSON *document = reply.status == URSATZ_OK && reply.json ? cJSON_Parse(reply.json) : NULL;
  if (!document) {
    int failed = send_failure_at(conn, 500, reply.message[0] ? reply.message : "composer could not list the operations",
                                 reply.path);
    workshop_composer_reply_destroy(&reply);
    return failed;
  }
  workshop_composer_reply_destroy(&reply);
  if (!cJSON_AddBoolToObject(document, "ok", 1)) {
    cJSON_Delete(document);
    return send_failure(conn, 500, "out of memory");
  }
  return ursatz_gui_send_object(conn, 200, document);
}


/* POST /api/materials/items/<id>/variants {chain?, count?, seed?, functions?, distance?} and .../each-op {seed?}: candidates from the
   item's current degrees; nothing is saved. The answer is the Composer's document plus "ok" and "seed". */
static int handle_variants_job(struct mg_connection *conn, const char *id, WorkshopVariantsOp op) {
  int status = 0;
  cJSON *body = read_json_body(conn, &status);
  if (!body) {
    return status;
  }
  WorkshopVariantsJob *job = NULL;
  UrsatzError error = {{0}};
  UrsatzStatus result = workshop_variants_job_new(op, id, body, &job, &error);
  if (result != URSATZ_OK) {
    return send_service_error(conn, result, &error);
  }
  return ursatz_gui_jobs_send_accepted(conn, workshop_variants_job_kind(job), workshop_variants_job_run, job,
                                       workshop_variants_job_free);
}

int ursatz_gui_motifs_variants(struct mg_connection *conn, const char *id) {
  return handle_variants_job(conn, id, WORKSHOP_VARIANTS_VARIANTS);
}

int ursatz_gui_motifs_each_op(struct mg_connection *conn, const char *id) {
  return handle_variants_job(conn, id, WORKSHOP_VARIANTS_EACH_OP);
}

/* POST /api/materials/items/<id>/reroll: reruns the stored chain on the source's stored version with a new seed and stores the
   result as a new version. Runs on the job queue. */
int ursatz_gui_motifs_reroll(struct mg_connection *conn, const char *id) {
  WorkshopVariantsJob *job = NULL;
  UrsatzError error = {{0}};
  UrsatzStatus result = workshop_variants_job_new(WORKSHOP_VARIANTS_REROLL, id, NULL, &job, &error);
  if (result != URSATZ_OK) {
    return send_service_error(conn, result, &error);
  }
  return ursatz_gui_jobs_send_accepted(conn, workshop_variants_job_kind(job), workshop_variants_job_run, job,
                                       workshop_variants_job_free);
}

/* POST .../items/<id>/{variants/save, flag, promote, link-form}: a body in, the item out. */

int ursatz_gui_motifs_item_command(struct mg_connection *conn, const char *id, ItemCommand command) {
  int status = 0;
  cJSON *body = read_json_body(conn, &status);
  if (!body) {
    return status;
  }
  cJSON *item = NULL;
  UrsatzError error = {{0}};
  UrsatzStatus result = command(id, body, &item, &error);
  cJSON_Delete(body);
  if (result != URSATZ_OK) {
    return send_service_error(conn, result, &error);
  }
  return send_ok_with(conn, "item", item);
}

/* POST /api/materials/recognize {source: {item, version}, target: {item, version}}: how the target derives from the source. */
int ursatz_gui_motifs_recognize(struct mg_connection *conn) {
  int status = 0;
  cJSON *request = read_json_body(conn, &status);
  if (!request) {
    return status;
  }
  cJSON *document = NULL;
  UrsatzError error = {{0}};
  UrsatzStatus result = workshop_variants_recognize(request, &document, &status, &error);
  cJSON_Delete(request);
  if (result != URSATZ_OK) {
    return send_failure_at(conn, status, error.message, error.path);
  }
  return ursatz_gui_send_object(conn, 200, document);
}

/* GET /api/materials/items/<id>/variant-tree: the item and, recursively, every motif derived from it. */
int ursatz_gui_motifs_variant_tree(struct mg_connection *conn, const char *id) {
  cJSON *tree = NULL;
  UrsatzError error = {{0}};
  UrsatzStatus result = workshop_variants_tree(id, &tree, &error);
  if (result != URSATZ_OK) {
    return send_service_error(conn, result, &error);
  }
  return send_ok_with(conn, "tree", tree);
}

/* GET /api/materials/items/<id>/op-checks: the marks of a motif from Try every operation, {"checks": {...}} (R-190). */
int ursatz_gui_motifs_op_checks_get(struct mg_connection *conn, const char *id) {
  cJSON *checks = NULL;
  UrsatzStatus result = workshop_material_op_checks_get(id, &checks);
  if (result != URSATZ_OK) {
    return send_material_failure(conn, result);
  }
  if (!cJSON_AddBoolToObject(checks, "ok", 1)) {
    cJSON_Delete(checks);
    return send_failure(conn, 500, "out of memory");
  }
  return ursatz_gui_send_object(conn, 200, checks);
}

/* PUT /api/materials/items/<id>/op-checks {checks}: replaces the marks. The file is not a version. */
int ursatz_gui_motifs_op_checks_put(struct mg_connection *conn, const char *id) {
  int status = 0;
  cJSON *body = read_json_body(conn, &status);
  if (!body) {
    return status;
  }
  UrsatzError failure = {{0}};
  cJSON *stored = NULL;
  UrsatzStatus result = workshop_material_op_checks_put(id, body, &failure, &stored);
  cJSON_Delete(body);
  if (result != URSATZ_OK) {
    return failure.message[0] != '\0' ? send_failure(conn, workshop_http_status(result, &failure), failure.message) : send_material_failure(conn, result);
  }
  if (!cJSON_AddBoolToObject(stored, "ok", 1)) {
    cJSON_Delete(stored);
    return send_failure(conn, 500, "out of memory");
  }
  return ursatz_gui_send_object(conn, 200, stored);
}

/* POST /api/materials/motifs/random {material, meter, key?, form?, style_id?, count?, seed?}: random motives from the Composer; nothing
   is saved. The answer is the Composer's document plus "ok". */
int ursatz_gui_motifs_random_motifs(struct mg_connection *conn) {
  int status = 0;
  cJSON *body = read_json_body(conn, &status);
  if (!body) {
    return status;
  }
  cJSON *document = NULL;
  UrsatzError error = {{0}};
  UrsatzStatus result = workshop_motif_random(body, &document, &status, &error);
  cJSON_Delete(body);
  if (result != URSATZ_OK) {
    return send_failure_at(conn, status, error.message, error.path);
  }
  return ursatz_gui_send_object(conn, 200, document);
}

/* {"ok":true,"complete":false,"message"}: the body is not composable yet, which is no refusal. */
static int send_incomplete(struct mg_connection *conn, const char *message) {
  cJSON *object = cJSON_CreateObject();
  if (!object || !cJSON_AddBoolToObject(object, "ok", 1) || !cJSON_AddBoolToObject(object, "complete", 0) ||
      !cJSON_AddStringToObject(object, "message", message ? message : "")) {
    cJSON_Delete(object);
    return send_failure(conn, 500, "out of memory");
  }
  return ursatz_gui_send_object(conn, 200, object);
}

/* POST /api/materials/items/validate {type: "motif"|"theme"|"piece", body}: the Composer reads the request the body
   would compose (or analyze) with and composes nothing (R-276). Answers {"ok":true,"complete":true,"request"} with the
   Composer's defaults filled, {"ok":true,"complete":false,"message"} when the body is not composable yet, or the
   refusal (400) with the request member at fault as "path". */
int ursatz_gui_motifs_validate(struct mg_connection *conn) {
  int status = 0;
  cJSON *input = read_json_body(conn, &status);
  if (!input) {
    return status;
  }
  const cJSON *type = cJSON_GetObjectItemCaseSensitive(input, "type");
  const cJSON *body = cJSON_GetObjectItemCaseSensitive(input, "body");
  bool is_motif = cJSON_IsString(type) && strcmp(type->valuestring, "motif") == 0;
  bool is_theme = cJSON_IsString(type) && strcmp(type->valuestring, "theme") == 0;
  bool is_piece = cJSON_IsString(type) && strcmp(type->valuestring, "piece") == 0;
  if (!(is_motif || is_theme || is_piece) || !cJSON_IsObject(body)) {
    cJSON_Delete(input);
    return send_failure(conn, 400, "type is motif, theme or piece and body an object");
  }
  cJSON *request = NULL;
  UrsatzError failure = {{0}};
  bool complete = false;
  UrsatzStatus built = URSATZ_OK;
  if (is_motif) {
    const cJSON *motif = cJSON_GetObjectItemCaseSensitive(body, "motif");
    const cJSON *form = cJSON_GetObjectItemCaseSensitive(motif, "form");
    const cJSON *notes = cJSON_GetObjectItemCaseSensitive(motif, "notes");
    bool draft = !cJSON_IsObject(motif) || !cJSON_IsString(form) || !cJSON_IsString(notes) ||
                 notes->valuestring[0] == '\0' ||
                 (strcmp(form->valuestring, "full") != 0 && strcmp(form->valuestring, "rhythm") != 0 &&
                  strcmp(form->valuestring, "contour") != 0);
    request = draft ? NULL : workshop_motif_request(motif);
    complete = request != NULL;
    if (!draft && !request) {
      workshop_error_printf(&failure, "the motif needs notes, meter and pickup");
    }
  } else {
    /* A form not saved yet names its motifs and themes by reference; the uses a save would make are made here. */
    cJSON *filled = NULL;
    UrsatzStatus uses = workshop_material_body_with_uses(body, &filled);
    if (uses == URSATZ_ERR_ALLOC) {
      built = workshop_fail(URSATZ_ERR_ALLOC, NULL);
    } else if (uses != URSATZ_OK) {
      workshop_error_printf(&failure, "a motif or theme the body names cannot be used yet");
    } else if (is_theme) {
      built = workshop_theme_validate(filled, &complete, &request, &failure);
    } else {
      built = workshop_piece_validate(filled, &complete, &request, &failure);
    }
    cJSON_Delete(filled);
  }
  if (built == URSATZ_ERR_ALLOC) {
    cJSON_Delete(request);
    cJSON_Delete(input);
    return send_failure(conn, 500, "out of memory");
  }
  if (built != URSATZ_OK) {
    cJSON_Delete(request);
    cJSON_Delete(input);
    return send_failure(conn, built == URSATZ_ERR_REQUEST ? 400 : 500, failure.message[0] ? failure.message : "the request could not be built");
  }
  if (!complete) {
    cJSON_Delete(input);
    return send_incomplete(conn, failure.message);
  }
  WorkshopComposerReply reply;
  bool called = workshop_motif_validate(is_motif ? "motive" : type->valuestring, request, &reply);
  cJSON_Delete(request);
  if (!called) {
    cJSON_Delete(input);
    return send_failure(conn, 500, "out of memory");
  }
  cJSON_Delete(input);
  cJSON *document = reply.status == URSATZ_OK && reply.json ? cJSON_Parse(reply.json) : NULL;
  cJSON *filled = document ? cJSON_DetachItemFromObjectCaseSensitive(document, "request") : NULL;
  cJSON_Delete(document);
  if (!filled) {
    int failed = send_failure_at(conn, workshop_composer_reply_failure_status(&reply),
                                 reply.message[0] ? reply.message : "composer could not check the request", reply.path);
    workshop_composer_reply_destroy(&reply);
    return failed;
  }
  workshop_composer_reply_destroy(&reply);
  cJSON *answer = cJSON_CreateObject();
  if (!answer || !cJSON_AddBoolToObject(answer, "ok", 1) || !cJSON_AddBoolToObject(answer, "complete", 1) ||
      !cJSON_AddItemToObject(answer, "request", filled)) {
    cJSON_Delete(filled);
    cJSON_Delete(answer);
    return send_failure(conn, 500, "out of memory");
  }
  return ursatz_gui_send_object(conn, 200, answer);
}
