#include "materials.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <civetweb.h>
#include <ctype.h>
#include <math.h>
#include <unistd.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#include "../util/fs_util.h"
#include "route_util.h"
#include "../service/composer_client.h"
#include "../service/compose_jobs.h"
#include "jobs.h"
#include "../service/material_service.h"
#include "../service/motif_service.h"
#include "../service/style_service.h"
#include "../service/subject_service.h"
#include "../service/piece_service.h"
#include "../service/playback_service.h"
#include "../service/theme_service.h"

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

static int material_status(UrsatzGuiMaterialResult result) {
  switch (result) {
    case URSATZ_GUI_MATERIAL_OK:
      return 200;
    case URSATZ_GUI_MATERIAL_ERR_INVALID_ID:
    case URSATZ_GUI_MATERIAL_ERR_INVALID_NAME:
    case URSATZ_GUI_MATERIAL_ERR_INVALID_TYPE:
    case URSATZ_GUI_MATERIAL_ERR_INVALID_BODY:
      return 400;
    case URSATZ_GUI_MATERIAL_ERR_NOT_FOUND:
      return 404;
    case URSATZ_GUI_MATERIAL_ERR_NOT_EMPTY:
    case URSATZ_GUI_MATERIAL_ERR_REFERENCED:
      return 409;
    default:
      return 500;
  }
}

static const char *material_message(UrsatzGuiMaterialResult result) {
  switch (result) {
    case URSATZ_GUI_MATERIAL_ERR_INVALID_ID:
      return "invalid id";
    case URSATZ_GUI_MATERIAL_ERR_INVALID_NAME:
      return "invalid name, description or note";
    case URSATZ_GUI_MATERIAL_ERR_INVALID_TYPE:
      return "invalid type";
    case URSATZ_GUI_MATERIAL_ERR_INVALID_BODY:
      return "invalid body";
    case URSATZ_GUI_MATERIAL_ERR_NOT_FOUND:
      return "not found";
    case URSATZ_GUI_MATERIAL_ERR_NOT_EMPTY:
      return "the project still holds items";
    case URSATZ_GUI_MATERIAL_ERR_REFERENCED:
      return "the item is used by other items";
    default:
      return "material store failure";
  }
}

static int send_material_failure(struct mg_connection *conn, UrsatzGuiMaterialResult result) {
  return send_failure(conn, material_status(result), material_message(result));
}

/* --- projects ------------------------------------------------------------ */

