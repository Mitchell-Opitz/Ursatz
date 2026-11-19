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

#include "route_util.h"
#include "jobs.h"

static const char *kMaterialsPrefix = "/api/materials";
static const char *kSubjectsPrefix = "/api/subjects";
static const size_t kMaxRequestBodySize = 256 * 1024;
static const int kMaxSegments = 8;

/* Sends {"ok":false,"error":...} with the given status. The message goes through cJSON, so it is escaped. */
/* The failure answer {"ok":false,"error":...}; a non-empty path (the request member the Composer refused, as a JSON
   Pointer) is added as "path". */
static int send_failure_at(struct mg_connection *conn, int status, const char *message, const char *path) {
  cJSON *object = cJSON_CreateObject();
  char *text = NULL;
  if (object && cJSON_AddBoolToObject(object, "ok", 0) && cJSON_AddStringToObject(object, "error", message) &&
      (!path || path[0] == '\0' || cJSON_AddStringToObject(object, "path", path))) {
    text = cJSON_PrintUnformatted(object);
  }
  cJSON_Delete(object);
  if (!text) {
    return ursatz_gui_send_json_status(conn, 500, ursatz_gui_reason_phrase(500), "{\"ok\":false,\"error\":\"out of memory\"}");
  }
  ursatz_gui_send_json_status(conn, status, ursatz_gui_reason_phrase(status), text);
  free(text);
  return status;
}

static int send_failure(struct mg_connection *conn, int status, const char *message) {
  return send_failure_at(conn, status, message, NULL);
}

/* {"ok":true,<key>:value}; value is moved into the answer (and freed on failure). */
static int send_ok_with(struct mg_connection *conn, const char *key, cJSON *value) {
  cJSON *object = cJSON_CreateObject();
  if (!object || !value || !cJSON_AddBoolToObject(object, "ok", 1)) {
    cJSON_Delete(value);
    cJSON_Delete(object);
    return send_failure(conn, 500, "out of memory");
  }
  if (!cJSON_AddItemToObject(object, key, value)) {
    cJSON_Delete(value);
    cJSON_Delete(object);
    return send_failure(conn, 500, "out of memory");
  }
  return ursatz_gui_send_object(conn, 200, object);
}

static int send_ok(struct mg_connection *conn) {
  ursatz_gui_send_json_status(conn, 200, ursatz_gui_reason_phrase(200), "{\"ok\":true}");
  return 200;
}

static int send_method_not_allowed(struct mg_connection *conn) {
  return send_failure(conn, 405, "method not allowed");
}

static int send_not_found(struct mg_connection *conn) {
  return send_failure(conn, 404, "not found");
}

/* Reads the request body as a JSON object. Returns NULL after sending the failure (400, or 413 past
 * kMaxRequestBodySize); *status is then what the caller returns. */
static cJSON *read_json_body(struct mg_connection *conn, int *status) {
  return ursatz_gui_read_json_body_max(conn, kMaxRequestBodySize, status);
}

/* The string member, NULL when absent or null; *bad is set when it is present but not a string. */
static const char *optional_string(const cJSON *body, const char *key, bool *bad) {
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(body, key);
  if (!item || cJSON_IsNull(item)) {
    return NULL;
  }
  if (!cJSON_IsString(item)) {
    *bad = true;
    return NULL;
  }
  return item->valuestring;
}

static int send_material_failure(struct mg_connection *conn, UrsatzStatus result) {
  return ursatz_gui_send_service_failure(conn, result, "material store failure");
}

/* --- projects ------------------------------------------------------------ */

static int handle_list_projects(struct mg_connection *conn) {
  cJSON *projects = NULL;
  UrsatzStatus result = workshop_material_project_list(&projects);
  if (result != URSATZ_OK) {
    return send_material_failure(conn, result);
  }
  return send_ok_with(conn, "projects", projects);
}

static int handle_create_project(struct mg_connection *conn) {
  int status = 0;
  cJSON *body = read_json_body(conn, &status);
  if (!body) {
    return status;
  }
  bool bad = false;
  const char *name = optional_string(body, "name", &bad);
  const char *description = optional_string(body, "description", &bad);
  if (bad || !name) {
    cJSON_Delete(body);
    return send_failure(conn, 400, "name is required; name and description are strings");
  }
  cJSON *project = NULL;
  UrsatzStatus result = workshop_material_project_create(name, description, &project);
  cJSON_Delete(body);
  if (result != URSATZ_OK) {
    return send_material_failure(conn, result);
  }
  return send_ok_with(conn, "project", project);
}

static int handle_get_project(struct mg_connection *conn, const char *id) {
  cJSON *project = NULL;
  UrsatzStatus result = workshop_material_project_get(id, &project);
  if (result != URSATZ_OK) {
    return send_material_failure(conn, result);
  }
  return send_ok_with(conn, "project", project);
}

static int handle_patch_project(struct mg_connection *conn, const char *id) {
  int status = 0;
  cJSON *body = read_json_body(conn, &status);
  if (!body) {
    return status;
  }
  bool bad = false;
  const char *name = optional_string(body, "name", &bad);
  const char *description = optional_string(body, "description", &bad);
  if (bad || (!name && !description)) {
    cJSON_Delete(body);
    return send_failure(conn, 400, "give name and/or description as strings");
  }
  cJSON *project = NULL;
  UrsatzStatus result = workshop_material_project_update(id, name, description, &project);
  cJSON_Delete(body);
  if (result != URSATZ_OK) {
    return send_material_failure(conn, result);
  }
  return send_ok_with(conn, "project", project);
}

static int handle_delete_project(struct mg_connection *conn, const char *id) {
  UrsatzStatus result = workshop_material_project_delete(id);
  if (result != URSATZ_OK) {
    return send_material_failure(conn, result);
  }
  return send_ok(conn);
}

/* --- items and versions -------------------------------------------------- */

