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














static int handle_update_use(struct mg_connection *conn, const char *id, const char *source_id) {
  cJSON *item = NULL;
  UrsatzStatus result = workshop_material_update_use(id, source_id, &item);
  if (result != URSATZ_OK) {
    return send_material_failure(conn, result);
  }
  return send_ok_with(conn, "item", item);
}


/* ---- validate ----------------------------------------------------------- */




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
    return post ? ursatz_gui_motifs_item_command(conn, segments[1], workshop_variants_save) : send_method_not_allowed(conn);
  }
  if (count == 3 && strcmp(segments[2], "variant-tree") == 0) {
    return get ? ursatz_gui_motifs_variant_tree(conn, segments[1]) : send_method_not_allowed(conn);
  }
  if (count == 3 && strcmp(segments[2], "reroll") == 0) {
    return post ? ursatz_gui_motifs_reroll(conn, segments[1]) : send_method_not_allowed(conn);
  }
  if (count == 3 && strcmp(segments[2], "link-form") == 0) {
    return post ? ursatz_gui_motifs_item_command(conn, segments[1], workshop_variants_link_form) : send_method_not_allowed(conn);
  }
  if (count == 3 && strcmp(segments[2], "promote") == 0) {
    return post ? ursatz_gui_motifs_item_command(conn, segments[1], workshop_variants_promote) : send_method_not_allowed(conn);
  }
  if (count == 3 && strcmp(segments[2], "flag") == 0) {
    return post ? ursatz_gui_motifs_item_command(conn, segments[1], workshop_variants_flag) : send_method_not_allowed(conn);
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