static int handle_list_projects(struct mg_connection *conn) {
  cJSON *projects = NULL;
  UrsatzGuiMaterialResult result = ursatz_gui_material_project_list(&projects);
  if (result != URSATZ_GUI_MATERIAL_OK) {
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
  UrsatzGuiMaterialResult result = ursatz_gui_material_project_create(name, description, &project);
  cJSON_Delete(body);
  if (result != URSATZ_GUI_MATERIAL_OK) {
    return send_material_failure(conn, result);
  }
  return send_ok_with(conn, "project", project);
}

static int handle_get_project(struct mg_connection *conn, const char *id) {
  cJSON *project = NULL;
  UrsatzGuiMaterialResult result = ursatz_gui_material_project_get(id, &project);
  if (result != URSATZ_GUI_MATERIAL_OK) {
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
  UrsatzGuiMaterialResult result = ursatz_gui_material_project_update(id, name, description, &project);
  cJSON_Delete(body);
  if (result != URSATZ_GUI_MATERIAL_OK) {
    return send_material_failure(conn, result);
  }
  return send_ok_with(conn, "project", project);
}

static int handle_delete_project(struct mg_connection *conn, const char *id) {
  UrsatzGuiMaterialResult result = ursatz_gui_material_project_delete(id);
  if (result != URSATZ_GUI_MATERIAL_OK) {
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
  UrsatzGuiMaterialResult result = ursatz_gui_material_item_list(
      query_value(conn, "project", project, sizeof(project)) ? project : NULL,
      query_value(conn, "type", type, sizeof(type)) ? type : NULL, &items);
  if (result != URSATZ_GUI_MATERIAL_OK) {
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

/* The key of a motive document as "D minor", or false when it has none. The tonic is spelled as the Composer spells it
   (pitch_class_names of its vocabulary). */
static bool import_key_text(const cJSON *document, char *out, size_t out_size) {
  const cJSON *key = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(document, "motive"), "key");
  const cJSON *tonic = cJSON_GetObjectItemCaseSensitive(key, "tonic_pitch_class");
  const cJSON *mode = cJSON_GetObjectItemCaseSensitive(key, "mode");
  char name[16];
  if (!cJSON_IsNumber(tonic) || !cJSON_IsString(mode) ||
      !ursatz_gui_composer_tonic_name(tonic->valueint, mode->valuestring, name, sizeof(name))) {
    return false;
  }
  snprintf(out, out_size, "%s %s", name, mode->valuestring);
  return true;
}

/* ---- degrees to C ------------------------------------------------------ */

typedef struct {
  int status;
  char message[512];
  char path[128];
} ToCFailure;

static void to_c_fail_at(ToCFailure *failure, int status, const char *message, const char *path) {
  failure->status = status;
  snprintf(failure->message, sizeof(failure->message), "%s", message);
  snprintf(failure->path, sizeof(failure->path), "%s", path);
}

static void to_c_fail(ToCFailure *failure, int status, const char *message) {
  to_c_fail_at(failure, status, message, "");
}

/* Writes a full motif's degree list as C notes in mode ("major" or "minor"): the Composer reads the degrees in
   "C <mode>" and answers analysis.notes_text. Returns the notes text (caller frees), or NULL with failure set. */
static char *degrees_to_c_notes(const char *degrees, const char *mode, const char *meter, const char *pickup,
                                ToCFailure *failure) {
  size_t length = degrees ? strlen(degrees) : 0;
  if (length == 0 || length > 4000) {
    to_c_fail(failure, 400, "degrees must be a list of 1 to 4000 characters");
    return NULL;
  }
  if (!mode) {
    to_c_fail(failure, 400, "mode is needed");
    return NULL;
  }
  if (!meter || !pickup) {
    to_c_fail(failure, 400, "meter and pickup are needed");
    return NULL;
  }
  char key[16];
  snprintf(key, sizeof(key), "C %s", mode);
  cJSON *request = ursatz_gui_motif_motive_request("degrees", NULL, degrees, meter, key, pickup);
  UrsatzGuiComposerReply reply;
  bool called = ursatz_gui_motif_motive(request, &reply);
  cJSON_Delete(request);
  if (!called) {
    to_c_fail(failure, 500, "out of memory");
    return NULL;
  }
  char *notes = NULL;
  if (ursatz_gui_composer_reply_refused(&reply)) {
    to_c_fail_at(failure, ursatz_gui_composer_reply_failure_status(&reply), reply.message[0] ? reply.message : "composer could not read the degrees", reply.path);
  } else {
    cJSON *document = reply.status == COMPOSER_OK && reply.json ? cJSON_Parse(reply.json) : NULL;
    const cJSON *text = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(document, "analysis"), "notes_text");
    if (cJSON_IsString(text)) {
      notes = strdup(text->valuestring);
    }
    cJSON_Delete(document);
    if (!notes) {
      to_c_fail(failure, 500, "composer could not write the motif in C");
    }
  }
  ursatz_gui_composer_reply_destroy(&reply);
  return notes;
}

/* POST /api/materials/motifs/to-c {degrees, mode, meter, pickup?}: answers {"ok":true,"notes_text":...} and saves
   nothing. Full motifs only; rhythm and contour lists hold no pitches. */
static int handle_motifs_to_c(struct mg_connection *conn) {
  int status = 0;
  cJSON *body = read_json_body(conn, &status);
  if (!body) {
    return status;
  }
  bool bad = false;
  const char *degrees = optional_string(body, "degrees", &bad);
  const char *mode = optional_string(body, "mode", &bad);
  const char *meter = optional_string(body, "meter", &bad);
  const char *pickup = optional_string(body, "pickup", &bad);
  if (bad) {
    cJSON_Delete(body);
    return send_failure(conn, 400, "degrees, mode, meter and pickup must be text");
  }
  ToCFailure failure = {0, ""};
  char *notes = degrees_to_c_notes(degrees, mode, meter, pickup ? pickup : "0", &failure);
  cJSON_Delete(body);
  if (!notes) {
    return send_failure_at(conn, failure.status, failure.message, failure.path);
  }
  cJSON *answer = cJSON_CreateObject();
  if (!answer || !cJSON_AddBoolToObject(answer, "ok", 1) || !cJSON_AddStringToObject(answer, "notes_text", notes)) {
    cJSON_Delete(answer);
    free(notes);
    return send_failure(conn, 500, "out of memory");
  }
  free(notes);
  return ursatz_gui_send_object(conn, 200, answer);
}

/* POST /api/materials/motifs/import: the body is a MIDI file; ?meter= and ?key= are optional. Answers
   {"ok":true,"notes_text":...,"meter":"4/4","key":"C minor","moved_from":"D minor"|null,"pickup":"0",
   "analysis":{...}} and saves nothing. The notes are moved to C; with no key found they are read as C major. */
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
  cJSON *request = cJSON_CreateObject();
  if (!request || (has_meter && !cJSON_AddStringToObject(request, "meter", meter)) ||
      (has_key && !cJSON_AddStringToObject(request, "key", key))) {
    cJSON_Delete(request);
    free(bytes);
    return send_failure(conn, 500, "out of memory");
  }
  UrsatzGuiComposerReply reply;
  bool called = ursatz_gui_motif_motive_from_midi(bytes, size, request, &reply);
  cJSON_Delete(request);
  free(bytes);
  if (!called) {
    return send_failure(conn, 500, "out of memory");
  }
  if (reply.status != COMPOSER_OK) {
    int failed = send_failure_at(conn, ursatz_gui_composer_reply_failure_status(&reply),
                                 reply.message[0] ? reply.message : "composer could not read the MIDI file",
                                 reply.path);
    ursatz_gui_composer_reply_destroy(&reply);
    return failed;
  }
  cJSON *document = reply.json ? cJSON_Parse(reply.json) : NULL;
  ursatz_gui_composer_reply_destroy(&reply);
  const cJSON *analysis = cJSON_GetObjectItemCaseSensitive(document, "analysis");
  const cJSON *notes_text = cJSON_GetObjectItemCaseSensitive(analysis, "notes_text");
  const cJSON *pickup = cJSON_GetObjectItemCaseSensitive(analysis, "pickup");
  const cJSON *bar = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(document, "motive"), "meter");
  const cJSON *beats = cJSON_GetObjectItemCaseSensitive(bar, "beats_per_bar");
  const cJSON *unit = cJSON_GetObjectItemCaseSensitive(bar, "beat_unit");
  if (!cJSON_IsString(notes_text) || !cJSON_IsString(pickup) || !cJSON_IsNumber(beats) || !cJSON_IsNumber(unit)) {
    cJSON_Delete(document);
    return send_failure(conn, 500, "composer could not analyze the MIDI file");
  }
  char meter_text[16];
  char key_text[32];
  snprintf(meter_text, sizeof(meter_text), "%d/%d", beats->valueint, unit->valueint);
  bool keyed = import_key_text(document, key_text, sizeof(key_text));
  /* Motifs are written with C as the tonic (R-179): a key on another tonic moves the notes to C in the same mode;
     no key found reads the notes as C major. */
  const char *final_notes = notes_text->valuestring;
  const char *final_key = "C major";
  const char *moved_from = NULL;
  char *moved_notes = NULL;
  char c_key[16];
  if (keyed) {
    const char *mode = strchr(key_text, ' ') + 1;
    snprintf(c_key, sizeof(c_key), "C %s", mode);
    final_key = c_key;
    if (strncmp(key_text, "C ", 2) != 0) {
      const cJSON *degrees_text = cJSON_GetObjectItemCaseSensitive(analysis, "degrees_text");
      ToCFailure failure = {0, ""};
      moved_notes = degrees_to_c_notes(cJSON_IsString(degrees_text) ? degrees_text->valuestring : NULL, mode, meter_text,
                                       pickup->valuestring, &failure);
      if (!moved_notes) {
        cJSON_Delete(document);
        return send_failure_at(conn, failure.status, failure.message, failure.path);
      }
      final_notes = moved_notes;
      moved_from = key_text;
    }
  }
  cJSON *answer = cJSON_CreateObject();
  if (!answer) {
    free(moved_notes);
    cJSON_Delete(document);
    return send_failure(conn, 500, "out of memory");
  }
  cJSON_AddBoolToObject(answer, "ok", 1);
  cJSON_AddStringToObject(answer, "notes_text", final_notes);
  cJSON_AddStringToObject(answer, "meter", meter_text);
  cJSON_AddStringToObject(answer, "key", final_key);
  cJSON_AddBoolToObject(answer, "key_found", keyed);
  if (moved_from) {
    cJSON_AddStringToObject(answer, "moved_from", moved_from);
  } else {
    cJSON_AddNullToObject(answer, "moved_from");
  }
  free(moved_notes);
  cJSON_AddStringToObject(answer, "pickup", pickup->valuestring);
  cJSON_AddItemToObject(answer, "analysis", cJSON_DetachItemFromObjectCaseSensitive(document, "analysis"));
  cJSON_Delete(document);
  return ursatz_gui_send_object(conn, 200, answer);
}

typedef struct {
  const char *form;
  const char *degrees;
  const char *meter;
  const char *key;
  const char *pickup;
  const char *render_key;
  const char *role;
  const char *harmony;
  const cJSON *features;
  const char *tempo; /* the Composer reads it: a whole number or a fraction like 195/2; NULL for none */
} RenderInput;

typedef struct {
  int status;
  char message[512];
  char path[128];
} RenderFailure;

static void render_fail_at(RenderFailure *failure, int status, const char *message, const char *path) {
  failure->status = status;
  snprintf(failure->message, sizeof(failure->message), "%s", message);
  snprintf(failure->path, sizeof(failure->path), "%s", path);
}

static void render_fail(RenderFailure *failure, int status, const char *message) {
  render_fail_at(failure, status, message, "");
}

/* Renders a degree, rhythm or contour list in render_key with the Composer. On success *out_xml is the MusicXML text
   and *out_midi (*out_midi_size bytes) the render's MIDI file, both caller-owned; playback is built from the MIDI by
   the Analyzer (R-275). On failure failure holds the status and message. */
static bool render_motif(const RenderInput *in, char **out_xml, uint8_t **out_midi, size_t *out_midi_size,
                         RenderFailure *failure) {
  const char *form = in->form;
  if (!form) {
    render_fail(failure, 400, "form is needed");
    return false;
  }
  size_t degrees_length = in->degrees ? strlen(in->degrees) : 0;
  if (degrees_length == 0 || degrees_length > 4000) {
    render_fail(failure, 400, "degrees must be a list of 1 to 4000 characters");
    return false;
  }
  bool has_key = in->key && in->key[0] != '\0';
  const char *render_key = in->render_key && in->render_key[0] != '\0' ? in->render_key : (has_key ? in->key : NULL);
  if (!in->meter || !in->pickup) {
    render_fail(failure, 400, "meter and pickup are needed");
    return false;
  }
  if (!render_key) {
    render_fail(failure, 400, "give a key");
    return false;
  }
  bool full = strcmp(form, "full") == 0;
  cJSON *request = ursatz_gui_motif_motive_request(full ? "degrees" : form, form, in->degrees, in->meter, has_key ? in->key : NULL, in->pickup);
  if (!request || !ursatz_gui_motif_add_extras(request, in->harmony, in->features, ursatz_gui_motif_role_option(in->role)) ||
      !cJSON_AddStringToObject(request, "render_key", render_key) || !cJSON_AddTrueToObject(request, "musicxml") ||
      !cJSON_AddTrueToObject(request, "midi") ||
      (in->tempo && !cJSON_AddStringToObject(request, "tempo", in->tempo))) {
    cJSON_Delete(request);
    render_fail(failure, 500, "out of memory");
    return false;
  }
  UrsatzGuiComposerReply reply;
  bool called = ursatz_gui_motif_motive_render(request, &reply);
  cJSON_Delete(request);
  if (!called) {
    render_fail(failure, 500, "out of memory");
    return false;
  }
  bool ok = false;
  if (ursatz_gui_composer_reply_refused(&reply)) {
    render_fail_at(failure, ursatz_gui_composer_reply_failure_status(&reply), reply.message[0] ? reply.message : "composer could not render the motif", reply.path);
  } else {
    char *xml = reply.status == COMPOSER_OK && reply.musicxml && reply.midi ? strdup(reply.musicxml) : NULL;
    if (xml) {
      *out_xml = xml;
      *out_midi = reply.midi;
      *out_midi_size = reply.midi_size;
      reply.midi = NULL;
      reply.midi_size = 0;
      ok = true;
    } else {
      render_fail(failure, 500, "composer could not render the motif");
    }
  }
  ursatz_gui_composer_reply_destroy(&reply);
  return ok;
}

/* The text of a refusal to build a playback; every one is a 500 (the file came from the Composer, not the request). */
static const char *playback_failure_text(UrsatzGuiPlaybackResult result) {
  return result == URSATZ_GUI_PLAYBACK_ERR_ALLOC ? "out of memory" : "could not build the playback from the MIDI file";
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

/* Sends {"ok":true,"musicxml":...,"playback":{...}} and frees xml and playback. */
static int send_render_answer(struct mg_connection *conn, char *xml, cJSON *playback) {
  cJSON *answer = cJSON_CreateObject();
  if (!answer || !cJSON_AddBoolToObject(answer, "ok", 1) || !cJSON_AddStringToObject(answer, "musicxml", xml)) {
    cJSON_Delete(answer);
    cJSON_Delete(playback);
    free(xml);
    return send_failure(conn, 500, "out of memory");
  }
  free(xml);
  cJSON_AddItemToObject(answer, "playback", playback);
  return ursatz_gui_send_object(conn, 200, answer);
}

/* POST /api/materials/render {form, degrees, meter, key?, pickup?, render_key?, tempo?, role?}: nothing is saved. */
static int handle_render(struct mg_connection *conn) {
  int status = 0;
  cJSON *body = read_json_body(conn, &status);
  if (!body) {
    return status;
  }
  bool bad = false;
  RenderInput in = {0};
  in.form = optional_string(body, "form", &bad);
  in.degrees = optional_string(body, "degrees", &bad);
  in.meter = optional_string(body, "meter", &bad);
  in.key = optional_string(body, "key", &bad);
  in.pickup = optional_string(body, "pickup", &bad);
  in.render_key = optional_string(body, "render_key", &bad);
  in.role = optional_string(body, "role", &bad);
  in.harmony = optional_string(body, "harmony", &bad);
  in.features = cJSON_GetObjectItemCaseSensitive(body, "features");
  if (!in.pickup) {
    in.pickup = "0";
  }
  const cJSON *tempo = cJSON_GetObjectItemCaseSensitive(body, "tempo");
  char tempo_text[48] = "";
  if (cJSON_IsString(tempo)) {
    in.tempo = tempo->valuestring;
  } else if (cJSON_IsNumber(tempo)) {
    snprintf(tempo_text, sizeof(tempo_text), "%.15g", tempo->valuedouble);
    in.tempo = tempo_text;
  } else if (tempo && !cJSON_IsNull(tempo)) {
    bad = true;
  }
  if (bad) {
    cJSON_Delete(body);
    return send_failure(conn, 400, "form, degrees, meter, key, pickup, render_key, role and harmony must be text and tempo a number or text like 195/2");
  }
  char *xml = NULL;
  uint8_t *midi = NULL;
  size_t midi_size = 0;
  RenderFailure failure = {0, ""};
  bool ok = render_motif(&in, &xml, &midi, &midi_size, &failure);
  cJSON_Delete(body);
  if (!ok) {
    return send_failure_at(conn, failure.status, failure.message, failure.path);
  }
  /* An unsaved render has no folder: its MIDI goes to a temporary file for the Analyzer, which imports by path. */
  char tmp_dir[512];
  char *playback_text = NULL;
  UrsatzGuiPlaybackResult built = ursatz_gui_material_render_tmp_dir(tmp_dir, sizeof(tmp_dir))
                                      ? ursatz_gui_playback_of_midi_bytes(midi, midi_size, tmp_dir, &playback_text)
                                      : URSATZ_GUI_PLAYBACK_ERR_IO;
  free(midi);
  cJSON *playback = built == URSATZ_GUI_PLAYBACK_OK ? cJSON_Parse(playback_text) : NULL;
  free(playback_text);
  if (!playback) {
    free(xml);
    return send_failure(conn, 500, playback_failure_text(built));
  }
  return send_render_answer(conn, xml, playback);
}

/* A motif body must name C as its tonic (R-179): "C major" or "C minor". Bodies without a motif are left alone. */
static bool motif_tonic_ok(const cJSON *version_body) {
  const cJSON *motif = cJSON_GetObjectItemCaseSensitive(version_body, "motif");
  if (!cJSON_IsObject(motif)) {
    return true;
  }
  const cJSON *key = cJSON_GetObjectItemCaseSensitive(motif, "key");
  return cJSON_IsString(key) && (strcmp(key->valuestring, "C major") == 0 || strcmp(key->valuestring, "C minor") == 0);
}

/* True when text has a character that is not white space. */
static bool has_text(const char *text) {
  for (; text && *text; text++) {
    if (!isspace((unsigned char)*text)) {
      return true;
    }
  }
  return false;
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
  if (!motif_tonic_ok(version_body)) {
    cJSON_Delete(body);
    return send_failure(conn, 400, "motifs are written with C as the tonic");
  }
  char analysis_error[URSATZ_GUI_MOTIF_ANALYSIS_ERROR_SIZE];
  char refusal[512] = "";
  char refusal_path[128] = "";
  UrsatzGuiMotifAnalysis analysis = ursatz_gui_motif_analyze_body((cJSON *)version_body, NULL, analysis_error, refusal, sizeof(refusal),
                                                refusal_path, sizeof(refusal_path));
  if (analysis != URSATZ_GUI_MOTIF_ANALYSIS_DONE) {
    cJSON_Delete(body);
    return analysis == URSATZ_GUI_MOTIF_ANALYSIS_REFUSED ? send_failure_at(conn, 400, refusal, refusal_path)
                                        : send_failure(conn, 500, "out of memory");
  }
  cJSON *item = NULL;
  /* A new motif's or theme's first version is noted "Created" unless the caller gives a note. */
  const char *first_note = has_text(note)                                             ? note
                           : strcmp(type, "motif") == 0 || strcmp(type, "theme") == 0 ? "Created"
                                                                                       : NULL;
  UrsatzGuiMaterialResult result =
      ursatz_gui_material_item_create_noted(type, name, project_id, version_body, first_note, &item);
  cJSON_Delete(body);
  if (result != URSATZ_GUI_MATERIAL_OK) {
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
  UrsatzGuiMaterialResult result = ursatz_gui_material_item_get(id, &meta, &body);
  if (result == URSATZ_GUI_MATERIAL_OK) {
    result = ursatz_gui_material_uses_status(id, &uses_status);
  }
  if (result == URSATZ_GUI_MATERIAL_OK) {
    result = ursatz_gui_material_used_by(id, &used_by);
  }
  if (result == URSATZ_GUI_MATERIAL_OK) {
    subjects = ursatz_gui_materials_subjects_for("items", id);
  }
  cJSON *object = result == URSATZ_GUI_MATERIAL_OK && subjects ? cJSON_CreateObject() : NULL;
  if (!object || !cJSON_AddBoolToObject(object, "ok", 1) || !cJSON_AddItemToObject(object, "item", meta)) {
    cJSON_Delete(object);
    cJSON_Delete(meta);
    cJSON_Delete(body);
    cJSON_Delete(uses_status);
    cJSON_Delete(used_by);
    cJSON_Delete(subjects);
    return result == URSATZ_GUI_MATERIAL_OK ? send_failure(conn, 500, "out of memory")
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
  UrsatzGuiMaterialResult result = URSATZ_GUI_MATERIAL_OK;
  if (project_id) {
    cJSON *project = NULL;
    result = ursatz_gui_material_project_get(project_id, &project);
    cJSON_Delete(project);
  }
  if (result == URSATZ_GUI_MATERIAL_OK && name) {
    result = ursatz_gui_material_item_rename(id, name, &item);
  }
  if (result == URSATZ_GUI_MATERIAL_OK && project_id) {
    cJSON_Delete(item);
    item = NULL;
    result = ursatz_gui_material_item_move(id, project_id, &item);
  }
  cJSON_Delete(body);
  if (result != URSATZ_GUI_MATERIAL_OK) {
    cJSON_Delete(item);
    return send_material_failure(conn, result);
  }
  return send_ok_with(conn, "item", item);
}

static int handle_delete_item(struct mg_connection *conn, const char *id) {
  char force[8];
  bool forced = query_value(conn, "force", force, sizeof(force)) && strcmp(force, "1") == 0;
  UrsatzGuiMaterialResult result = ursatz_gui_material_item_delete(id, forced);
  if (result == URSATZ_GUI_MATERIAL_ERR_REFERENCED) {
    cJSON *used_by = NULL;
    cJSON *object = cJSON_CreateObject();
    if (ursatz_gui_material_delete_blockers(id, &used_by) != URSATZ_GUI_MATERIAL_OK || !object ||
        !cJSON_AddBoolToObject(object, "ok", 0) || !cJSON_AddStringToObject(object, "error", material_message(result))) {
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
  if (result != URSATZ_GUI_MATERIAL_OK) {
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
  if ((cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(version_body, "motif")) ||
       cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(version_body, "theme"))) && !has_text(note)) {
    cJSON_Delete(body);
    return send_failure(conn, 400, "give a version note");
  }
  if (!motif_tonic_ok(version_body)) {
    cJSON_Delete(body);
    return send_failure(conn, 400, "motifs are written with C as the tonic");
  }
  char analysis_error[URSATZ_GUI_MOTIF_ANALYSIS_ERROR_SIZE];
  char refusal[512] = "";
  char refusal_path[128] = "";
  /* The previous version tells whether proposed segments are to be proposed again (R-312). */
  cJSON *previous_meta = NULL;
  cJSON *previous_body = NULL;
  if (ursatz_gui_material_item_get(id, &previous_meta, &previous_body) != URSATZ_GUI_MATERIAL_OK) {
    previous_body = NULL;
  }
  cJSON_Delete(previous_meta);
  UrsatzGuiMotifAnalysis analysis = ursatz_gui_motif_analyze_body((cJSON *)version_body, previous_body, analysis_error, refusal,
                                                sizeof(refusal), refusal_path, sizeof(refusal_path));
  cJSON_Delete(previous_body);
  if (analysis != URSATZ_GUI_MOTIF_ANALYSIS_DONE) {
    cJSON_Delete(body);
    return analysis == URSATZ_GUI_MOTIF_ANALYSIS_REFUSED ? send_failure_at(conn, 400, refusal, refusal_path)
                                        : send_failure(conn, 500, "out of memory");
  }
  cJSON *item = NULL;
  UrsatzGuiMaterialResult result = ursatz_gui_material_item_add_version(id, version_body, note, &item);
  cJSON_Delete(body);
  if (result != URSATZ_GUI_MATERIAL_OK) {
    return send_material_failure(conn, result);
  }
  return send_item_answer(conn, item, analysis_error);
}

/* POST /api/materials/items/<id>/reanalyze: analyzes the current version of a motif again and saves the result as a
   new version (versions are never rewritten, R-130). A refusal writes nothing. */
static int handle_reanalyze(struct mg_connection *conn, const char *id) {
  cJSON *meta = NULL;
  cJSON *version_body = NULL;
  UrsatzGuiMaterialResult result = ursatz_gui_material_item_get(id, &meta, &version_body);
  if (result != URSATZ_GUI_MATERIAL_OK) {
    return send_material_failure(conn, result);
  }
  const cJSON *type = cJSON_GetObjectItemCaseSensitive(meta, "type");
  bool motif = cJSON_IsString(type) && strcmp(type->valuestring, "motif") == 0;
  cJSON_Delete(meta);
  if (!motif) {
    cJSON_Delete(version_body);
    return send_failure(conn, 400, "only motifs are analyzed");
  }
  char analysis_error[URSATZ_GUI_MOTIF_ANALYSIS_ERROR_SIZE];
  char refusal[512] = "";
  char refusal_path[128] = "";
  UrsatzGuiMotifAnalysis analysis = ursatz_gui_motif_analyze_body(version_body, NULL, analysis_error, refusal, sizeof(refusal), refusal_path,
                                                sizeof(refusal_path));
  if (analysis != URSATZ_GUI_MOTIF_ANALYSIS_DONE) {
    cJSON_Delete(version_body);
    return analysis == URSATZ_GUI_MOTIF_ANALYSIS_REFUSED ? send_failure_at(conn, 400, refusal, refusal_path)
                                        : send_failure(conn, 500, "out of memory");
  }
  cJSON *item = NULL;
  result = ursatz_gui_material_item_add_version(id, version_body, "Re-analyzed", &item);
  cJSON_Delete(version_body);
  if (result != URSATZ_GUI_MATERIAL_OK) {
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
  UrsatzGuiMaterialResult result = ursatz_gui_material_item_get_version(id, (int)number, &version_body);
  if (result != URSATZ_GUI_MATERIAL_OK) {
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
  UrsatzGuiMaterialResult found = ursatz_gui_material_item_get(id, &meta, &stored);
  if (found != URSATZ_GUI_MATERIAL_OK) {
    cJSON_Delete(body);
    return send_material_failure(conn, found);
  }
  const cJSON *type = cJSON_GetObjectItemCaseSensitive(meta, "type");
  bool piece = cJSON_IsString(type) && strcmp(type->valuestring, "piece") == 0;
  cJSON_Delete(meta);
  cJSON_Delete(stored);
  long long chosen = has_seed ? (long long)seed->valuedouble : 0;
  /* Composed on the job queue: the answer is the job (GET /api/jobs/<id>). */
  UrsatzGuiComposeOp op = piece ? URSATZ_GUI_COMPOSE_PIECE : URSATZ_GUI_COMPOSE_THEME;
  UrsatzGuiComposeJob *job = ursatz_gui_compose_job_new(op, id);
  if (job) {
    job->has_seed = has_seed;
    job->seed = chosen;
    job->update_uses = cJSON_IsTrue(update_uses);
  }
  cJSON_Delete(body);
  if (!job) {
    return send_failure(conn, 500, "out of memory");
  }
  return ursatz_gui_jobs_send_accepted(conn, ursatz_gui_compose_job_kind(op), ursatz_gui_compose_job_run, job,
                                       ursatz_gui_compose_job_free);
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
  UrsatzGuiComposeJob *job = ursatz_gui_compose_job_new(URSATZ_GUI_COMPOSE_PIECE_REGENERATE, id);
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
    ursatz_gui_compose_job_free(job);
    return send_failure(conn, 500, "out of memory");
  }
  return ursatz_gui_jobs_send_accepted(conn, "regenerate", ursatz_gui_compose_job_run, job, ursatz_gui_compose_job_free);
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
  UrsatzGuiComposeJob *job = ursatz_gui_compose_job_new(URSATZ_GUI_COMPOSE_PIECE_VARIATIONS, id);
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
    ursatz_gui_compose_job_free(job);
    return send_failure(conn, 500, "out of memory");
  }
  return ursatz_gui_jobs_send_accepted(conn, "variations", ursatz_gui_compose_job_run, job, ursatz_gui_compose_job_free);
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
      ursatz_gui_piece_variation_file(id, version, set, name, &data, &size, &type) != URSATZ_GUI_THEME_OK) {
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
  UrsatzGuiComposeJob *job = ursatz_gui_compose_job_new(URSATZ_GUI_COMPOSE_PIECE_CHOOSE, id);
  if (job) {
    job->version = version;
    job->set = set;
    job->index = (int)index->valuedouble;
  }
  cJSON_Delete(body);
  if (!job) {
    return send_failure(conn, 500, "out of memory");
  }
  return ursatz_gui_jobs_send_accepted(conn, "choose", ursatz_gui_compose_job_run, job, ursatz_gui_compose_job_free);
}

/* GET /api/materials/items/<id>/versions/<n>/files/<name>: one of a theme or piece version's files. */
static int handle_version_file(struct mg_connection *conn, const char *id, const char *number_text, const char *name) {
  char *end = NULL;
  long number = strtol(number_text, &end, 10);
  const char *content_type = ursatz_gui_material_version_file_type(name);
  if (end == number_text || *end != '\0' || number < 1 || number > 1000000 || !content_type) {
    return send_failure(conn, 404, "file not found");
  }
  unsigned char *data = NULL;
  size_t size = 0;
  UrsatzGuiMaterialResult result = ursatz_gui_material_version_file(id, (int)number, name, &data, &size);
  if (result != URSATZ_GUI_MATERIAL_OK) {
    return result == URSATZ_GUI_MATERIAL_ERR_NOT_FOUND ? send_failure(conn, 404, "file not found")
                                                        : send_material_failure(conn, result);
  }
  mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n",
            content_type, size);
  mg_write(conn, data, size);
  free(data);
  return 200;
}

/* The render cache of a motif version (R-275): items/<id>/v<n>/render/ holds score.musicxml and motif.mid, and
   render.json {api_version, composer_version, ursatz_commit, render_key, tempo} names what they were made with. A
   version never changes, so the files stand for the same key, tempo and Composer build (its API version, release and
   Ursatz commit, composer_api_versions). */
static bool render_cache_dir(const char *id, int number, char *out, size_t out_size) {
  char version_dir[512];
  return ursatz_gui_material_version_dir(id, number, version_dir, sizeof(version_dir)) &&
         snprintf(out, out_size, "%s/render", version_dir) < (int)out_size;
}

static bool render_cache_file(const char *dir, const char *name, char *out, size_t out_size) {
  return snprintf(out, out_size, "%s/%s", dir, name) < (int)out_size;
}

/* True when the string member name of meta equals value and value is not empty. */
static bool render_meta_is(const cJSON *meta, const char *name, const char *value) {
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(meta, name);
  return cJSON_IsString(item) && value[0] != '\0' && strcmp(item->valuestring, value) == 0;
}

/* True when render.json in dir names this render key, tempo and the linked Composer build. */
static bool render_cache_matches(const char *dir, const char *render_key, int tempo) {
  char path[640];
  if (!render_cache_file(dir, "render.json", path, sizeof(path))) {
    return false;
  }
  cJSON *meta = ursatz_gui_read_json_object(path);
  UrsatzGuiComposerBuild build = ursatz_gui_composer_build();
  const cJSON *key = cJSON_GetObjectItemCaseSensitive(meta, "render_key");
  const cJSON *cached_tempo = cJSON_GetObjectItemCaseSensitive(meta, "tempo");
  bool same = render_meta_is(meta, "api_version", build.api_version) &&
              render_meta_is(meta, "composer_version", build.composer_version) &&
              render_meta_is(meta, "ursatz_commit", build.ursatz_commit) && cJSON_IsString(key) &&
              strcmp(key->valuestring, render_key) == 0 && cJSON_IsNumber(cached_tempo) && cached_tempo->valueint == tempo;
  cJSON_Delete(meta);
  return same;
}

/* The cached MusicXML text of a version for this render key and tempo (caller frees), or NULL. */
static char *render_cache_read_score(const char *id, int number, const char *render_key, int tempo) {
  char dir[576];
  char path[640];
  if (!render_cache_dir(id, number, dir, sizeof(dir)) || !render_cache_matches(dir, render_key, tempo) ||
      !render_cache_file(dir, "score.musicxml", path, sizeof(path))) {
    return NULL;
  }
  return ursatz_gui_read_file(path, NULL);
}

/* Stores a render. The files go first and render.json last, so a half-written cache never matches. A failure is
   silent: the next request renders again. */
static void render_cache_write(const char *id, int number, const char *render_key, int tempo, const char *xml,
                               const uint8_t *midi, size_t midi_size) {
  char version_dir[512];
  char dir[576];
  char path[640];
  if (!ursatz_gui_material_version_dir(id, number, version_dir, sizeof(version_dir)) ||
      !render_cache_dir(id, number, dir, sizeof(dir)) || !ursatz_gui_ensure_dir(version_dir) ||
      !ursatz_gui_ensure_dir(dir)) {
    return;
  }
  cJSON *meta = cJSON_CreateObject();
  if (!meta) {
    return;
  }
  UrsatzGuiComposerBuild build = ursatz_gui_composer_build();
  cJSON_AddStringToObject(meta, "api_version", build.api_version);
  cJSON_AddStringToObject(meta, "composer_version", build.composer_version);
  cJSON_AddStringToObject(meta, "ursatz_commit", build.ursatz_commit);
  cJSON_AddStringToObject(meta, "render_key", render_key);
  cJSON_AddNumberToObject(meta, "tempo", tempo);
  char *meta_text = cJSON_PrintUnformatted(meta);
  cJSON_Delete(meta);
  if (meta_text && render_cache_file(dir, "score.musicxml", path, sizeof(path)) &&
      ursatz_gui_write_file_atomic(path, xml, strlen(xml)) && render_cache_file(dir, "motif.mid", path, sizeof(path)) &&
      ursatz_gui_write_file_atomic(path, midi, midi_size) && render_cache_file(dir, "render.json", path, sizeof(path))) {
    ursatz_gui_write_file_atomic(path, meta_text, strlen(meta_text));
  }
  free(meta_text);
}

/* Sends a file's bytes with a content type. */
static int send_bytes(struct mg_connection *conn, const char *type, const void *data, size_t size) {
  mg_send_http_ok(conn, type, (long long)size);
  mg_write(conn, data, size);
  return 200;
}

/* GET /api/materials/items/<id>/versions/<n>/playback?file=theme|loop|ending: the playback of a theme or piece
   version's MIDI file, built by the Analyzer (R-275). */
static int handle_version_file_playback(struct mg_connection *conn, const char *id, int number, const char *file) {
  const char *name = strcmp(file, "theme") == 0 ? "theme.mid" : strcmp(file, "loop") == 0 ? "loop.mid"
                     : strcmp(file, "ending") == 0 ? "ending.mid" : NULL;
  char dir[512];
  char path[640];
  cJSON *version_body = NULL;
  if (!name) {
    return send_failure(conn, 400, "file must be theme, loop or ending");
  }
  UrsatzGuiMaterialResult result = ursatz_gui_material_item_get_version(id, number, &version_body);
  if (result != URSATZ_GUI_MATERIAL_OK) {
    return send_material_failure(conn, result);
  }
  cJSON_Delete(version_body);
  FILE *exists = ursatz_gui_material_version_dir(id, number, dir, sizeof(dir)) &&
                         snprintf(path, sizeof(path), "%s/%s", dir, name) < (int)sizeof(path)
                     ? fopen(path, "rb")
                     : NULL;
  if (!exists) {
    return send_failure(conn, 404, "file not found");
  }
  fclose(exists);
  char *json = NULL;
  UrsatzGuiPlaybackResult built = ursatz_gui_playback_of_midi(path, &json);
  return built == URSATZ_GUI_PLAYBACK_OK ? send_playback_text(conn, json)
                                         : send_failure(conn, 500, playback_failure_text(built));
}

/* GET /api/materials/items/<pce-id>/versions/<n>/variations/<set>/<file>/playback: the playback of a variation's MIDI
   file (variation-<i>.mid). */
static int handle_piece_variation_playback(struct mg_connection *conn, const char *id, const char *number_text,
                                           const char *set_text, const char *name) {
  int version = 0;
  int set = 0;
  char path[640];
  if (!parse_version_number(number_text, &version) || !parse_version_number(set_text, &set) ||
      ursatz_gui_piece_variation_midi_path(id, version, set, name, path, sizeof(path)) != URSATZ_GUI_THEME_OK) {
    return send_failure(conn, 404, "file not found");
  }
  char *json = NULL;
  UrsatzGuiPlaybackResult built = ursatz_gui_playback_of_midi(path, &json);
  return built == URSATZ_GUI_PLAYBACK_OK ? send_playback_text(conn, json)
                                         : send_failure(conn, 500, playback_failure_text(built));
}

/* GET /api/materials/items/<id>/versions/<n>/score and .../playback, with ?key= for the key to sound it in (default:
   the motif's own key). Both come from the version's render (motif.degrees through the Composer), cached per version
   so the Composer runs at most once for a key and tempo. With ?file= the playback route answers for a theme or piece
   version's own MIDI file instead. */
static int handle_version_render(struct mg_connection *conn, const char *id, const char *number_text, bool playback_only) {
  char *end = NULL;
  long number = strtol(number_text, &end, 10);
  if (end == number_text || *end != '\0' || number < 1 || number > 1000000) {
    return send_failure(conn, 404, "version not found");
  }
  char file[16];
  if (playback_only && query_value(conn, "file", file, sizeof(file))) {
    return handle_version_file_playback(conn, id, (int)number, file);
  }
  char render_key[32];
  bool has_render_key = query_value(conn, "key", render_key, sizeof(render_key));
  cJSON *version_body = NULL;
  UrsatzGuiMaterialResult result = ursatz_gui_material_item_get_version(id, (int)number, &version_body);
  if (result != URSATZ_GUI_MATERIAL_OK) {
    return send_material_failure(conn, result);
  }
  const cJSON *motif = cJSON_GetObjectItemCaseSensitive(version_body, "motif");
  RenderInput in = {0};
  in.form = optional_string(motif, "form", &(bool){false});
  in.degrees = optional_string(motif, "degrees", &(bool){false});
  in.meter = optional_string(motif, "meter", &(bool){false});
  in.key = optional_string(motif, "key", &(bool){false});
  in.pickup = optional_string(motif, "pickup", &(bool){false});
  in.role = optional_string(version_body, "role", &(bool){false});
  if (!in.role) {
    in.role = optional_string(motif, "role", &(bool){false});
  }
  in.harmony = ursatz_gui_motif_harmony(motif);
  in.features = cJSON_GetObjectItemCaseSensitive(motif, "features");
  in.render_key = has_render_key ? render_key : NULL;
  if (!cJSON_IsObject(motif) || !in.degrees) {
    cJSON_Delete(version_body);
    return send_failure(conn, 404, "this version has no degrees to render");
  }
  if (!in.pickup) {
    in.pickup = "0";
  }
  /* A copy: the key text lives in version_body, which is freed before the cache is written. */
  char effective_key_text[64] = "";
  const char *key_source = in.render_key && in.render_key[0] != '\0' ? in.render_key : in.key;
  if (key_source) {
    snprintf(effective_key_text, sizeof(effective_key_text), "%s", key_source);
  }
  const char *effective_key = key_source ? effective_key_text : NULL;
  char cache_dir[576];
  char midi_path[640];
  bool cached = effective_key && render_cache_dir(id, (int)number, cache_dir, sizeof(cache_dir)) &&
                render_cache_matches(cache_dir, effective_key, 0) &&
                snprintf(midi_path, sizeof(midi_path), "%s/motif.mid", cache_dir) < (int)sizeof(midi_path);
  if (cached && !playback_only) {
    char *score = render_cache_read_score(id, (int)number, effective_key, 0);
    if (score) {
      cJSON_Delete(version_body);
      send_bytes(conn, "application/xml", score, strlen(score));
      free(score);
      return 200;
    }
  }
  if (cached && playback_only) {
    char *json = NULL;
    if (ursatz_gui_playback_of_midi(midi_path, &json) == URSATZ_GUI_PLAYBACK_OK) {
      cJSON_Delete(version_body);
      return send_playback_text(conn, json);
    }
  }
  char *xml = NULL;
  uint8_t *midi = NULL;
  size_t midi_size = 0;
  RenderFailure failure = {0, ""};
  bool ok = render_motif(&in, &xml, &midi, &midi_size, &failure);
  cJSON_Delete(version_body);
  if (!ok) {
    return send_failure_at(conn, failure.status, failure.message, failure.path);
  }
  if (effective_key) {
    render_cache_write(id, (int)number, effective_key, 0, xml, midi, midi_size);
  }
  int answered;
  if (playback_only) {
    char tmp_dir[512];
    char *json = NULL;
    UrsatzGuiPlaybackResult built = ursatz_gui_material_render_tmp_dir(tmp_dir, sizeof(tmp_dir))
                                        ? ursatz_gui_playback_of_midi_bytes(midi, midi_size, tmp_dir, &json)
                                        : URSATZ_GUI_PLAYBACK_ERR_IO;
    answered = built == URSATZ_GUI_PLAYBACK_OK ? send_playback_text(conn, json)
                                               : send_failure(conn, 500, playback_failure_text(built));
  } else {
    answered = send_bytes(conn, "application/xml", xml, strlen(xml));
  }
  free(midi);
  free(xml);
  return answered;
}

/* ---- variants ---------------------------------------------------------- */

/* GET /api/materials/variant-operations: the Composer's operation list, as the Composer API gives it. */
static int handle_variant_operations(struct mg_connection *conn) {
  UrsatzGuiComposerReply reply;
  ursatz_gui_motif_variant_operations(&reply);
  cJSON *document = reply.status == COMPOSER_OK && reply.json ? cJSON_Parse(reply.json) : NULL;
  if (!document) {
    int failed = send_failure_at(conn, 500, reply.message[0] ? reply.message : "composer could not list the operations",
                                 reply.path);
    ursatz_gui_composer_reply_destroy(&reply);
    return failed;
  }
  ursatz_gui_composer_reply_destroy(&reply);
  if (!cJSON_AddBoolToObject(document, "ok", 1)) {
    cJSON_Delete(document);
    return send_failure(conn, 500, "out of memory");
  }
  return ursatz_gui_send_object(conn, 200, document);
}

/* The names --function takes, the vocabulary's functions; the stored derived.functions hold the same. */
static bool function_name_ok(const char *name) {
  return ursatz_gui_composer_vocabulary_has("functions", name);
}

/* The version body of the basic motive of motif's lineage: the end of its derived.source_item links, at most 32 hops.
   Caller-owned; NULL when the motif is not derived or the lineage cannot be read. */
static cJSON *lineage_root(const cJSON *motif) {
  cJSON *root = NULL;
  const cJSON *derived = cJSON_GetObjectItemCaseSensitive(motif, "derived");
  for (int hops = 0; hops < 32 && cJSON_IsObject(derived); hops++) {
    const cJSON *source = cJSON_GetObjectItemCaseSensitive(derived, "source_item");
    cJSON *body = NULL;
    if (!cJSON_IsString(source) || ursatz_gui_material_item_get(source->valuestring, NULL, &body) != URSATZ_GUI_MATERIAL_OK) {
      break;
    }
    cJSON_Delete(root);
    root = body;
    derived = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(root, "motif"), "derived");
  }
  return cJSON_IsObject(derived) ? (cJSON_Delete(root), NULL) : root;
}

/* The largest seed a variants request takes: limits.seed_max.variants of the vocabulary, -1 when it is not given (no
   seed is then valid). */
static double max_variant_seed(void) {
  return ursatz_gui_composer_seed_max("variants");
}


/* A variants run on the job queue: the variants of the item's motif, the one variant per operation (each-op), or a reroll
   (a new version of a derived motif). The queue owns it. */
typedef enum { VARIANTS_JOB_VARIANTS, VARIANTS_JOB_EACH_OP, VARIANTS_JOB_REROLL } VariantsJobOp;

typedef struct {
  VariantsJobOp op;
  char *id;
  cJSON *body; /* the request body of the variants route */
  int count;
  double seed; /* negative: from the clock */
} VariantsJob;

static void variants_job_run(void *input, const ComposerJob *job, UrsatzGuiJobResult *out);

static void variants_job_free(void *input) {
  VariantsJob *job = input;
  if (job) {
    cJSON_Delete(job->body);
    free(job->id);
    free(job);
  }
}

/* A job input; takes body. NULL on allocation failure (body is freed too). */
static VariantsJob *variants_job_new(VariantsJobOp op, const char *id, cJSON *body, int count, double seed) {
  VariantsJob *job = calloc(1, sizeof(*job));
  if (job) {
    job->id = strdup(id);
  }
  if (!job || !job->id) {
    free(job);
    cJSON_Delete(body);
    return NULL;
  }
  job->op = op;
  job->body = body;
  job->count = count;
  job->seed = seed;
  return job;
}

static const char *variants_job_kind(VariantsJobOp op) {
  return op == VARIANTS_JOB_VARIANTS ? "variants" : op == VARIANTS_JOB_EACH_OP ? "each-op" : "reroll";
}

static void job_failure(UrsatzGuiJobResult *out, int status, const char *message) {
  ursatz_gui_job_result_failure(out, status, message);
}

static void job_ok_with(UrsatzGuiJobResult *out, const char *key, cJSON *value) {
  cJSON *object = cJSON_CreateObject();
  if (!object || !value || !cJSON_AddBoolToObject(object, "ok", 1) || !cJSON_AddItemToObject(object, key, value)) {
    cJSON_Delete(value);
    cJSON_Delete(object);
    job_failure(out, 500, "out of memory");
    return;
  }
  ursatz_gui_job_result_json(out, 200, object);
}

static void job_material_failure(UrsatzGuiJobResult *out, UrsatzGuiMaterialResult result) {
  job_failure(out, material_status(result), material_message(result));
}

/* POST /api/materials/items/<id>/variants {chain?, count?, seed?, functions?, distance?}: candidates from the item's current degrees; nothing is
   saved. The answer is the Composer's document plus "ok" and "seed". */
static int handle_variants(struct mg_connection *conn, const char *id) {
  int status = 0;
  cJSON *body = read_json_body(conn, &status);
  if (!body) {
    return status;
  }
  const cJSON *chain = cJSON_GetObjectItemCaseSensitive(body, "chain");
  const cJSON *count_item = cJSON_GetObjectItemCaseSensitive(body, "count");
  const cJSON *seed_item = cJSON_GetObjectItemCaseSensitive(body, "seed");
  const cJSON *functions = cJSON_GetObjectItemCaseSensitive(body, "functions");
  const cJSON *distance_item = cJSON_GetObjectItemCaseSensitive(body, "distance");
  if (cJSON_IsNull(distance_item)) {
    distance_item = NULL;
  }
  if (cJSON_IsNull(functions)) {
    functions = NULL;
  }
  if (cJSON_IsNull(chain)) {
    chain = NULL;
  }
  if (cJSON_IsNull(count_item)) {
    count_item = NULL;
  }
  if (cJSON_IsNull(seed_item)) {
    seed_item = NULL;
  }
  int count = 6;
  double seed = -1;
  /* The names (which functions, which distance, how many) are the Composer's to refuse, with the path at fault. */
  bool functions_ok = !functions || cJSON_IsArray(functions);
  const cJSON *function = NULL;
  cJSON_ArrayForEach(function, functions) {
    functions_ok = functions_ok && cJSON_IsString(function);
  }
  const char *distance = cJSON_IsString(distance_item) ? distance_item->valuestring : NULL;
  bool distance_ok = !distance_item || distance;
  if (distance_ok && distance && chain) {
    cJSON_Delete(body);
    return send_failure(conn, 400, "distance takes random operations");
  }
  bool valid = functions_ok && distance_ok &&
               (!chain || (cJSON_IsObject(chain) && cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(chain, "steps")))) &&
               (!count_item || (cJSON_IsNumber(count_item) && count_item->valuedouble >= 1 &&
                                count_item->valuedouble <= 16 && count_item->valuedouble == (int)count_item->valuedouble)) &&
               (!seed_item || (cJSON_IsNumber(seed_item) && seed_item->valuedouble >= 0 &&
                               seed_item->valuedouble <= max_variant_seed() &&
                               seed_item->valuedouble == (double)(long long)seed_item->valuedouble));
  if (!valid) {
    cJSON_Delete(body);
    return send_failure(conn, 400, "chain is {steps: [...]}, count is 1 to 16, seed a whole number from 0, distance a name and functions a list of names");
  }
  if (count_item) {
    count = (int)count_item->valuedouble;
  }
  if (seed_item) {
    seed = seed_item->valuedouble;
  }
  VariantsJob *job = variants_job_new(VARIANTS_JOB_VARIANTS, id, body, count, seed);
  if (!job) {
    return send_failure(conn, 500, "out of memory");
  }
  return ursatz_gui_jobs_send_accepted(conn, variants_job_kind(job->op), variants_job_run, job, variants_job_free);
}

/* POST /api/materials/items/<id>/each-op {seed?}: one variant per registry operation on the item's current degrees, in
   registry order, with the operations the motif refuses in `skipped`; nothing is saved. The answer is the Composer's
   document plus "ok" and "seed". */
static int handle_each_op(struct mg_connection *conn, const char *id) {
  int status = 0;
  cJSON *body = read_json_body(conn, &status);
  if (!body) {
    return status;
  }
  const cJSON *seed_item = cJSON_GetObjectItemCaseSensitive(body, "seed");
  if (cJSON_IsNull(seed_item)) {
    seed_item = NULL;
  }
  if (seed_item && (!cJSON_IsNumber(seed_item) || seed_item->valuedouble < 0 || seed_item->valuedouble > max_variant_seed() ||
                    seed_item->valuedouble != (double)(long long)seed_item->valuedouble)) {
    cJSON_Delete(body);
    return send_failure(conn, 400, "seed is a whole number from 0");
  }
  double seed = seed_item ? seed_item->valuedouble : -1;
  VariantsJob *job = variants_job_new(VARIANTS_JOB_EACH_OP, id, body, 0, seed);
  if (!job) {
    return send_failure(conn, 500, "out of memory");
  }
  return ursatz_gui_jobs_send_accepted(conn, variants_job_kind(job->op), variants_job_run, job, variants_job_free);
}

/* GET /api/materials/items/<id>/op-checks: the marks of a motif from Try every operation, {"checks": {...}} (R-190). */
static int handle_op_checks_get(struct mg_connection *conn, const char *id) {
  cJSON *checks = NULL;
  UrsatzGuiMaterialResult result = ursatz_gui_material_op_checks_get(id, &checks);
  if (result != URSATZ_GUI_MATERIAL_OK) {
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
  char message[256] = "";
  cJSON *stored = NULL;
  UrsatzGuiMaterialResult result = ursatz_gui_material_op_checks_put(id, body, message, sizeof(message), &stored);
  cJSON_Delete(body);
  if (result != URSATZ_GUI_MATERIAL_OK) {
    return message[0] != '\0' ? send_failure(conn, material_status(result), message) : send_material_failure(conn, result);
  }
  if (!cJSON_AddBoolToObject(stored, "ok", 1)) {
    cJSON_Delete(stored);
    return send_failure(conn, 500, "out of memory");
  }
  return ursatz_gui_send_object(conn, 200, stored);
}

/* The motif member of the version body a {item, version} reference names, with the body kept in *body. NULL with
   *message set when the reference is malformed, gone, not a motif or has no degrees (not analyzed). */
static const cJSON *motif_by_reference(const cJSON *reference, const char *which, cJSON **body, int *status,
                                       char *message, size_t message_size) {
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(reference, "item");
  const cJSON *version = cJSON_GetObjectItemCaseSensitive(reference, "version");
  if (!cJSON_IsString(item) || !cJSON_IsNumber(version) || version->valuedouble < 1 ||
      version->valuedouble != (int)version->valuedouble) {
    *status = 400;
    snprintf(message, message_size, "%s is {item, version}", which);
    return NULL;
  }
  UrsatzGuiMaterialResult result = ursatz_gui_material_item_get_version(item->valuestring, version->valueint, body);
  if (result != URSATZ_GUI_MATERIAL_OK) {
    *status = material_status(result);
    snprintf(message, message_size, "the %s motif or its version was not found", which);
    return NULL;
  }
  const cJSON *motif = cJSON_GetObjectItemCaseSensitive(*body, "motif");
  const cJSON *degrees = cJSON_GetObjectItemCaseSensitive(motif, "degrees");
  if (!cJSON_IsObject(motif) || !cJSON_IsString(degrees) || degrees->valuestring[0] == '\0') {
    *status = 400;
    snprintf(message, message_size, "the %s is not an analyzed motif", which);
    return NULL;
  }
  return motif;
}

/* POST /api/materials/recognize {source: {item, version}, target: {item, version}}: how the target derives from the
   source, from the Composer's recognize command. Nothing is saved. The answer is the Composer's document plus "ok". */
static int handle_recognize(struct mg_connection *conn) {
  int status = 0;
  cJSON *request = read_json_body(conn, &status);
  if (!request) {
    return status;
  }
  char message[512];
  cJSON *source_body = NULL;
  cJSON *target_body = NULL;
  const cJSON *source = motif_by_reference(cJSON_GetObjectItemCaseSensitive(request, "source"), "source", &source_body,
                                           &status, message, sizeof(message));
  const cJSON *target = source ? motif_by_reference(cJSON_GetObjectItemCaseSensitive(request, "target"), "target",
                                                    &target_body, &status, message, sizeof(message))
                               : NULL;
  cJSON_Delete(request);
  if (!target) {
    cJSON_Delete(source_body);
    return send_failure(conn, status, message);
  }
  bool bad = false;
  const char *form = optional_string(source, "form", &bad);
  const char *meter = optional_string(source, "meter", &bad);
  const char *key = optional_string(source, "key", &bad);
  const char *pickup = optional_string(source, "pickup", &bad);
  const char *target_pickup = optional_string(target, "pickup", &bad);
  const char *source_degrees = optional_string(source, "degrees", &bad);
  const char *target_degrees = optional_string(target, "degrees", &bad);
  const char *target_form = optional_string(target, "form", &bad);
  const char *target_meter = optional_string(target, "meter", &bad);
  const char *target_key = optional_string(target, "key", &bad);
  if (bad || !form || !meter || !source_degrees || !target_degrees ||
      (target_form && strcmp(form, target_form) != 0) || (target_meter && strcmp(meter, target_meter) != 0) ||
      (key ? !target_key || strcmp(key, target_key) != 0 : target_key != NULL)) {
    cJSON_Delete(source_body);
    cJSON_Delete(target_body);
    return send_failure(conn, 400, "the two motifs need the same form, meter and key to be compared");
  }
  bool full = strcmp(form, "full") == 0;
  cJSON *source_request = ursatz_gui_motif_motive_request(full ? "degrees" : NULL, full ? NULL : form, source_degrees, NULL, NULL, NULL);
  cJSON *target_request = ursatz_gui_motif_motive_request(full ? "degrees" : NULL, full ? NULL : form, target_degrees, NULL, NULL, NULL);
  const char *role = ursatz_gui_motif_role_option(optional_string(source, "role", &(bool){false}));
  cJSON *recognize = cJSON_CreateObject();
  bool built = source_request && target_request && recognize &&
               ursatz_gui_motif_add_extras(source_request, ursatz_gui_motif_harmony(source), cJSON_GetObjectItemCaseSensitive(source, "features"),
                                 NULL) &&
               ursatz_gui_motif_add_extras(target_request, ursatz_gui_motif_harmony(target), cJSON_GetObjectItemCaseSensitive(target, "features"),
                                 NULL) &&
               cJSON_AddStringToObject(recognize, "meter", meter) &&
               (!key || key[0] == '\0' || cJSON_AddStringToObject(recognize, "key", key)) &&
               (!role || cJSON_AddStringToObject(recognize, "role", role)) &&
               (!pickup || cJSON_AddStringToObject(recognize, "pickup", pickup)) &&
               (!target_pickup || cJSON_AddStringToObject(recognize, "target_pickup", target_pickup));
  if (built) {
    built = cJSON_AddItemToObject(recognize, "source", source_request);
    source_request = built ? NULL : source_request;
  }
  if (built) {
    built = cJSON_AddItemToObject(recognize, "target", target_request);
    target_request = built ? NULL : target_request;
  }
  cJSON_Delete(source_request);
  cJSON_Delete(target_request);
  cJSON_Delete(source_body);
  cJSON_Delete(target_body);
  UrsatzGuiComposerReply reply;
  bool called = built && ursatz_gui_motif_recognize(recognize, &reply);
  cJSON_Delete(recognize);
  if (!called) {
    return send_failure(conn, 500, "out of memory");
  }
  cJSON *document = reply.status == COMPOSER_OK && reply.json ? cJSON_Parse(reply.json) : NULL;
  if (!document) {
    int failed = send_failure_at(conn, ursatz_gui_composer_reply_failure_status(&reply),
                                 reply.message[0] ? reply.message : "composer could not recognize the motifs", reply.path);
    ursatz_gui_composer_reply_destroy(&reply);
    return failed;
  }
  ursatz_gui_composer_reply_destroy(&reply);
  if (!cJSON_AddBoolToObject(document, "ok", 1)) {
    cJSON_Delete(document);
    return send_failure(conn, 500, "out of memory");
  }
  return ursatz_gui_send_object(conn, 200, document);
}

/* The analysis text of a variant's motive document for the form: notes_text (degrees_text when degrees is set),
   rhythm_text or contour_text. */
static const char *variant_text(const cJSON *variant, const char *form, bool degrees) {
  const cJSON *analysis = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(variant, "motive"), "analysis");
  const char *name = strcmp(form, "rhythm") == 0 ? "rhythm_text" : strcmp(form, "contour") == 0 ? "contour_text" : degrees ? "degrees_text" : "notes_text";
  const cJSON *text = cJSON_GetObjectItemCaseSensitive(analysis, name);
  return cJSON_IsString(text) && text->valuestring[0] != '\0' ? text->valuestring : NULL;
}

/* Adds a duplicate of the member of from named name to object (an empty array when from has none of that type). */
static bool copy_member(cJSON *object, const cJSON *from, const char *name, bool as_array) {
  const cJSON *value = cJSON_GetObjectItemCaseSensitive(from, name);
  cJSON *copy = (as_array ? cJSON_IsArray(value) : value != NULL) ? cJSON_Duplicate(value, 1) : cJSON_CreateArray();
  return copy && cJSON_AddItemToObject(object, name, copy);
}

static bool apply_variant_extras(cJSON *derived, const cJSON *variant);
static bool set_variant_motif_members(cJSON *motif, const cJSON *variant);

/* The derivation record of a stored variant: the chain steps, the seed, the kind and the dimensions kept and changed. */
static cJSON *derived_record(const char *source_id, int source_version, const cJSON *variant, const char *flag) {
  const cJSON *chain = cJSON_GetObjectItemCaseSensitive(variant, "chain");
  const cJSON *seed = cJSON_GetObjectItemCaseSensitive(variant, "seed");
  const cJSON *kind = cJSON_GetObjectItemCaseSensitive(variant, "kind");
  cJSON *derived = cJSON_CreateObject();
  cJSON *steps = cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(chain, "steps"), 1);
  bool ok = derived && steps && cJSON_AddStringToObject(derived, "source_item", source_id) &&
            cJSON_AddNumberToObject(derived, "source_version", source_version) &&
            cJSON_AddItemToObject(derived, "operations", steps) &&
            cJSON_AddNumberToObject(derived, "seed", cJSON_IsNumber(seed) ? seed->valuedouble : 0) &&
            cJSON_AddStringToObject(derived, "kind", cJSON_IsString(kind) ? kind->valuestring : "motive-form") &&
            copy_member(derived, variant, "kept", true) && copy_member(derived, variant, "changed", true) &&
            apply_variant_extras(derived, variant) && cJSON_AddStringToObject(derived, "flag", flag);
  if (!ok) {
    if (derived && !cJSON_GetObjectItemCaseSensitive(derived, "operations")) {
      cJSON_Delete(steps);
    }
    cJSON_Delete(derived);
    return NULL;
  }
  return derived;
}

/* Writes prefix and text into out (501 bytes), cut with "..." on a character boundary when over 500 bytes. */
static void bounded_note(char *out, size_t size, const char *prefix, const char *text) {
  snprintf(out, size, "%s%s", prefix, text);
  if (strlen(prefix) + strlen(text) > size - 1) {
    size_t cut = size - 4;
    while (cut > 0 && ((unsigned char)out[cut] & 0xC0) == 0x80) {
      cut--;
    }
    snprintf(out + cut, size - cut, "...");
  }
}

/* True when the variant document carries what a derived motif is made from. */
static bool variant_ok(const cJSON *variant) {
  const cJSON *chain = cJSON_GetObjectItemCaseSensitive(variant, "chain");
  return cJSON_IsObject(variant) && cJSON_IsObject(chain) &&
         cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(chain, "steps")) &&
         cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(variant, "seed")) &&
         cJSON_IsString(cJSON_GetObjectItemCaseSensitive(variant, "kind")) &&
         cJSON_IsString(cJSON_GetObjectItemCaseSensitive(variant, "derivation")) &&
         cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(variant, "motive"), "analysis"));
}