/* The value of a query parameter into out; false when absent or too long. */
static bool query_value(const struct mg_connection *conn, const char *name, char *out, size_t out_size) {
  const struct mg_request_info *info = mg_get_request_info(conn);
  if (!info->query_string) {
    return false;
  }
  return mg_get_var(info->query_string, strlen(info->query_string), name, out, out_size) > 0;
}

static int handle_list_items(struct mg_connection *conn) {
  char project[256];
  char type[32];
  cJSON *items = NULL;
  UrsatzStatus result = workshop_material_item_list(
      query_value(conn, "project", project, sizeof(project)) ? project : NULL,
      query_value(conn, "type", type, sizeof(type)) ? type : NULL, &items);
  if (result != URSATZ_OK) {
    return send_material_failure(conn, result);
  }
  return send_ok_with(conn, "items", items);
}

/* Adds "analysis_error" to the item answer when the Composer was unavailable. */
static int send_item_answer(struct mg_connection *conn, cJSON *item, const char *analysis_error) {
  if (item && analysis_error[0]) {
    cJSON_AddStringToObject(item, "analysis_error", analysis_error);
  }
  return send_ok_with(conn, "item", item);
}

/* ---- motif import from a MIDI file ------------------------------------ */

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
static int handle_import_motif(struct mg_connection *conn) {
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

/* The text of a refusal to build a playback; every one is a 500 (the file came from the Composer, not the request). */
static const char *playback_failure_text(UrsatzStatus result) {
  return result == URSATZ_ERR_ALLOC ? "out of memory" : "could not build the playback from the MIDI file";
}

/* Sends the playback JSON text the Analyzer built, and frees it. */
static int send_playback_text(struct mg_connection *conn, char *json) {
  cJSON *playback = cJSON_Parse(json);
  free(json);
  if (!playback) {
    return send_failure(conn, 500, "could not build the playback from the MIDI file");
  }
  return ursatz_gui_send_object(conn, 200, playback);
}

/* POST /api/materials/render {form, degrees, meter, key?, pickup?, render_key?, tempo?, role?}: nothing is saved. */
static int handle_render(struct mg_connection *conn) {
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

static int handle_create_item(struct mg_connection *conn) {
  int status = 0;
  cJSON *body = read_json_body(conn, &status);
  if (!body) {
    return status;
  }
  bool bad = false;
  const char *type = optional_string(body, "type", &bad);
  const char *name = optional_string(body, "name", &bad);
  const char *project_id = optional_string(body, "project_id", &bad);
  const char *note = optional_string(body, "note", &bad);
  const cJSON *version_body = cJSON_GetObjectItemCaseSensitive(body, "body");
  if (bad || !type || !name || !project_id || !cJSON_IsObject(version_body)) {
    cJSON_Delete(body);
    return send_failure(conn, 400, "type, name, project_id and an object body are required; note is a string");
  }
  if (!workshop_motif_tonic_ok(version_body)) {
    cJSON_Delete(body);
    return send_failure(conn, 400, "motifs are written with C as the tonic");
  }
  char analysis_error[WORKSHOP_MOTIF_ANALYSIS_ERROR_SIZE];
  char refusal[512] = "";
  char refusal_path[128] = "";
  WorkshopMotifAnalysis analysis = workshop_motif_analyze_body((cJSON *)version_body, NULL, analysis_error, refusal, sizeof(refusal),
                                                refusal_path, sizeof(refusal_path));
  if (analysis != WORKSHOP_MOTIF_ANALYSIS_DONE) {
    cJSON_Delete(body);
    return analysis == WORKSHOP_MOTIF_ANALYSIS_REFUSED ? send_failure_at(conn, 400, refusal, refusal_path)
                                        : send_failure(conn, 500, "out of memory");
  }
  cJSON *item = NULL;
  /* A new motif's or theme's first version is noted "Created" unless the caller gives a note. */
  const char *first_note = workshop_motif_first_note(type, note);
  UrsatzStatus result =
      workshop_material_item_create_noted(type, name, project_id, version_body, first_note, &item);
  cJSON_Delete(body);
  if (result != URSATZ_OK) {
    return send_material_failure(conn, result);
  }
  return send_item_answer(conn, item, analysis_error);
}

/* The item answer: {"ok":true,"item":meta,"body":current,"uses_status":[...],"used_by":[...],"subjects":[...]}. */
static int handle_get_item(struct mg_connection *conn, const char *id) {
  cJSON *meta = NULL;
  cJSON *body = NULL;
  cJSON *uses_status = NULL;
  cJSON *used_by = NULL;
  cJSON *subjects = NULL;
  UrsatzStatus result = workshop_material_item_get(id, &meta, &body);
  if (result == URSATZ_OK) {
    result = workshop_material_uses_status(id, &uses_status);
  }
  if (result == URSATZ_OK) {
    result = workshop_material_used_by(id, &used_by);
  }
  if (result == URSATZ_OK) {
    subjects = workshop_subjects_for_item(id);
  }
  cJSON *object = result == URSATZ_OK && subjects ? cJSON_CreateObject() : NULL;
  if (!object || !cJSON_AddBoolToObject(object, "ok", 1) || !cJSON_AddItemToObject(object, "item", meta)) {
    cJSON_Delete(object);
    cJSON_Delete(meta);
    cJSON_Delete(body);
    cJSON_Delete(uses_status);
    cJSON_Delete(used_by);
    cJSON_Delete(subjects);
    return result == URSATZ_OK ? send_failure(conn, 500, "out of memory")
                                            : send_material_failure(conn, result);
  }
  cJSON_AddItemToObject(object, "body", body);
  cJSON_AddItemToObject(object, "uses_status", uses_status);
  cJSON_AddItemToObject(object, "used_by", used_by);
  cJSON_AddItemToObject(object, "subjects", subjects);
  return ursatz_gui_send_object(conn, 200, object);
}

static int handle_patch_item(struct mg_connection *conn, const char *id) {
  int status = 0;
  cJSON *body = read_json_body(conn, &status);
  if (!body) {
    return status;
  }
  bool bad = false;
  const char *name = optional_string(body, "name", &bad);
  const char *project_id = optional_string(body, "project_id", &bad);
  if (bad || (!name && !project_id)) {
    cJSON_Delete(body);
    return send_failure(conn, 400, "give name and/or project_id as strings");
  }
  /* The project is checked up front, so a bad second field cannot leave the rename applied. */
  cJSON *item = NULL;
  UrsatzStatus result = URSATZ_OK;
  if (project_id) {
    cJSON *project = NULL;
    result = workshop_material_project_get(project_id, &project);
    cJSON_Delete(project);
  }
  if (result == URSATZ_OK && name) {
    result = workshop_material_item_rename(id, name, &item);
  }
  if (result == URSATZ_OK && project_id) {
    cJSON_Delete(item);
    item = NULL;
    result = workshop_material_item_move(id, project_id, &item);
  }
  cJSON_Delete(body);
  if (result != URSATZ_OK) {
    cJSON_Delete(item);
    return send_material_failure(conn, result);
  }
  return send_ok_with(conn, "item", item);
}

static int handle_delete_item(struct mg_connection *conn, const char *id) {
  char force[8];
  bool forced = query_value(conn, "force", force, sizeof(force)) && strcmp(force, "1") == 0;
  UrsatzStatus result = workshop_material_item_delete(id, forced);
  if (result == URSATZ_ERR_CONFLICT) {
    cJSON *used_by = NULL;
    cJSON *object = cJSON_CreateObject();
    if (workshop_material_delete_blockers(id, &used_by) != URSATZ_OK || !object ||
        !cJSON_AddBoolToObject(object, "ok", 0) || !cJSON_AddStringToObject(object, "error", ursatz_gui_failure_message("material store failure"))) {
      cJSON_Delete(used_by);
      cJSON_Delete(object);
      return send_failure(conn, 500, "out of memory");
    }
    cJSON_AddItemToObject(object, "used_by", used_by);
    char *text = cJSON_PrintUnformatted(object);
    cJSON_Delete(object);
    if (!text) {
      return send_failure(conn, 500, "out of memory");
    }
    ursatz_gui_send_json_status(conn, 409, ursatz_gui_reason_phrase(409), text);
    free(text);
    return 409;
  }
  if (result != URSATZ_OK) {
    return send_material_failure(conn, result);
  }
  return send_ok(conn);
}

static int handle_add_version(struct mg_connection *conn, const char *id) {
  int status = 0;
  cJSON *body = read_json_body(conn, &status);
  if (!body) {
    return status;
  }
  bool bad = false;
  const char *note = optional_string(body, "note", &bad);
  const cJSON *version_body = cJSON_GetObjectItemCaseSensitive(body, "body");
  if (bad || !cJSON_IsObject(version_body)) {
    cJSON_Delete(body);
    return send_failure(conn, 400, "an object body is required; note is a string");
  }
  /* Every new version of a motif or a theme says what changed. */
  if (!workshop_motif_version_note_ok(version_body, note)) {
    cJSON_Delete(body);
    return send_failure(conn, 400, "give a version note");
  }
  if (!workshop_motif_tonic_ok(version_body)) {
    cJSON_Delete(body);
    return send_failure(conn, 400, "motifs are written with C as the tonic");
  }
  char analysis_error[WORKSHOP_MOTIF_ANALYSIS_ERROR_SIZE];
  char refusal[512] = "";
  char refusal_path[128] = "";
  /* The previous version tells whether proposed segments are to be proposed again (R-312). */
  cJSON *previous_meta = NULL;
  cJSON *previous_body = NULL;
  if (workshop_material_item_get(id, &previous_meta, &previous_body) != URSATZ_OK) {
    previous_body = NULL;
  }
  cJSON_Delete(previous_meta);
  WorkshopMotifAnalysis analysis = workshop_motif_analyze_body((cJSON *)version_body, previous_body, analysis_error, refusal,
                                                sizeof(refusal), refusal_path, sizeof(refusal_path));
  cJSON_Delete(previous_body);
  if (analysis != WORKSHOP_MOTIF_ANALYSIS_DONE) {
    cJSON_Delete(body);
    return analysis == WORKSHOP_MOTIF_ANALYSIS_REFUSED ? send_failure_at(conn, 400, refusal, refusal_path)
                                        : send_failure(conn, 500, "out of memory");
  }
  cJSON *item = NULL;
  UrsatzStatus result = workshop_material_item_add_version(id, version_body, note, &item);
  cJSON_Delete(body);
  if (result != URSATZ_OK) {
    return send_material_failure(conn, result);
  }
  return send_item_answer(conn, item, analysis_error);
}

/* POST /api/materials/items/<id>/reanalyze: analyzes the current version of a motif again and saves the result as a
   new version (versions are never rewritten, R-130). A refusal writes nothing. */
static int handle_reanalyze(struct mg_connection *conn, const char *id) {
  cJSON *meta = NULL;
  cJSON *version_body = NULL;
  UrsatzStatus result = workshop_material_item_get(id, &meta, &version_body);
  if (result != URSATZ_OK) {
    return send_material_failure(conn, result);
  }
  const cJSON *type = cJSON_GetObjectItemCaseSensitive(meta, "type");
  bool motif = cJSON_IsString(type) && strcmp(type->valuestring, "motif") == 0;
  cJSON_Delete(meta);
  if (!motif) {
    cJSON_Delete(version_body);
    return send_failure(conn, 400, "only motifs are analyzed");
  }
  char analysis_error[WORKSHOP_MOTIF_ANALYSIS_ERROR_SIZE];
  char refusal[512] = "";
  char refusal_path[128] = "";
  WorkshopMotifAnalysis analysis = workshop_motif_analyze_body(version_body, NULL, analysis_error, refusal, sizeof(refusal), refusal_path,
                                                sizeof(refusal_path));
  if (analysis != WORKSHOP_MOTIF_ANALYSIS_DONE) {
    cJSON_Delete(version_body);
    return analysis == WORKSHOP_MOTIF_ANALYSIS_REFUSED ? send_failure_at(conn, 400, refusal, refusal_path)
                                        : send_failure(conn, 500, "out of memory");
  }
  cJSON *item = NULL;
  result = workshop_material_item_add_version(id, version_body, "Re-analyzed", &item);
  cJSON_Delete(version_body);
  if (result != URSATZ_OK) {
    return send_material_failure(conn, result);
  }
  return send_item_answer(conn, item, analysis_error);
}

static int handle_get_version(struct mg_connection *conn, const char *id, const char *number_text) {
  char *end = NULL;
  long number = strtol(number_text, &end, 10);
  if (end == number_text || *end != '\0' || number < 1 || number > 1000000) {
    return send_failure(conn, 404, "version not found");
  }
  cJSON *version_body = NULL;
  UrsatzStatus result = workshop_material_item_get_version(id, (int)number, &version_body);
  if (result != URSATZ_OK) {
    return send_material_failure(conn, result);
  }
  cJSON *object = cJSON_CreateObject();
  if (!object || !cJSON_AddBoolToObject(object, "ok", 1) || !cJSON_AddNumberToObject(object, "number", (double)number)) {
    cJSON_Delete(object);
    cJSON_Delete(version_body);
    return send_failure(conn, 500, "out of memory");
  }
  cJSON_AddItemToObject(object, "body", version_body);
  return ursatz_gui_send_object(conn, 200, object);
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

/* The optional member seed of body: false when it is not a whole number from 0 to 4294967295. */
static bool read_seed(const cJSON *body, long long *seed, bool *has_seed) {
  const cJSON *member = cJSON_GetObjectItemCaseSensitive(body, "seed");
  *has_seed = member && !cJSON_IsNull(member);
  if (!*has_seed) {
    return true;
  }
  if (!cJSON_IsNumber(member) || member->valuedouble < 0 || member->valuedouble > 4294967295.0 ||
      member->valuedouble != (double)(long long)member->valuedouble) {
    return false;
  }
  *seed = (long long)member->valuedouble;
  return true;
}

/* POST /api/materials/items/<thm-id or pce-id>/compose {seed?, update_uses?}: composes the theme or piece into a new
   version with its files (R-167, R-174). With update_uses, every updated use moves to its source's current version first. */
static int handle_compose(struct mg_connection *conn, const char *id) {
  int status = 0;
  cJSON *body = read_json_body(conn, &status);
  if (!body) {
    return status;
  }
  const cJSON *seed = cJSON_GetObjectItemCaseSensitive(body, "seed");
  const cJSON *update_uses = cJSON_GetObjectItemCaseSensitive(body, "update_uses");
  bool has_seed = seed && !cJSON_IsNull(seed);
  if ((has_seed && (!cJSON_IsNumber(seed) || seed->valuedouble < 0 || seed->valuedouble > 4294967295.0 ||
                    seed->valuedouble != (double)(long long)seed->valuedouble)) ||
      (update_uses && !cJSON_IsBool(update_uses))) {
    cJSON_Delete(body);
    return send_failure(conn, 400, "seed is a whole number from 0 to 4294967295; update_uses is a boolean");
  }
  cJSON *meta = NULL;
  cJSON *stored = NULL;
  UrsatzStatus found = workshop_material_item_get(id, &meta, &stored);
  if (found != URSATZ_OK) {
    cJSON_Delete(body);
    return send_material_failure(conn, found);
  }
  const cJSON *type = cJSON_GetObjectItemCaseSensitive(meta, "type");
  bool piece = cJSON_IsString(type) && strcmp(type->valuestring, "piece") == 0;
  cJSON_Delete(meta);
  cJSON_Delete(stored);
  long long chosen = has_seed ? (long long)seed->valuedouble : 0;
  /* Composed on the job queue: the answer is the job (GET /api/jobs/<id>). */
  WorkshopComposeOp op = piece ? WORKSHOP_COMPOSE_PIECE : WORKSHOP_COMPOSE_THEME;
  WorkshopComposeJob *job = workshop_compose_job_new(op, id);
  if (job) {
    job->has_seed = has_seed;
    job->seed = chosen;
    job->update_uses = cJSON_IsTrue(update_uses);
  }
  cJSON_Delete(body);
  if (!job) {
    return send_failure(conn, 500, "out of memory");
  }
  return ursatz_gui_jobs_send_accepted(conn, workshop_compose_job_kind(op), workshop_compose_job_run, job,
                                       workshop_compose_job_free);
}

/* POST /api/materials/items/<pce-id>/versions/<n>/regenerate {section_id, lane?, seed?}: writes one section of a composed
   piece version again, as the next version (R-173, R-174). */
static int handle_piece_regenerate(struct mg_connection *conn, const char *id, const char *number_text) {
  int version = 0;
  int status = 0;
  if (!parse_version_number(number_text, &version)) {
    return send_failure(conn, 404, "version not found");
  }
  cJSON *body = read_json_body(conn, &status);
  if (!body) {
    return status;
  }
  const cJSON *section = cJSON_GetObjectItemCaseSensitive(body, "section_id");
  const cJSON *lane = cJSON_GetObjectItemCaseSensitive(body, "lane");
  long long seed = 0;
  bool has_seed = false;
  if (!cJSON_IsString(section) || !section->valuestring[0] || (lane && !cJSON_IsNull(lane) && !cJSON_IsString(lane)) ||
      !read_seed(body, &seed, &has_seed)) {
    cJSON_Delete(body);
    return send_failure(conn, 400, "section_id is a string; lane is a string; seed is a whole number from 0 to 4294967295");
  }
  WorkshopComposeJob *job = workshop_compose_job_new(WORKSHOP_COMPOSE_PIECE_REGENERATE, id);
  if (job) {
    job->version = version;
    job->section = strdup(section->valuestring);
    job->lane = cJSON_IsString(lane) ? strdup(lane->valuestring) : NULL;
    job->has_seed = has_seed;
    job->seed = seed;
  }
  bool built = job && job->section && (!cJSON_IsString(lane) || job->lane);
  cJSON_Delete(body);
  if (!built) {
    workshop_compose_job_free(job);
    return send_failure(conn, 500, "out of memory");
  }
  return ursatz_gui_jobs_send_accepted(conn, "regenerate", workshop_compose_job_run, job, workshop_compose_job_free);
}

/* POST /api/materials/items/<pce-id>/versions/<n>/variations {section_id, lane, count, seed?}: makes variations of a
   section of a composed piece version into a new set; the answer lists them. */
static int handle_piece_variations(struct mg_connection *conn, const char *id, const char *number_text) {
  int version = 0;
  int status = 0;
  if (!parse_version_number(number_text, &version)) {
    return send_failure(conn, 404, "version not found");
  }
  cJSON *body = read_json_body(conn, &status);
  if (!body) {
    return status;
  }
  const cJSON *section = cJSON_GetObjectItemCaseSensitive(body, "section_id");
  const cJSON *lane = cJSON_GetObjectItemCaseSensitive(body, "lane");
  const cJSON *count = cJSON_GetObjectItemCaseSensitive(body, "count");
  long long seed = 0;
  bool has_seed = false;
  if (!cJSON_IsString(section) || !section->valuestring[0] || !cJSON_IsString(lane) || !cJSON_IsNumber(count) ||
      count->valuedouble < 1 || count->valuedouble > 8 || count->valuedouble != (double)(int)count->valuedouble ||
      !read_seed(body, &seed, &has_seed)) {
    cJSON_Delete(body);
    return send_failure(conn, 400, "section_id and lane are strings; count is a whole number from 1 to 8; seed is a whole number from 0 to 4294967295");
  }
  WorkshopComposeJob *job = workshop_compose_job_new(WORKSHOP_COMPOSE_PIECE_VARIATIONS, id);
  if (job) {
    job->version = version;
    job->section = strdup(section->valuestring);
    job->lane = strdup(lane->valuestring);
    job->count = (int)count->valuedouble;
    job->has_seed = has_seed;
    job->seed = seed;
  }
  bool built = job && job->section && job->lane;
  cJSON_Delete(body);
  if (!built) {
    workshop_compose_job_free(job);
    return send_failure(conn, 500, "out of memory");
  }
  return ursatz_gui_jobs_send_accepted(conn, "variations", workshop_compose_job_run, job, workshop_compose_job_free);
}

/* GET /api/materials/items/<pce-id>/versions/<n>/variations/<set>/<file>: a file of a variations set. */
static int handle_piece_variation_file(struct mg_connection *conn, const char *id, const char *number_text, const char *set_text,
                                       const char *name) {
  int version = 0;
  int set = 0;
  unsigned char *data = NULL;
  size_t size = 0;
  const char *type = NULL;
  if (!parse_version_number(number_text, &version) || !parse_version_number(set_text, &set) ||
      workshop_piece_variation_file(id, version, set, name, &data, &size, &type) != URSATZ_OK) {
    return send_failure(conn, 404, "file not found");
  }
  mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n", type, size);
  mg_write(conn, data, size);
  free(data);
  return 200;
}

/* POST /api/materials/items/<pce-id>/versions/<n>/variations/<set>/choose {index}: makes a variation the next version. */
static int handle_piece_choose(struct mg_connection *conn, const char *id, const char *number_text, const char *set_text) {
  int version = 0;
  int set = 0;
  int status = 0;
  if (!parse_version_number(number_text, &version) || !parse_version_number(set_text, &set)) {
    return send_failure(conn, 404, "variations not found");
  }
  cJSON *body = read_json_body(conn, &status);
  if (!body) {
    return status;
  }
  const cJSON *index = cJSON_GetObjectItemCaseSensitive(body, "index");
  if (!cJSON_IsNumber(index) || index->valuedouble < 1 || index->valuedouble > 99 ||
      index->valuedouble != (double)(int)index->valuedouble) {
    cJSON_Delete(body);
    return send_failure(conn, 400, "index is a whole number from 1");
  }
  WorkshopComposeJob *job = workshop_compose_job_new(WORKSHOP_COMPOSE_PIECE_CHOOSE, id);
  if (job) {
    job->version = version;
    job->set = set;
    job->index = (int)index->valuedouble;
  }
  cJSON_Delete(body);
  if (!job) {
    return send_failure(conn, 500, "out of memory");
  }
  return ursatz_gui_jobs_send_accepted(conn, "choose", workshop_compose_job_run, job, workshop_compose_job_free);
}

/* GET /api/materials/items/<id>/versions/<n>/files/<name>: one of a theme or piece version's files. */
static int handle_version_file(struct mg_connection *conn, const char *id, const char *number_text, const char *name) {
  char *end = NULL;
  long number = strtol(number_text, &end, 10);
  const char *content_type = workshop_material_version_file_type(name);
  if (end == number_text || *end != '\0' || number < 1 || number > 1000000 || !content_type) {
    return send_failure(conn, 404, "file not found");
  }
  unsigned char *data = NULL;
  size_t size = 0;
  UrsatzStatus result = workshop_material_version_file(id, (int)number, name, &data, &size);
  if (result != URSATZ_OK) {
    return result == URSATZ_ERR_NOT_FOUND ? send_failure(conn, 404, "file not found")
                                                        : send_material_failure(conn, result);
  }
  mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n",
            content_type, size);
  mg_write(conn, data, size);
  free(data);
  return 200;
}

/* Sends a file's bytes with a content type. */
static int send_bytes(struct mg_connection *conn, const char *type, const void *data, size_t size) {
  mg_send_http_ok(conn, type, (long long)size);
  mg_write(conn, data, size);
  return 200;
}

/* GET /api/materials/items/<pce-id>/versions/<n>/variations/<set>/<file>/playback: the playback of a variation's MIDI
   file (variation-<i>.mid). */
static int handle_piece_variation_playback(struct mg_connection *conn, const char *id, const char *number_text,
                                           const char *set_text, const char *name) {
  int version = 0;
  int set = 0;
  char path[640];
  if (!parse_version_number(number_text, &version) || !parse_version_number(set_text, &set) ||
      workshop_piece_variation_midi_path(id, version, set, name, path, sizeof(path)) != URSATZ_OK) {
    return send_failure(conn, 404, "file not found");
  }
  char *json = NULL;
  UrsatzStatus built = workshop_playback_of_midi(path, &json);
  return built == URSATZ_OK ? send_playback_text(conn, json)
                                         : send_failure(conn, 500, playback_failure_text(built));
}

/* GET /api/materials/items/<id>/versions/<n>/score and .../playback, with ?key= for the key to sound it in (default: the motif's own
   key). With ?file= the playback route answers for a theme or piece version's own MIDI file instead. */
static int handle_version_render(struct mg_connection *conn, const char *id, const char *number_text, bool playback_only) {
  char *end = NULL;
  long number = strtol(number_text, &end, 10);
  if (end == number_text || *end != '\0' || number < 1 || number > 1000000) {
    return send_failure(conn, 404, "version not found");
  }
  char file[16];
  char render_key[32];
  bool has_file = playback_only && query_value(conn, "file", file, sizeof(file));
  bool has_render_key = query_value(conn, "key", render_key, sizeof(render_key));
  char *musicxml = NULL;
  cJSON *playback = NULL;
  int status = 0;
  UrsatzError error = {{0}};
  UrsatzStatus result = workshop_motif_version_render(id, (int)number, has_render_key ? render_key : NULL, playback_only,
                                                      has_file ? file : NULL, &musicxml, &playback, &status, &error);
  if (result != URSATZ_OK) {
    return send_failure_at(conn, status, error.message, error.path);
  }
  if (playback) {
    return ursatz_gui_send_object(conn, 200, playback);
  }
  int answered = send_bytes(conn, "application/xml", musicxml, strlen(musicxml));
  free(musicxml);
  return answered;
}

/* ---- variants ---------------------------------------------------------- */

/* GET /api/materials/variant-operations: the Composer's operation list, as the Composer API gives it. */
static int handle_variant_operations(struct mg_connection *conn) {
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

/* The failure answer of a workshop call: the status its error says (the Composer's refusal path travels as "path"). */
static int send_service_error(struct mg_connection *conn, UrsatzStatus status, const UrsatzError *error) {
  return send_failure_at(conn, workshop_http_status(status, NULL), error->message, error->path);
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

static int handle_variants(struct mg_connection *conn, const char *id) {
  return handle_variants_job(conn, id, WORKSHOP_VARIANTS_VARIANTS);
}

static int handle_each_op(struct mg_connection *conn, const char *id) {
  return handle_variants_job(conn, id, WORKSHOP_VARIANTS_EACH_OP);
}

/* POST /api/materials/items/<id>/reroll: reruns the stored chain on the source's stored version with a new seed and stores the
   result as a new version. Runs on the job queue. */
static int handle_reroll(struct mg_connection *conn, const char *id) {
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
typedef UrsatzStatus (*ItemCommand)(const char *id, const cJSON *body, cJSON **item, UrsatzError *error);

static int handle_item_command(struct mg_connection *conn, const char *id, ItemCommand command) {
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
static int handle_recognize(struct mg_connection *conn) {
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
static int handle_variant_tree(struct mg_connection *conn, const char *id) {
  cJSON *tree = NULL;
  UrsatzError error = {{0}};
  UrsatzStatus result = workshop_variants_tree(id, &tree, &error);
  if (result != URSATZ_OK) {
    return send_service_error(conn, result, &error);
  }
  return send_ok_with(conn, "tree", tree);
}

/* GET /api/materials/items/<id>/op-checks: the marks of a motif from Try every operation, {"checks": {...}} (R-190). */
static int handle_op_checks_get(struct mg_connection *conn, const char *id) {
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
static int handle_op_checks_put(struct mg_connection *conn, const char *id) {
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


static int handle_update_use(struct mg_connection *conn, const char *id, const char *source_id) {
  cJSON *item = NULL;
  UrsatzStatus result = workshop_material_update_use(id, source_id, &item);
  if (result != URSATZ_OK) {
    return send_material_failure(conn, result);
  }
  return send_ok_with(conn, "item", item);
}

/* POST /api/materials/motifs/random {material, meter, key?, form?, style_id?, count?, seed?}: random motives from the Composer; nothing
   is saved. The answer is the Composer's document plus "ok". */
static int handle_random_motifs(struct mg_connection *conn) {
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

/* ---- validate ----------------------------------------------------------- */


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
static int handle_validate(struct mg_connection *conn) {
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

/* GET /api/materials/items/<id>/export[?version=<n>]: the Analyzer's text export of a motif, theme or piece (text/plain).
   The current version unless version is given. */
static int handle_get_item_export(struct mg_connection *conn, const char *id) {
  char version_text[16];
  int version = 0;
  if (query_value(conn, "version", version_text, sizeof(version_text))) {
    char *end = NULL;
    long parsed = strtol(version_text, &end, 10);
    if (end == version_text || *end != '\0' || parsed < 1 || parsed > 999999) {
      return send_failure(conn, 400, "version must be a whole number from 1");
    }
    version = (int)parsed;
  }
  char *text = NULL;
  UrsatzError failure = {{0}};
  UrsatzStatus result = workshop_material_export(id, version, &text, &failure);
  if (result == URSATZ_ERR_NOT_FOUND) {
    return send_failure(conn, 404, "item or version not found");
  }
  if (result == URSATZ_ERR_REQUEST) {
    return send_failure(conn, 422, failure.message[0] ? failure.message : "composer could not render the motif");
  }
  if (result != URSATZ_OK) {
    return send_failure(conn, 500, result == URSATZ_ERR_ALLOC ? "out of memory" : "failed to export the item");
  }
  size_t length = strlen(text);
  mg_send_http_ok(conn, "text/plain; charset=utf-8", (long)length);
  mg_write(conn, text, length);
  free(text);
  return 200;
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
    return post ? handle_validate(conn) : send_method_not_allowed(conn);
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
    return post ? handle_variants(conn, segments[1]) : send_method_not_allowed(conn);
  }
  if (count == 3 && strcmp(segments[2], "op-checks") == 0) {
    if (get) {
      return handle_op_checks_get(conn, segments[1]);
    }
    return strcmp(method, "PUT") == 0 ? handle_op_checks_put(conn, segments[1]) : send_method_not_allowed(conn);
  }
  if (count == 3 && strcmp(segments[2], "each-op") == 0) {
    return post ? handle_each_op(conn, segments[1]) : send_method_not_allowed(conn);
  }
  if (count == 4 && strcmp(segments[2], "variants") == 0 && strcmp(segments[3], "save") == 0) {
    return post ? handle_item_command(conn, segments[1], workshop_variants_save) : send_method_not_allowed(conn);
  }
  if (count == 3 && strcmp(segments[2], "variant-tree") == 0) {
    return get ? handle_variant_tree(conn, segments[1]) : send_method_not_allowed(conn);
  }
  if (count == 3 && strcmp(segments[2], "reroll") == 0) {
    return post ? handle_reroll(conn, segments[1]) : send_method_not_allowed(conn);
  }
  if (count == 3 && strcmp(segments[2], "link-form") == 0) {
    return post ? handle_item_command(conn, segments[1], workshop_variants_link_form) : send_method_not_allowed(conn);
  }
  if (count == 3 && strcmp(segments[2], "promote") == 0) {
    return post ? handle_item_command(conn, segments[1], workshop_variants_promote) : send_method_not_allowed(conn);
  }
  if (count == 3 && strcmp(segments[2], "flag") == 0) {
    return post ? handle_item_command(conn, segments[1], workshop_variants_flag) : send_method_not_allowed(conn);
  }
  if (count == 5 && strcmp(segments[2], "uses") == 0 && strcmp(segments[4], "update") == 0) {
    return post ? handle_update_use(conn, segments[1], segments[3]) : send_method_not_allowed(conn);
  }
  return send_not_found(conn);
}

/*
 * Splits the part of local_uri after prefix into path segments (copied into
 * storage). Returns the segment count, or -1 when there are too many, an
 * empty segment, or a segment of 256 characters or more.
 */
static int split_path(const char *rest, char segments[][256]) {
  int count = 0;
  if (*rest == '\0') {
    return 0;
  }
  if (*rest != '/') {
    return -1;
  }
  rest++;
  while (*rest != '\0') {
    const char *slash = strchr(rest, '/');
    size_t len = slash ? (size_t)(slash - rest) : strlen(rest);
    if (len == 0 || len >= 256 || count >= kMaxSegments) {
      return -1;
    }
    memcpy(segments[count], rest, len);
    segments[count][len] = '\0';
    count++;
    rest += len;
    if (*rest == '/') {
      rest++;
      if (*rest == '\0') {
        return -1;
      }
    }
  }
  return count;
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

/* --- subjects ------------------------------------------------------------ */

static int send_subject_failure(struct mg_connection *conn, UrsatzStatus result) {
  return ursatz_gui_send_service_failure(conn, result, "subject store failure");
}


static int handle_list_subjects(struct mg_connection *conn) {
  cJSON *subjects = NULL;
  UrsatzStatus result = workshop_subject_list(&subjects);
  if (result != URSATZ_OK) {
    return send_subject_failure(conn, result);
  }
  return send_ok_with(conn, "subjects", subjects);
}

static int handle_subject_groups(struct mg_connection *conn) {
  cJSON *groups = NULL;
  UrsatzStatus result = workshop_subject_groups(&groups);
  if (result != URSATZ_OK) {
    return send_subject_failure(conn, result);
  }
  return send_ok_with(conn, "groups", groups);
}

static int handle_create_subject(struct mg_connection *conn) {
  int status = 0;
  cJSON *body = read_json_body(conn, &status);
  if (!body) {
    return status;
  }
  bool bad = false;
  const char *name = optional_string(body, "name", &bad);
  const char *kind = optional_string(body, "kind", &bad);
  const char *note = optional_string(body, "note", &bad);
  if (bad || !name || !kind) {
    cJSON_Delete(body);
    return send_failure(conn, 400, "name and kind are required; name, kind and note are strings");
  }
  cJSON *subject = NULL;
  UrsatzStatus result = workshop_subject_create(name, kind, note, &subject);
  cJSON_Delete(body);
  if (result != URSATZ_OK) {
    return send_subject_failure(conn, result);
  }
  return send_ok_with(conn, "subject", subject);
}

static int handle_get_subject(struct mg_connection *conn, const char *id) {
  cJSON *subject = NULL;
  UrsatzStatus result = workshop_subject_get(id, &subject);
  if (result != URSATZ_OK) {
    return send_subject_failure(conn, result);
  }
  return send_ok_with(conn, "subject", subject);
}

static int handle_patch_subject(struct mg_connection *conn, const char *id) {
  int status = 0;
  cJSON *body = read_json_body(conn, &status);
  if (!body) {
    return status;
  }
  bool bad = false;
  const char *name = optional_string(body, "name", &bad);
  const char *kind = optional_string(body, "kind", &bad);
  const char *note = optional_string(body, "note", &bad);
  if (bad || (!name && !kind && !note)) {
    cJSON_Delete(body);
    return send_failure(conn, 400, "give name, kind and/or note as strings");
  }
  cJSON *subject = NULL;
  UrsatzStatus result = workshop_subject_update(id, name, kind, note, &subject);
  cJSON_Delete(body);
  if (result != URSATZ_OK) {
    return send_subject_failure(conn, result);
  }
  return send_ok_with(conn, "subject", subject);
}

static int handle_delete_subject(struct mg_connection *conn, const char *id) {
  UrsatzStatus result = workshop_subject_delete(id);
  if (result != URSATZ_OK) {
    return send_subject_failure(conn, result);
  }
  return send_ok(conn);
}

/* {"ok":true,"subject":...} plus "already" for a link or relation that was in place. */
static int send_subject_after(struct mg_connection *conn, cJSON *subject, bool already, bool with_already) {
  cJSON *object = cJSON_CreateObject();
  if (!object || !subject || !cJSON_AddBoolToObject(object, "ok", 1) ||
      (with_already && !cJSON_AddBoolToObject(object, "already", already ? 1 : 0))) {
    cJSON_Delete(object);
    cJSON_Delete(subject);
    return send_failure(conn, 500, "out of memory");
  }
  cJSON_AddItemToObject(object, "subject", subject);
  return ursatz_gui_send_object(conn, 200, object);
}

/* kind is "items", "pieces" or "related"; POST adds the link, DELETE removes it. */
static int handle_subject_link(struct mg_connection *conn, const char *method, const char *id, const char *kind,
                               const char *other) {
  bool post = strcmp(method, "POST") == 0;
  if (!post && strcmp(method, "DELETE") != 0) {
    return send_method_not_allowed(conn);
  }
  bool already = false;
  cJSON *subject = NULL;
  UrsatzStatus result;
  if (post && strcmp(kind, "items") == 0) {
    /* A variant belongs to its motif and has no subjects (R-187). An absent item is left to the subject service. */
    cJSON *body = NULL;
    bool variant = workshop_material_item_get(other, NULL, &body) == URSATZ_OK &&
                   workshop_material_item_is_variant(body);
    cJSON_Delete(body);
    if (variant) {
      return send_failure(conn, 400, "variants have no subjects; save it as a new motif first");
    }
  }
  if (strcmp(kind, "items") == 0) {
    result = post ? workshop_subject_link_item(id, other, &already, &subject)
                  : workshop_subject_unlink_item(id, other, &subject);
  } else if (strcmp(kind, "pieces") == 0) {
    result = post ? workshop_subject_link_piece(id, other, &already, &subject)
                  : workshop_subject_unlink_piece(id, other, &subject);
  } else {
    result = post ? workshop_subject_relate(id, other, &already, &subject)
                  : workshop_subject_unrelate(id, other, &subject);
  }
  if (result != URSATZ_OK) {
    return send_subject_failure(conn, result);
  }
  return send_subject_after(conn, subject, already, post);
}

static int route_subjects(struct mg_connection *conn, const char *method, char segments[][256], int count) {
  bool get = strcmp(method, "GET") == 0;
  if (count == 0) {
    if (get) {
      return handle_list_subjects(conn);
    }
    return strcmp(method, "POST") == 0 ? handle_create_subject(conn) : send_method_not_allowed(conn);
  }
  if (count == 1) {
    if (strcmp(segments[0], "groups") == 0) {
      return get ? handle_subject_groups(conn) : send_method_not_allowed(conn);
    }
    if (get) {
      return handle_get_subject(conn, segments[0]);
    }
    if (strcmp(method, "PATCH") == 0) {
      return handle_patch_subject(conn, segments[0]);
    }
    return strcmp(method, "DELETE") == 0 ? handle_delete_subject(conn, segments[0]) : send_method_not_allowed(conn);
  }
  if (count == 3 && (strcmp(segments[1], "items") == 0 || strcmp(segments[1], "pieces") == 0 ||
                     strcmp(segments[1], "related") == 0)) {
    return handle_subject_link(conn, method, segments[0], segments[1], segments[2]);
  }
  return send_not_found(conn);
}

/*
 * Handler registered at "/api/subjects"; same dispatch as the materials handler.
 */
static int subjects_handler(struct mg_connection *conn, void *cbdata) {
  (void)cbdata;
  const struct mg_request_info *info = mg_get_request_info(conn);
  char segments[8][256];
  int count = split_path(info->local_uri + strlen(kSubjectsPrefix), segments);
  if (count < 0) {
    return send_not_found(conn);
  }
  return route_subjects(conn, info->request_method, segments, count);
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
    return strcmp(method, "GET") == 0 ? handle_variant_operations(conn) : send_method_not_allowed(conn);
  }
  if (count == 1 && strcmp(segments[0], "recognize") == 0) {
    return strcmp(method, "POST") == 0 ? handle_recognize(conn) : send_method_not_allowed(conn);
  }
  if (count == 1 && strcmp(segments[0], "render") == 0) {
    return strcmp(method, "POST") == 0 ? handle_render(conn) : send_method_not_allowed(conn);
  }
  if (count == 2 && strcmp(segments[0], "motifs") == 0 && strcmp(segments[1], "random") == 0) {
    return strcmp(method, "POST") == 0 ? handle_random_motifs(conn) : send_method_not_allowed(conn);
  }
  if (count == 2 && strcmp(segments[0], "motifs") == 0 && strcmp(segments[1], "import") == 0) {
    return strcmp(method, "POST") == 0 ? handle_import_motif(conn) : send_method_not_allowed(conn);
  }
  return send_not_found(conn);
}

void ursatz_gui_materials_register(struct mg_context *ctx) {
  mg_set_request_handler(ctx, kMaterialsPrefix, materials_handler, NULL);
  mg_set_request_handler(ctx, kSubjectsPrefix, subjects_handler, NULL);
}