static bool flag_ok(const char *flag) {
  return flag && (strcmp(flag, "approved") == 0 || strcmp(flag, "rejected") == 0 || strcmp(flag, "unflagged") == 0);
}

/* "<source name> <op>[ + <op>]", the source name cut so the whole stays within 200 bytes. */
static void variant_default_name(const char *source_name, const cJSON *steps, char *out, size_t size) {
  char ops[160] = "";
  size_t used = 0;
  const cJSON *step = NULL;
  cJSON_ArrayForEach(step, steps) {
    const cJSON *op = cJSON_GetObjectItemCaseSensitive(step, "op");
    if (cJSON_IsString(op)) {
      used += (size_t)snprintf(ops + used, sizeof(ops) - used, "%s%s", used ? " + " : "", op->valuestring);
      if (used >= sizeof(ops)) {
        used = sizeof(ops) - 1;
        break;
      }
    }
  }
  size_t limit = size - 1 > 200 ? 200 : size - 1;
  size_t keep = strlen(source_name);
  size_t room = limit > used + 1 ? limit - used - 1 : 0;
  if (keep > room) {
    keep = room;
    while (keep > 0 && ((unsigned char)source_name[keep] & 0xC0) == 0x80) {
      keep--;
    }
  }
  snprintf(out, size, "%.*s %s", (int)keep, source_name, ops);
}

/* POST /api/materials/items/<id>/variants/save {variant, name?, flag}: a derived motif made from one candidate. */
static int handle_variant_save(struct mg_connection *conn, const char *id) {
  int status = 0;
  cJSON *body = read_json_body(conn, &status);
  if (!body) {
    return status;
  }
  bool bad = false;
  const cJSON *variant = cJSON_GetObjectItemCaseSensitive(body, "variant");
  const char *name = optional_string(body, "name", &bad);
  const char *flag = optional_string(body, "flag", &bad);
  if (!flag) {
    flag = "unflagged";
  }
  if (bad || !variant_ok(variant) || !flag_ok(flag)) {
    cJSON_Delete(body);
    return send_failure(conn, 400, "variant must be a candidate from the variants route; flag is approved, rejected or unflagged");
  }
  cJSON *meta = NULL;
  cJSON *source = NULL;
  UrsatzGuiMaterialResult result = ursatz_gui_material_item_get(id, &meta, &source);
  if (result != URSATZ_GUI_MATERIAL_OK) {
    cJSON_Delete(body);
    return send_material_failure(conn, result);
  }
  const cJSON *motif = cJSON_GetObjectItemCaseSensitive(source, "motif");
  const char *form = optional_string(motif, "form", &(bool){false});
  const char *text = form ? variant_text(variant, form, false) : NULL;
  const cJSON *version = cJSON_GetObjectItemCaseSensitive(meta, "current_version");
  if (!cJSON_IsObject(motif) || !form || !text || !variant_text(variant, form, true) || !cJSON_IsNumber(version)) {
    cJSON_Delete(body);
    cJSON_Delete(meta);
    cJSON_Delete(source);
    return send_failure(conn, 400, "the variant does not match this motif");
  }
  const char *source_name = optional_string(meta, "name", &(bool){false});
  const char *project_id = optional_string(meta, "project_id", &(bool){false});
  char default_name[256];
  variant_default_name(source_name ? source_name : "", cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(variant, "chain"), "steps"),
                       default_name, sizeof(default_name));
  const cJSON *derivation = cJSON_GetObjectItemCaseSensitive(variant, "derivation");
  char note[501];
  bounded_note(note, sizeof(note), "", derivation->valuestring);

  cJSON *new_body = cJSON_CreateObject();
  cJSON *new_motif = cJSON_CreateObject();
  cJSON *derived = derived_record(id, version->valueint, variant, flag);
  const cJSON *role = cJSON_GetObjectItemCaseSensitive(motif, "role");
  const cJSON *key = cJSON_GetObjectItemCaseSensitive(motif, "key");
  const cJSON *meter = cJSON_GetObjectItemCaseSensitive(motif, "meter");
  const cJSON *pickup = cJSON_GetObjectItemCaseSensitive(motif, "pickup");
  cJSON *analysis = cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(variant, "motive"), 1);
  bool ok = new_body && new_motif && derived && analysis && cJSON_IsString(role) && cJSON_IsString(meter) &&
            cJSON_IsString(pickup) && cJSON_AddArrayToObject(new_body, "uses") &&
            cJSON_AddStringToObject(new_motif, "role", role->valuestring) &&
            cJSON_AddStringToObject(new_motif, "form", form) && cJSON_AddStringToObject(new_motif, "notes", text) &&
            cJSON_AddStringToObject(new_motif, "meter", meter->valuestring) &&
            (cJSON_IsString(key) ? cJSON_AddStringToObject(new_motif, "key", key->valuestring) != NULL
                                 : cJSON_AddNullToObject(new_motif, "key") != NULL) &&
            cJSON_AddStringToObject(new_motif, "pickup", pickup->valuestring) &&
            set_variant_motif_members(new_motif, variant) && cJSON_AddItemToObject(new_motif, "analysis", analysis) &&
            cJSON_AddStringToObject(new_motif, "degrees", variant_text(variant, form, true)) &&
            cJSON_AddItemToObject(new_motif, "derived", derived) && cJSON_AddItemToObject(new_body, "motif", new_motif);
  if (!ok) {
    /* Whatever was not attached to a parent is still ours. */
    if (analysis && !cJSON_GetObjectItemCaseSensitive(new_motif, "analysis")) {
      cJSON_Delete(analysis);
    }
    if (derived && new_motif && !cJSON_GetObjectItemCaseSensitive(new_motif, "derived")) {
      cJSON_Delete(derived);
    }
    if (new_motif && new_body && !cJSON_GetObjectItemCaseSensitive(new_body, "motif")) {
      cJSON_Delete(new_motif);
    }
    cJSON_Delete(new_body);
    cJSON_Delete(body);
    cJSON_Delete(meta);
    cJSON_Delete(source);
    return send_failure(conn, 500, "out of memory");
  }
  cJSON *item = NULL;
  result = ursatz_gui_material_item_create_noted("motif", name && name[0] != '\0' ? name : default_name,
                                                 project_id ? project_id : "", new_body, note, &item);
  cJSON_Delete(new_body);
  cJSON_Delete(body);
  cJSON_Delete(meta);
  cJSON_Delete(source);
  if (result != URSATZ_GUI_MATERIAL_OK) {
    return send_material_failure(conn, result);
  }
  return send_ok_with(conn, "item", item);
}

static bool kind_ok(const char *kind) {
  return kind && (strcmp(kind, "exact") == 0 || strcmp(kind, "motive-form") == 0 || strcmp(kind, "variant") == 0);
}

/* The derived object of a motif version body, NULL when it has none. */
static cJSON *derived_of(cJSON *version_body) {
  cJSON *derived = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(version_body, "motif"), "derived");
  return cJSON_IsObject(derived) ? derived : NULL;
}

/* Replaces a string member of object. */
static bool set_string(cJSON *object, const char *name, const char *value) {
  cJSON_DeleteItemFromObjectCaseSensitive(object, name);
  return cJSON_AddStringToObject(object, name, value) != NULL;
}

/* POST /api/materials/items/<id>/flag {flag, kind?}: a new version that changes only derived.flag (and kind). */
static int handle_flag(struct mg_connection *conn, const char *id) {
  int status = 0;
  cJSON *body = read_json_body(conn, &status);
  if (!body) {
    return status;
  }
  bool bad = false;
  const char *flag = optional_string(body, "flag", &bad);
  const char *kind = optional_string(body, "kind", &bad);
  if (bad || !flag_ok(flag) || (kind && !kind_ok(kind))) {
    cJSON_Delete(body);
    return send_failure(conn, 400, "flag is approved, rejected or unflagged; kind is exact, motive-form or variant");
  }
  cJSON *current = NULL;
  UrsatzGuiMaterialResult result = ursatz_gui_material_item_get(id, NULL, &current);
  if (result != URSATZ_GUI_MATERIAL_OK) {
    cJSON_Delete(body);
    return send_material_failure(conn, result);
  }
  cJSON *derived = derived_of(current);
  if (!derived) {
    cJSON_Delete(body);
    cJSON_Delete(current);
    return send_failure(conn, 400, "only a derived motif has a flag");
  }
  if (!set_string(derived, "flag", flag) || (kind && !set_string(derived, "kind", kind))) {
    cJSON_Delete(body);
    cJSON_Delete(current);
    return send_failure(conn, 500, "out of memory");
  }
  cJSON *item = NULL;
  result = ursatz_gui_material_item_add_version(id, current, flag, &item);
  cJSON_Delete(body);
  cJSON_Delete(current);
  if (result != URSATZ_GUI_MATERIAL_OK) {
    return send_material_failure(conn, result);
  }
  return send_ok_with(conn, "item", item);
}

/* POST /api/materials/items/<id>/promote {name, project_id, note?}: a variant becomes a motif of its own. A new version
   with derived.standalone true (R-192), the item renamed and moved. The id, versions and derived record stay. */
static int handle_promote(struct mg_connection *conn, const char *id) {
  int status = 0;
  cJSON *body = read_json_body(conn, &status);
  if (!body) {
    return status;
  }
  bool bad = false;
  const char *name = optional_string(body, "name", &bad);
  const char *project_id = optional_string(body, "project_id", &bad);
  const char *note = optional_string(body, "note", &bad);
  if (bad || !name || !project_id) {
    cJSON_Delete(body);
    return send_failure(conn, 400, "promote needs a name and a project_id; note is a string");
  }
  cJSON *current = NULL;
  UrsatzGuiMaterialResult result = ursatz_gui_material_item_get(id, NULL, &current);
  if (result != URSATZ_GUI_MATERIAL_OK) {
    cJSON_Delete(body);
    return send_material_failure(conn, result);
  }
  cJSON *derived = derived_of(current);
  if (!derived || !ursatz_gui_material_item_is_variant(current)) {
    cJSON_Delete(body);
    cJSON_Delete(current);
    return send_failure(conn, 400, "only a variant can be saved as a new motif");
  }
  /* The project is checked up front, so a bad name cannot leave the item moved. */
  cJSON *project = NULL;
  result = ursatz_gui_material_project_get(project_id, &project);
  cJSON_Delete(project);
  cJSON *item = NULL;
  if (result == URSATZ_GUI_MATERIAL_OK) {
    result = ursatz_gui_material_item_rename(id, name, &item);
    cJSON_Delete(item);
    item = NULL;
  }
  if (result == URSATZ_GUI_MATERIAL_OK) {
    result = ursatz_gui_material_item_move(id, project_id, &item);
    cJSON_Delete(item);
    item = NULL;
  }
  if (result == URSATZ_GUI_MATERIAL_OK) {
    cJSON_DeleteItemFromObjectCaseSensitive(derived, "standalone");
    result = cJSON_AddTrueToObject(derived, "standalone")
                 ? ursatz_gui_material_item_add_version(id, current, note && *note ? note : "Saved as new motif", &item)
                 : URSATZ_GUI_MATERIAL_ERR_ALLOC;
  }
  cJSON_Delete(body);
  cJSON_Delete(current);
  if (result != URSATZ_GUI_MATERIAL_OK) {
    cJSON_Delete(item);
    return send_material_failure(conn, result);
  }
  return send_ok_with(conn, "item", item);
}

/* A chain for the Composer from stored operations: the steps with their params, less the ones the seed drew (`open`),
   so a rerun draws them again. NULL when out of memory. */
static cJSON *chain_for_reroll(const cJSON *operations) {
  cJSON *chain = cJSON_CreateObject();
  cJSON *steps = chain ? cJSON_AddArrayToObject(chain, "steps") : NULL;
  const cJSON *step = NULL;
  cJSON_ArrayForEach(step, operations) {
    const cJSON *op = cJSON_GetObjectItemCaseSensitive(step, "op");
    const cJSON *params = cJSON_GetObjectItemCaseSensitive(step, "params");
    const cJSON *open = cJSON_GetObjectItemCaseSensitive(step, "open");
    cJSON *entry = cJSON_CreateObject();
    cJSON *kept = cJSON_CreateObject();
    if (!steps || !entry || !kept || !cJSON_IsString(op) || !cJSON_AddStringToObject(entry, "op", op->valuestring)) {
      cJSON_Delete(entry);
      cJSON_Delete(kept);
      cJSON_Delete(chain);
      return NULL;
    }
    const cJSON *param = NULL;
    cJSON_ArrayForEach(param, params) {
      bool drawn = false;
      const cJSON *name = NULL;
      cJSON_ArrayForEach(name, open) {
        drawn = drawn || (cJSON_IsString(name) && param->string && strcmp(name->valuestring, param->string) == 0);
      }
      if (!drawn && !cJSON_AddItemToObject(kept, param->string, cJSON_Duplicate(param, 1))) {
        cJSON_Delete(entry);
        cJSON_Delete(kept);
        cJSON_Delete(chain);
        return NULL;
      }
    }
    if (!cJSON_AddItemToObject(entry, "params", kept)) {
      cJSON_Delete(entry);
      cJSON_Delete(kept);
      cJSON_Delete(chain);
      return NULL;
    }
    cJSON_AddItemToArray(steps, entry);
  }
  return chain;
}

/* Replaces the member name of object with value (moved in). */
static bool replace_member(cJSON *object, const char *name, cJSON *value) {
  cJSON_DeleteItemFromObjectCaseSensitive(object, name);
  if (value && cJSON_AddItemToObject(object, name, value)) {
    return true;
  }
  cJSON_Delete(value);
  return false;
}

/* Sets motif.harmony (the variant's own chords as text, or null) and motif.features (the labelled runs it kept). */
static bool set_variant_motif_members(cJSON *motif, const cJSON *variant) {
  const cJSON *harmony = cJSON_GetObjectItemCaseSensitive(variant, "harmony");
  const cJSON *features = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(variant, "motive"), "motive"), "features");
  cJSON *kept = cJSON_CreateArray();
  const cJSON *feature = NULL;
  cJSON_ArrayForEach(feature, features) {
    const cJSON *label = cJSON_GetObjectItemCaseSensitive(feature, "label");
    const cJSON *first = cJSON_GetObjectItemCaseSensitive(feature, "first");
    const cJSON *last = cJSON_GetObjectItemCaseSensitive(feature, "last");
    cJSON *entry = cJSON_IsString(label) && cJSON_IsNumber(first) && cJSON_IsNumber(last) ? cJSON_CreateObject() : NULL;
    if (entry && cJSON_AddStringToObject(entry, "label", label->valuestring) &&
        cJSON_AddNumberToObject(entry, "first", first->valuedouble) && cJSON_AddNumberToObject(entry, "last", last->valuedouble)) {
      cJSON_AddItemToArray(kept, entry);
    } else {
      cJSON_Delete(entry);
    }
  }
  return replace_member(motif, "harmony",
                        cJSON_IsString(harmony) && harmony->valuestring[0] != '\0' ? cJSON_CreateString(harmony->valuestring)
                                                                                    : cJSON_CreateNull()) &&
         replace_member(motif, "features", kept);
}

/* Sets derived.harmony (the variant's own chords as text, or null), derived.base_harmony (its base harmony as text, or
   null), derived.tones and derived.warnings (from its analysis), derived.functions (the names it suits) and
   derived.remoteness (its distance from the basic motive, 0 to 1, or null) from a candidate. */
static bool apply_variant_extras(cJSON *derived, const cJSON *variant) {
  const cJSON *harmony = cJSON_GetObjectItemCaseSensitive(variant, "harmony");
  const cJSON *remoteness = cJSON_GetObjectItemCaseSensitive(variant, "remoteness");
  cJSON *functions = cJSON_CreateArray();
  const cJSON *name = NULL;
  cJSON_ArrayForEach(name, cJSON_GetObjectItemCaseSensitive(variant, "functions")) {
    if (cJSON_IsString(name) && function_name_ok(name->valuestring)) {
      cJSON_AddItemToArray(functions, cJSON_CreateString(name->valuestring));
    }
  }
  const cJSON *analysis = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(variant, "motive"), "analysis");
  const cJSON *base = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(analysis, "base_harmony"), "text");
  const cJSON *tones = cJSON_GetObjectItemCaseSensitive(analysis, "tones");
  const cJSON *warnings = cJSON_GetObjectItemCaseSensitive(analysis, "warnings");
  return replace_member(derived, "harmony",
                        cJSON_IsString(harmony) && harmony->valuestring[0] != '\0' ? cJSON_CreateString(harmony->valuestring)
                                                                                    : cJSON_CreateNull()) &&
         replace_member(derived, "base_harmony",
                        cJSON_IsString(base) && base->valuestring[0] != '\0' ? cJSON_CreateString(base->valuestring)
                                                                              : cJSON_CreateNull()) &&
         replace_member(derived, "tones", cJSON_IsArray(tones) ? cJSON_Duplicate(tones, 1) : cJSON_CreateArray()) &&
         replace_member(derived, "warnings", cJSON_IsArray(warnings) ? cJSON_Duplicate(warnings, 1) : cJSON_CreateArray()) &&
         replace_member(derived, "functions", functions) &&
         replace_member(derived, "remoteness",
                        cJSON_IsNumber(remoteness) && remoteness->valuedouble >= 0 && remoteness->valuedouble <= 1
                            ? cJSON_CreateNumber(remoteness->valuedouble)
                            : cJSON_CreateNull());
}

/* True when every step of the chain is the "exact" step the Composer names a motive that equals its source. */
static bool steps_are_exact(const cJSON *steps) {
  const cJSON *step = NULL;
  cJSON_ArrayForEach(step, steps) {
    const cJSON *op = cJSON_GetObjectItemCaseSensitive(step, "op");
    if (!cJSON_IsString(op) || strcmp(op->valuestring, "exact") != 0) {
      return false;
    }
  }
  return true;
}

/* True when id is source_id or one of the items above it, following derived.source_item at most 32 hops. */
static bool item_is_in_lineage(const char *id, const char *source_id) {
  char current[128];
  snprintf(current, sizeof(current), "%s", source_id);
  for (int hops = 0; hops <= 32; hops++) {
    if (strcmp(current, id) == 0) {
      return true;
    }
    cJSON *body = NULL;
    if (ursatz_gui_material_item_get(current, NULL, &body) != URSATZ_GUI_MATERIAL_OK) {
      return false;
    }
    const cJSON *next = cJSON_GetObjectItemCaseSensitive(derived_of(body), "source_item");
    bool more = cJSON_IsString(next) && strlen(next->valuestring) < sizeof(current);
    if (more) {
      snprintf(current, sizeof(current), "%s", next->valuestring);
    }
    cJSON_Delete(body);
    if (!more) {
      return false;
    }
  }
  return true;
}

/* The derivation record a recognition describes for the target: the source and its version, the chain, no seed, the
   target's own chords, and the dimensions kept and changed. NULL when the recognition lacks them or on no memory. */
static cJSON *derived_from_recognition(const char *source_id, int source_version, const cJSON *recognition,
                                       const cJSON *target_motif) {
  const cJSON *steps = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(recognition, "chain"), "steps");
  const cJSON *kept = cJSON_GetObjectItemCaseSensitive(recognition, "kept");
  const cJSON *changed = cJSON_GetObjectItemCaseSensitive(recognition, "changed");
  const cJSON *remoteness = cJSON_GetObjectItemCaseSensitive(recognition, "remoteness");
  if (!cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(recognition, "found")) || !cJSON_IsArray(steps) ||
      !cJSON_IsArray(kept) || !cJSON_IsArray(changed)) {
    return NULL;
  }
  const char *harmony = ursatz_gui_motif_harmony(target_motif);
  cJSON *derived = cJSON_CreateObject();
  cJSON *functions = cJSON_CreateArray();
  const cJSON *name = NULL;
  cJSON_ArrayForEach(name, cJSON_GetObjectItemCaseSensitive(recognition, "functions")) {
    if (cJSON_IsString(name) && function_name_ok(name->valuestring)) {
      cJSON_AddItemToArray(functions, cJSON_CreateString(name->valuestring));
    }
  }
  bool ok = derived && functions && cJSON_AddStringToObject(derived, "source_item", source_id) &&
            cJSON_AddNumberToObject(derived, "source_version", source_version) &&
            cJSON_AddItemToObject(derived, "operations", cJSON_Duplicate(steps, 1)) &&
            cJSON_AddNullToObject(derived, "seed") &&
            cJSON_AddStringToObject(derived, "kind", steps_are_exact(steps) ? "exact" : "motive-form") &&
            cJSON_AddItemToObject(derived, "kept", cJSON_Duplicate(kept, 1)) &&
            cJSON_AddItemToObject(derived, "changed", cJSON_Duplicate(changed, 1)) &&
            (harmony ? cJSON_AddStringToObject(derived, "harmony", harmony) : cJSON_AddNullToObject(derived, "harmony")) &&
            cJSON_AddItemToObject(derived, "functions", functions) &&
            (cJSON_IsNumber(remoteness) ? cJSON_AddNumberToObject(derived, "remoteness", remoteness->valuedouble)
                                        : cJSON_AddNullToObject(derived, "remoteness")) &&
            cJSON_AddStringToObject(derived, "flag", "unflagged");
  if (!ok) {
    if (derived && !cJSON_GetObjectItemCaseSensitive(derived, "functions")) {
      cJSON_Delete(functions);
    }
    cJSON_Delete(derived);
    return NULL;
  }
  return derived;
}

/* POST /api/materials/items/<id>/link-form {source: {item, version}, recognition}: a new version of the item (the
   target) whose derived record says it is a form of the source, from a recognition the recognize route gave. 409 when
   the item is the source or above it in the source's lineage, since the tree would then loop. */
static int handle_link_form(struct mg_connection *conn, const char *id) {
  int status = 0;
  cJSON *body = read_json_body(conn, &status);
  if (!body) {
    return status;
  }
  const cJSON *reference = cJSON_GetObjectItemCaseSensitive(body, "source");
  const cJSON *source_item = cJSON_GetObjectItemCaseSensitive(reference, "item");
  const cJSON *source_version = cJSON_GetObjectItemCaseSensitive(reference, "version");
  const cJSON *recognition = cJSON_GetObjectItemCaseSensitive(body, "recognition");
  if (!cJSON_IsString(source_item) || !cJSON_IsNumber(source_version) || source_version->valuedouble < 1 ||
      source_version->valuedouble != (int)source_version->valuedouble || !cJSON_IsObject(recognition) ||
      !cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(recognition, "found"))) {
    cJSON_Delete(body);
    return send_failure(conn, 400, "link-form needs a source {item, version} and a recognition that found a chain");
  }
  cJSON *current = NULL;
  UrsatzGuiMaterialResult result = ursatz_gui_material_item_get(id, NULL, &current);
  cJSON *source = NULL;
  if (result == URSATZ_GUI_MATERIAL_OK) {
    result = ursatz_gui_material_item_get_version(source_item->valuestring, source_version->valueint, &source);
  }
  if (result != URSATZ_GUI_MATERIAL_OK) {
    cJSON_Delete(body);
    cJSON_Delete(current);
    return send_material_failure(conn, result);
  }
  cJSON *motif = cJSON_GetObjectItemCaseSensitive(current, "motif");
  if (!cJSON_IsObject(motif) || !cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(source, "motif"))) {
    cJSON_Delete(body);
    cJSON_Delete(current);
    cJSON_Delete(source);
    return send_failure(conn, 400, "only a motif can be linked as a form of a motif");
  }
  cJSON_Delete(source);
  if (item_is_in_lineage(id, source_item->valuestring)) {
    cJSON_Delete(body);
    cJSON_Delete(current);
    return send_failure(conn, 409, "a motif cannot be a form of itself or of one of its own forms");
  }
  cJSON *derived = derived_from_recognition(source_item->valuestring, source_version->valueint, recognition, motif);
  const cJSON *derivation = cJSON_GetObjectItemCaseSensitive(recognition, "derivation");
  char note[501];
  bounded_note(note, sizeof(note), "linked: ", cJSON_IsString(derivation) ? derivation->valuestring : "form of another motif");
  if (!derived) {
    cJSON_Delete(body);
    cJSON_Delete(current);
    return send_failure(conn, 400, "the recognition lacks a chain, kept and changed");
  }
  cJSON *item = NULL;
  /* A linked motif already exists on its own, so it is a standalone motif (R-192), not a variant of the source. */
  bool linked = cJSON_AddTrueToObject(derived, "standalone") != NULL;
  result = linked && replace_member(motif, "derived", derived) ? ursatz_gui_material_item_add_version(id, current, note, &item)
                                                              : URSATZ_GUI_MATERIAL_ERR_ALLOC;
  if (!linked) {
    cJSON_Delete(derived);
  }
  cJSON_Delete(body);
  cJSON_Delete(current);
  if (result != URSATZ_GUI_MATERIAL_OK) {
    return send_material_failure(conn, result);
  }
  return send_ok_with(conn, "item", item);
}

/* Reruns the stored chain of the derived motif id on the source's stored version with a new seed and stores the result
   as a new version (the reroll job). */
static void reroll_run(const char *id, const ComposerJob *composer_job, UrsatzGuiJobResult *out) {
  cJSON *current = NULL;
  UrsatzGuiMaterialResult result = ursatz_gui_material_item_get(id, NULL, &current);
  if (result != URSATZ_GUI_MATERIAL_OK) {
    {
    job_material_failure(out, result);
    return;
  }
  }
  cJSON *motif = cJSON_GetObjectItemCaseSensitive(current, "motif");
  cJSON *derived = derived_of(current);
  const cJSON *source_item = cJSON_GetObjectItemCaseSensitive(derived, "source_item");
  const cJSON *source_version = cJSON_GetObjectItemCaseSensitive(derived, "source_version");
  const cJSON *stored_seed = cJSON_GetObjectItemCaseSensitive(derived, "seed");
  const cJSON *form_item = cJSON_GetObjectItemCaseSensitive(motif, "form");
  if (!derived || !cJSON_IsString(source_item) || !cJSON_IsNumber(source_version) || !cJSON_IsString(form_item)) {
    cJSON_Delete(current);
    job_failure(out, 400, "only a derived motif can be rerolled");
    return;
  }
  const char *form = form_item->valuestring;
  cJSON *source = NULL;
  result = ursatz_gui_material_item_get_version(source_item->valuestring, source_version->valueint, &source);
  if (result != URSATZ_GUI_MATERIAL_OK) {
    cJSON_Delete(current);
    job_failure(out, 404, "the source motif or its version is gone");
    return;
  }
  cJSON *chain = chain_for_reroll(cJSON_GetObjectItemCaseSensitive(derived, "operations"));
  cJSON *basic_body = lineage_root(cJSON_GetObjectItemCaseSensitive(source, "motif"));
  const cJSON *shown = cJSON_GetObjectItemCaseSensitive(motif, "degrees");
  int status = 500;
  char message[512] = "out of memory";
  cJSON *document = NULL;
  const cJSON *variant = NULL;
  double base = cJSON_IsNumber(stored_seed) ? stored_seed->valuedouble : 0;
  double seed = 0;
  for (int attempt = 0; chain && attempt < 16; attempt++) {
    seed = fmod(base + 1000003.0 + attempt, max_variant_seed() + 1);
    cJSON_Delete(document);
    document = ursatz_gui_motif_run_variants(cJSON_GetObjectItemCaseSensitive(source, "motif"), basic_body, NULL, NULL, false, chain, 1, seed, composer_job,
                            &status, message, sizeof(message));
    if (!document) {
      break;
    }
    variant = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(document, "variants"), 0);
    const char *degrees = variant_ok(variant) ? variant_text(variant, form, true) : NULL;
    if (!degrees) {
      status = 500;
      snprintf(message, sizeof(message), "composer returned no variant");
      cJSON_Delete(document);
      document = NULL;
      break;
    }
    if (!cJSON_IsString(shown) || strcmp(degrees, shown->valuestring) != 0) {
      break;
    }
    variant = NULL;
  }
  cJSON_Delete(source);
  cJSON_Delete(chain);
  cJSON_Delete(basic_body);
  if (!document || !variant) {
    bool same = document != NULL;
    cJSON_Delete(document);
    cJSON_Delete(current);
    ursatz_gui_job_result_failure_at(out, same ? 409 : status, same ? "rerolling gives the same motif" : message,
                                     same ? NULL : ursatz_gui_motif_variants_path());
    return;
  }
  const cJSON *chain_out = cJSON_GetObjectItemCaseSensitive(variant, "chain");
  const cJSON *kind = cJSON_GetObjectItemCaseSensitive(variant, "kind");
  const cJSON *derivation = cJSON_GetObjectItemCaseSensitive(variant, "derivation");
  const char *notes = variant_text(variant, form, false);
  bool ok = notes && cJSON_IsString(kind) && cJSON_IsString(derivation) &&
            replace_member(motif, "notes", cJSON_CreateString(notes)) &&
            replace_member(motif, "degrees", cJSON_CreateString(variant_text(variant, form, true))) &&
            replace_member(motif, "analysis", cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(variant, "motive"), 1)) &&
            replace_member(derived, "operations", cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(chain_out, "steps"), 1)) &&
            replace_member(derived, "seed", cJSON_CreateNumber(seed)) &&
            replace_member(derived, "kind", cJSON_CreateString(kind->valuestring)) &&
            replace_member(derived, "kept", cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(variant, "kept"), 1)) &&
            replace_member(derived, "changed", cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(variant, "changed"), 1)) &&
            apply_variant_extras(derived, variant) && set_variant_motif_members(motif, variant) &&
            set_string(derived, "flag", "unflagged");
  char note[501];
  if (ok) {
    bounded_note(note, sizeof(note), "rerolled: ", derivation->valuestring);
  }
  cJSON *item = NULL;
  result = ok ? ursatz_gui_material_item_add_version(id, current, note, &item) : URSATZ_GUI_MATERIAL_ERR_ALLOC;
  cJSON_Delete(document);
  cJSON_Delete(current);
  if (result != URSATZ_GUI_MATERIAL_OK) {
    {
    job_material_failure(out, result);
    return;
  }
  }
  job_ok_with(out, "item", item);
}

/* POST /api/materials/items/<id>/reroll: reruns the stored chain on the source's stored version with a new seed and
   stores the result as a new version. Runs on the job queue. */
static int handle_reroll(struct mg_connection *conn, const char *id) {
  VariantsJob *job = variants_job_new(VARIANTS_JOB_REROLL, id, NULL, 0, -1);
  if (!job) {
    return send_failure(conn, 500, "out of memory");
  }
  return ursatz_gui_jobs_send_accepted(conn, variants_job_kind(job->op), variants_job_run, job, variants_job_free);
}

/* The variants and each-op runs of a job: the answer is the Composer's document plus "ok" and "seed". */
static void variants_run(const VariantsJob *input, const ComposerJob *composer_job, UrsatzGuiJobResult *out) {
  bool each_op = input->op == VARIANTS_JOB_EACH_OP;
  const cJSON *chain = cJSON_GetObjectItemCaseSensitive(input->body, "chain");
  const cJSON *functions = cJSON_GetObjectItemCaseSensitive(input->body, "functions");
  const cJSON *distance_item = cJSON_GetObjectItemCaseSensitive(input->body, "distance");
  chain = cJSON_IsNull(chain) ? NULL : chain;
  functions = cJSON_IsNull(functions) ? NULL : functions;
  const char *distance = cJSON_IsString(distance_item) ? distance_item->valuestring : NULL;
  cJSON *item_body = NULL;
  UrsatzGuiMaterialResult result = ursatz_gui_material_item_get(input->id, NULL, &item_body);
  if (result != URSATZ_GUI_MATERIAL_OK) {
    job_material_failure(out, result);
    return;
  }
  const cJSON *motif = cJSON_GetObjectItemCaseSensitive(item_body, "motif");
  int status = 400;
  char message[512] = "only a motif has variants";
  cJSON *basic_body = cJSON_IsObject(motif) ? lineage_root(motif) : NULL;
  cJSON *document = cJSON_IsObject(motif)
                        ? (each_op ? ursatz_gui_motif_run_variants(motif, basic_body, NULL, NULL, true, NULL, 0, input->seed, composer_job, &status,
                                                  message, sizeof(message))
                                   : ursatz_gui_motif_run_variants(motif, basic_body, functions, distance, false, chain, input->count, input->seed,
                                                  composer_job, &status, message, sizeof(message)))
                        : NULL;
  cJSON_Delete(basic_body);
  cJSON_Delete(item_body);
  if (!document) {
    ursatz_gui_job_result_failure_at(out, status, message, ursatz_gui_motif_variants_path());
    return;
  }
  const cJSON *run_seed = cJSON_GetObjectItemCaseSensitive(document, "seed");
  if (!cJSON_AddBoolToObject(document, "ok", 1) ||
      (!run_seed && !cJSON_AddNumberToObject(document, "seed", input->seed >= 0 ? input->seed : 0))) {
    cJSON_Delete(document);
    job_failure(out, 500, "out of memory");
    return;
  }
  ursatz_gui_job_result_json(out, 200, document);
}

static void variants_job_run(void *input, const ComposerJob *composer_job, UrsatzGuiJobResult *out) {
  const VariantsJob *job = input;
  if (job->op == VARIANTS_JOB_REROLL) {
    reroll_run(job->id, composer_job, out);
  } else {
    variants_run(job, composer_job, out);
  }
}

static const int kMaxTreeDepth = 32;

/* A tree node {id, name, current_version, standalone, derived, children: []} (derived is a copy, or null; standalone
   is true for a promoted motif) with every entry of index
   whose derived.source_item is id below it. ancestors lists the ids above, so a loop in the stored records ends. */
static cJSON *variant_node(const cJSON *index, const char *id, const char *name, int version, const cJSON *derived,
                           const char **ancestors, int depth) {
  cJSON *node = cJSON_CreateObject();
  cJSON *children = cJSON_CreateArray();
  cJSON *copy = derived ? cJSON_Duplicate(derived, 1) : cJSON_CreateNull();
  if (!node || !children || !copy || !cJSON_AddStringToObject(node, "id", id) ||
      !cJSON_AddStringToObject(node, "name", name) || !cJSON_AddNumberToObject(node, "current_version", version) ||
      !cJSON_AddBoolToObject(node, "standalone",
                             cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(derived, "standalone"))) ||
      !cJSON_AddItemToObject(node, "derived", copy)) {
    if (!node || !cJSON_GetObjectItemCaseSensitive(node, "derived")) {
      cJSON_Delete(copy);
    }
    cJSON_Delete(children);
    cJSON_Delete(node);
    return NULL;
  }
  if (!cJSON_AddItemToObject(node, "children", children)) {
    cJSON_Delete(children);
    cJSON_Delete(node);
    return NULL;
  }
  if (depth >= kMaxTreeDepth) {
    return node;
  }
  ancestors[depth] = id;
  const cJSON *entry = NULL;
  cJSON_ArrayForEach(entry, index) {
    const cJSON *entry_id = cJSON_GetObjectItemCaseSensitive(entry, "id");
    const cJSON *source = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(entry, "derived"), "source_item");
    if (!cJSON_IsString(entry_id) || !cJSON_IsString(source) || strcmp(source->valuestring, id) != 0) {
      continue;
    }
    bool seen = false;
    for (int i = 0; i <= depth; i++) {
      seen = seen || strcmp(ancestors[i], entry_id->valuestring) == 0;
    }
    if (seen) {
      continue;
    }
    const cJSON *entry_name = cJSON_GetObjectItemCaseSensitive(entry, "name");
    const cJSON *entry_version = cJSON_GetObjectItemCaseSensitive(entry, "current_version");
    cJSON *child = variant_node(index, entry_id->valuestring, cJSON_IsString(entry_name) ? entry_name->valuestring : "",
                                cJSON_IsNumber(entry_version) ? entry_version->valueint : 0,
                                cJSON_GetObjectItemCaseSensitive(entry, "derived"), ancestors, depth + 1);
    if (!child) {
      cJSON_Delete(node);
      return NULL;
    }
    cJSON_AddItemToArray(children, child);
  }
  return node;
}

/* GET /api/materials/items/<id>/variant-tree: the item and, recursively, every motif derived from it. */
static int handle_variant_tree(struct mg_connection *conn, const char *id) {
  cJSON *meta = NULL;
  cJSON *body = NULL;
  UrsatzGuiMaterialResult result = ursatz_gui_material_item_get(id, &meta, &body);
  if (result != URSATZ_GUI_MATERIAL_OK) {
    return send_material_failure(conn, result);
  }
  cJSON *index = NULL;
  if (!cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(body, "motif"))) {
    cJSON_Delete(meta);
    cJSON_Delete(body);
    return send_failure(conn, 400, "only a motif has variants");
  }
  result = ursatz_gui_material_derived_motifs(&index);
  const char *name = optional_string(meta, "name", &(bool){false});
  const cJSON *version = cJSON_GetObjectItemCaseSensitive(meta, "current_version");
  const char *ancestors[33];
  cJSON *tree = result == URSATZ_GUI_MATERIAL_OK
                    ? variant_node(index, id, name ? name : "", cJSON_IsNumber(version) ? version->valueint : 0,
                                   derived_of(body), ancestors, 0)
                    : NULL;
  cJSON_Delete(meta);
  cJSON_Delete(body);
  cJSON_Delete(index);
  if (result != URSATZ_GUI_MATERIAL_OK) {
    return send_material_failure(conn, result);
  }
  return send_ok_with(conn, "tree", tree);
}

static int handle_update_use(struct mg_connection *conn, const char *id, const char *source_id) {
  cJSON *item = NULL;
  UrsatzGuiMaterialResult result = ursatz_gui_material_update_use(id, source_id, &item);
  if (result != URSATZ_GUI_MATERIAL_OK) {
    return send_material_failure(conn, result);
  }
  return send_ok_with(conn, "item", item);
}

/* POST /api/materials/motifs/random {mode, meter, key?, form?, style_id?, count?, seed?}: random motives from the
   Composer; nothing is saved. mode and meter may be "random": the Composer draws them from the seed and says what it
   drew as "drawn". The answer is the Composer's document plus "ok". */
static int handle_random_motifs(struct mg_connection *conn) {
  int status = 0;
  cJSON *body = read_json_body(conn, &status);
  if (!body) {
    return status;
  }
  bool bad = false;
  const char *mode = optional_string(body, "mode", &bad);
  const char *meter = optional_string(body, "meter", &bad);
  const char *key = optional_string(body, "key", &bad);
  const char *form = optional_string(body, "form", &bad);
  const char *style_id = optional_string(body, "style_id", &bad);
  const char *role = optional_string(body, "role", &bad);
  const cJSON *count_item = cJSON_GetObjectItemCaseSensitive(body, "count");
  const cJSON *seed_item = cJSON_GetObjectItemCaseSensitive(body, "seed");
  if (cJSON_IsNull(count_item)) {
    count_item = NULL;
  }
  if (cJSON_IsNull(seed_item)) {
    seed_item = NULL;
  }
  if (key && key[0] == '\0') {
    key = NULL;
  }
  if (style_id && style_id[0] == '\0') {
    style_id = NULL;
  }
  if (role && role[0] == '\0') {
    role = NULL;
  }
  bool valid = !bad && mode && (!role || ursatz_gui_motif_role_option(role)) && meter &&
               (!form || strcmp(form, "full") == 0 || strcmp(form, "rhythm") == 0 || strcmp(form, "contour") == 0) &&
               (!count_item || (cJSON_IsNumber(count_item) && count_item->valuedouble >= 1 &&
                                count_item->valuedouble <= 16 && count_item->valuedouble == (int)count_item->valuedouble)) &&
               (!seed_item || (cJSON_IsNumber(seed_item) && seed_item->valuedouble >= 0 &&
                               seed_item->valuedouble == (double)(long long)seed_item->valuedouble));
  if (!valid) {
    cJSON_Delete(body);
    return send_failure(conn, 400,
                        "mode and meter are required, form full, rhythm or contour, role melodic or accompaniment, "
                        "count 1 to 16 and seed a whole number from 0");
  }
  char style_path[1024] = "";
  if (style_id && ursatz_gui_style_model_path(style_id, style_path, sizeof(style_path)) != URSATZ_GUI_STYLE_OK) {
    cJSON_Delete(body);
    return send_failure(conn, 400, "unknown style");
  }
  cJSON *request = cJSON_CreateObject();
  bool built = request && cJSON_AddStringToObject(request, "mode", mode) && cJSON_AddStringToObject(request, "meter", meter) &&
               (!key || cJSON_AddStringToObject(request, "key", key)) &&
               cJSON_AddStringToObject(request, "form", form ? form : "full") &&
               (!role || cJSON_AddStringToObject(request, "role", role)) &&
               (!style_id || cJSON_AddStringToObject(request, "style", style_path)) &&
               cJSON_AddNumberToObject(request, "count", count_item ? (int)count_item->valuedouble : 6) &&
               (!seed_item || cJSON_AddNumberToObject(request, "seed", seed_item->valuedouble));
  cJSON_Delete(body);
  UrsatzGuiComposerReply reply;
  bool called = built && ursatz_gui_motif_random_motive(request, &reply);
  cJSON_Delete(request);
  if (!called) {
    return send_failure(conn, 500, "out of memory");
  }
  cJSON *document = reply.status == COMPOSER_OK && reply.json ? cJSON_Parse(reply.json) : NULL;
  if (!document) {
    int failed = send_failure_at(conn, ursatz_gui_composer_reply_failure_status(&reply),
                                 reply.message[0] ? reply.message : "composer could not draw the motives", reply.path);
    ursatz_gui_composer_reply_destroy(&reply);
    return failed;
  }
  ursatz_gui_composer_reply_destroy(&reply);
  if (!cJSON_AddBoolToObject(document, "ok", 1)) {
    cJSON_Delete(document);
    return send_failure(conn, 500, "out of memory");
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
  char message[512] = "";
  bool complete = false;
  UrsatzGuiThemeResult built = URSATZ_GUI_THEME_OK;
  if (is_motif) {
    const cJSON *motif = cJSON_GetObjectItemCaseSensitive(body, "motif");
    const cJSON *form = cJSON_GetObjectItemCaseSensitive(motif, "form");
    const cJSON *notes = cJSON_GetObjectItemCaseSensitive(motif, "notes");
    bool draft = !cJSON_IsObject(motif) || !cJSON_IsString(form) || !cJSON_IsString(notes) ||
                 notes->valuestring[0] == '\0' ||
                 (strcmp(form->valuestring, "full") != 0 && strcmp(form->valuestring, "rhythm") != 0 &&
                  strcmp(form->valuestring, "contour") != 0);
    request = draft ? NULL : ursatz_gui_motif_request(motif);
    complete = request != NULL;
    if (!draft && !request) {
      snprintf(message, sizeof(message), "the motif needs notes, meter and pickup");
    }
  } else {
    /* A form not saved yet names its motifs and themes by reference; the uses a save would make are made here. */
    cJSON *filled = NULL;
    UrsatzGuiMaterialResult uses = ursatz_gui_material_body_with_uses(body, &filled);
    if (uses == URSATZ_GUI_MATERIAL_ERR_ALLOC) {
      built = URSATZ_GUI_THEME_ERR_ALLOC;
    } else if (uses != URSATZ_GUI_MATERIAL_OK) {
      snprintf(message, sizeof(message), "a motif or theme the body names cannot be used yet");
    } else if (is_theme) {
      built = ursatz_gui_theme_validate(filled, &complete, &request, message, sizeof(message));
    } else {
      built = ursatz_gui_piece_validate(filled, &complete, &request, message, sizeof(message));
    }
    cJSON_Delete(filled);
  }
  if (built == URSATZ_GUI_THEME_ERR_ALLOC) {
    cJSON_Delete(request);
    cJSON_Delete(input);
    return send_failure(conn, 500, "out of memory");
  }
  if (built != URSATZ_GUI_THEME_OK) {
    cJSON_Delete(request);
    cJSON_Delete(input);
    return send_failure(conn, built == URSATZ_GUI_THEME_ERR_INVALID ? 400 : 500, message[0] ? message : "the request could not be built");
  }
  if (!complete) {
    cJSON_Delete(input);
    return send_incomplete(conn, message);
  }
  UrsatzGuiComposerReply reply;
  bool called = ursatz_gui_motif_validate(is_motif ? "motive" : type->valuestring, request, &reply);
  cJSON_Delete(request);
  if (!called) {
    cJSON_Delete(input);
    return send_failure(conn, 500, "out of memory");
  }
  cJSON_Delete(input);
  cJSON *document = reply.status == COMPOSER_OK && reply.json ? cJSON_Parse(reply.json) : NULL;
  cJSON *filled = document ? cJSON_DetachItemFromObjectCaseSensitive(document, "request") : NULL;
  cJSON_Delete(document);
  if (!filled) {
    int failed = send_failure_at(conn, ursatz_gui_composer_reply_failure_status(&reply),
                                 reply.message[0] ? reply.message : "composer could not check the request", reply.path);
    ursatz_gui_composer_reply_destroy(&reply);
    return failed;
  }
  ursatz_gui_composer_reply_destroy(&reply);
  cJSON *answer = cJSON_CreateObject();
  if (!answer || !cJSON_AddBoolToObject(answer, "ok", 1) || !cJSON_AddBoolToObject(answer, "complete", 1) ||
      !cJSON_AddItemToObject(answer, "request", filled)) {
    cJSON_Delete(filled);
    cJSON_Delete(answer);
    return send_failure(conn, 500, "out of memory");
  }
  return ursatz_gui_send_object(conn, 200, answer);
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
    return post ? handle_variant_save(conn, segments[1]) : send_method_not_allowed(conn);
  }
  if (count == 3 && strcmp(segments[2], "variant-tree") == 0) {
    return get ? handle_variant_tree(conn, segments[1]) : send_method_not_allowed(conn);
  }
  if (count == 3 && strcmp(segments[2], "reroll") == 0) {
    return post ? handle_reroll(conn, segments[1]) : send_method_not_allowed(conn);
  }
  if (count == 3 && strcmp(segments[2], "link-form") == 0) {
    return post ? handle_link_form(conn, segments[1]) : send_method_not_allowed(conn);
  }
  if (count == 3 && strcmp(segments[2], "promote") == 0) {
    return post ? handle_promote(conn, segments[1]) : send_method_not_allowed(conn);
  }
  if (count == 3 && strcmp(segments[2], "flag") == 0) {
    return post ? handle_flag(conn, segments[1]) : send_method_not_allowed(conn);
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

static int subject_status(UrsatzGuiSubjectResult result) {
  switch (result) {
    case URSATZ_GUI_SUBJECT_OK:
      return 200;
    case URSATZ_GUI_SUBJECT_ERR_INVALID_ID:
    case URSATZ_GUI_SUBJECT_ERR_INVALID_NAME:
    case URSATZ_GUI_SUBJECT_ERR_INVALID_KIND:
    case URSATZ_GUI_SUBJECT_ERR_SELF:
      return 400;
    case URSATZ_GUI_SUBJECT_ERR_NOT_FOUND:
    case URSATZ_GUI_SUBJECT_ERR_ITEM_NOT_FOUND:
    case URSATZ_GUI_SUBJECT_ERR_PIECE_NOT_FOUND:
    case URSATZ_GUI_SUBJECT_ERR_NOT_LINKED:
    case URSATZ_GUI_SUBJECT_ERR_NOT_RELATED:
      return 404;
    default:
      return 500;
  }
}

static const char *subject_message(UrsatzGuiSubjectResult result) {
  switch (result) {
    case URSATZ_GUI_SUBJECT_ERR_INVALID_ID:
      return "invalid id";
    case URSATZ_GUI_SUBJECT_ERR_INVALID_NAME:
      return "invalid name or note";
    case URSATZ_GUI_SUBJECT_ERR_INVALID_KIND:
      return "invalid kind";
    case URSATZ_GUI_SUBJECT_ERR_SELF:
      return "a subject cannot relate to itself";
    case URSATZ_GUI_SUBJECT_ERR_NOT_FOUND:
      return "subject not found";
    case URSATZ_GUI_SUBJECT_ERR_ITEM_NOT_FOUND:
      return "item not found";
    case URSATZ_GUI_SUBJECT_ERR_PIECE_NOT_FOUND:
      return "piece not found";
    case URSATZ_GUI_SUBJECT_ERR_NOT_LINKED:
      return "not linked";
    case URSATZ_GUI_SUBJECT_ERR_NOT_RELATED:
      return "not related";
    default:
      return "subject store failure";
  }
}

static int send_subject_failure(struct mg_connection *conn, UrsatzGuiSubjectResult result) {
  return send_failure(conn, subject_status(result), subject_message(result));
}

cJSON *ursatz_gui_materials_subjects_for(const char *kind, const char *id) {
  cJSON *rows = NULL;
  cJSON *found = cJSON_CreateArray();
  if (!found || ursatz_gui_subject_list(&rows) != URSATZ_GUI_SUBJECT_OK) {
    cJSON_Delete(found);
    return NULL;
  }
  const cJSON *row = NULL;
  cJSON_ArrayForEach(row, rows) {
    const cJSON *row_id = cJSON_GetObjectItemCaseSensitive(row, "id");
    cJSON *subject = NULL;
    if (!cJSON_IsString(row_id) || ursatz_gui_subject_get(row_id->valuestring, &subject) != URSATZ_GUI_SUBJECT_OK) {
      continue;
    }
    const cJSON *links = cJSON_GetObjectItemCaseSensitive(subject, kind);
    const cJSON *link = NULL;
    bool linked = false;
    cJSON_ArrayForEach(link, links) {
      if (cJSON_IsString(link) && strcmp(link->valuestring, id) == 0) {
        linked = true;
        break;
      }
    }
    cJSON *entry = linked ? cJSON_CreateObject() : NULL;
    if (entry) {
      const cJSON *name = cJSON_GetObjectItemCaseSensitive(subject, "name");
      const cJSON *subject_kind = cJSON_GetObjectItemCaseSensitive(subject, "kind");
      if (!cJSON_AddStringToObject(entry, "id", row_id->valuestring) ||
          !cJSON_AddStringToObject(entry, "name", cJSON_IsString(name) ? name->valuestring : "") ||
          !cJSON_AddStringToObject(entry, "kind", cJSON_IsString(subject_kind) ? subject_kind->valuestring : "")) {
        cJSON_Delete(entry);
        cJSON_Delete(subject);
        cJSON_Delete(rows);
        cJSON_Delete(found);
        return NULL;
      }
      cJSON_AddItemToArray(found, entry);
    }
    cJSON_Delete(subject);
  }
  cJSON_Delete(rows);
  return found;
}

static int handle_list_subjects(struct mg_connection *conn) {
  cJSON *subjects = NULL;
  UrsatzGuiSubjectResult result = ursatz_gui_subject_list(&subjects);
  if (result != URSATZ_GUI_SUBJECT_OK) {
    return send_subject_failure(conn, result);
  }
  return send_ok_with(conn, "subjects", subjects);
}

static int handle_subject_groups(struct mg_connection *conn) {
  cJSON *groups = NULL;
  UrsatzGuiSubjectResult result = ursatz_gui_subject_groups(&groups);
  if (result != URSATZ_GUI_SUBJECT_OK) {
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
  UrsatzGuiSubjectResult result = ursatz_gui_subject_create(name, kind, note, &subject);
  cJSON_Delete(body);
  if (result != URSATZ_GUI_SUBJECT_OK) {
    return send_subject_failure(conn, result);
  }
  return send_ok_with(conn, "subject", subject);
}

static int handle_get_subject(struct mg_connection *conn, const char *id) {
  cJSON *subject = NULL;
  UrsatzGuiSubjectResult result = ursatz_gui_subject_get(id, &subject);
  if (result != URSATZ_GUI_SUBJECT_OK) {
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
  UrsatzGuiSubjectResult result = ursatz_gui_subject_update(id, name, kind, note, &subject);
  cJSON_Delete(body);
  if (result != URSATZ_GUI_SUBJECT_OK) {
    return send_subject_failure(conn, result);
  }
  return send_ok_with(conn, "subject", subject);
}

static int handle_delete_subject(struct mg_connection *conn, const char *id) {
  UrsatzGuiSubjectResult result = ursatz_gui_subject_delete(id);
  if (result != URSATZ_GUI_SUBJECT_OK) {
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
  UrsatzGuiSubjectResult result;
  if (post && strcmp(kind, "items") == 0) {
    /* A variant belongs to its motif and has no subjects (R-187). An absent item is left to the subject service. */
    cJSON *body = NULL;
    bool variant = ursatz_gui_material_item_get(other, NULL, &body) == URSATZ_GUI_MATERIAL_OK &&
                   ursatz_gui_material_item_is_variant(body);
    cJSON_Delete(body);
    if (variant) {
      return send_failure(conn, 400, "variants have no subjects; save it as a new motif first");
    }
  }
  if (strcmp(kind, "items") == 0) {
    result = post ? ursatz_gui_subject_link_item(id, other, &already, &subject)
                  : ursatz_gui_subject_unlink_item(id, other, &subject);
  } else if (strcmp(kind, "pieces") == 0) {
    result = post ? ursatz_gui_subject_link_piece(id, other, &already, &subject)
                  : ursatz_gui_subject_unlink_piece(id, other, &subject);
  } else {
    result = post ? ursatz_gui_subject_relate(id, other, &already, &subject)
                  : ursatz_gui_subject_unrelate(id, other, &subject);
  }
  if (result != URSATZ_GUI_SUBJECT_OK) {
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
  if (count == 2 && strcmp(segments[0], "motifs") == 0 && strcmp(segments[1], "to-c") == 0) {
    return strcmp(method, "POST") == 0 ? handle_motifs_to_c(conn) : send_method_not_allowed(conn);
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
