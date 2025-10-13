#include "materials.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <civetweb.h>
#include <ctype.h>
#include <unistd.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#include "../service/composer_service.h"
#include "../service/material_service.h"
#include "../service/subject_service.h"

static const char *kMaterialsPrefix = "/api/materials";
static const char *kSubjectsPrefix = "/api/subjects";
static const size_t kMaxRequestBodySize = 256 * 1024;
static const int kMaxSegments = 6;

static int send_json_status(struct mg_connection *conn, int status, const char *reason, const char *body) {
  size_t len = strlen(body);
  mg_printf(conn,
            "HTTP/1.1 %d %s\r\nContent-Type: application/json\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n",
            status, reason, len);
  mg_write(conn, body, len);
  return status;
}

static const char *reason_for(int status) {
  switch (status) {
    case 200:
      return "OK";
    case 404:
      return "Not Found";
    case 405:
      return "Method Not Allowed";
    case 409:
      return "Conflict";
    case 413:
      return "Payload Too Large";
    case 500:
      return "Internal Server Error";
    default:
      return "Bad Request";
  }
}

/* Sends {"ok":false,"error":...} with the given status. The message goes through cJSON, so it is escaped. */
static int send_failure(struct mg_connection *conn, int status, const char *message) {
  cJSON *object = cJSON_CreateObject();
  char *text = NULL;
  if (object && cJSON_AddBoolToObject(object, "ok", 0) && cJSON_AddStringToObject(object, "error", message)) {
    text = cJSON_PrintUnformatted(object);
  }
  cJSON_Delete(object);
  if (!text) {
    return send_json_status(conn, 500, reason_for(500), "{\"ok\":false,\"error\":\"out of memory\"}");
  }
  send_json_status(conn, status, reason_for(status), text);
  free(text);
  return status;
}

/* Sends object as the 200 response and frees it. */
static int send_object(struct mg_connection *conn, cJSON *object) {
  char *text = object ? cJSON_PrintUnformatted(object) : NULL;
  cJSON_Delete(object);
  if (!text) {
    return send_failure(conn, 500, "out of memory");
  }
  send_json_status(conn, 200, reason_for(200), text);
  free(text);
  return 200;
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
  return send_object(conn, object);
}

static int send_ok(struct mg_connection *conn) {
  send_json_status(conn, 200, reason_for(200), "{\"ok\":true}");
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
  size_t capacity = 4096;
  char *buffer = malloc(capacity);
  size_t total = 0;
  *status = 400;
  if (!buffer) {
    *status = send_failure(conn, 500, "out of memory");
    return NULL;
  }
  for (;;) {
    if (total + 1 >= capacity) {
      if (capacity > kMaxRequestBodySize) {
        free(buffer);
        *status = send_failure(conn, 413, "request body too large");
        return NULL;
      }
      char *grown = realloc(buffer, capacity * 2);
      if (!grown) {
        free(buffer);
        *status = send_failure(conn, 500, "out of memory");
        return NULL;
      }
      buffer = grown;
      capacity *= 2;
    }
    int n = mg_read(conn, buffer + total, capacity - total - 1);
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
  buffer[total] = '\0';
  cJSON *body = cJSON_Parse(buffer);
  free(buffer);
  if (!cJSON_IsObject(body)) {
    cJSON_Delete(body);
    *status = send_failure(conn, 400, "the body must be a JSON object");
    return NULL;
  }
  *status = 200;
  return body;
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

/* ---- motif analysis on save ----------------------------------------- */

/* meter ^[0-9]{1,2}/[0-9]{1,2}$ */
static bool analysis_meter_ok(const char *text) {
  size_t i = 0;
  while (isdigit((unsigned char)text[i])) {
    i++;
  }
  if (i < 1 || i > 2 || text[i] != '/') {
    return false;
  }
  size_t start = ++i;
  while (isdigit((unsigned char)text[i])) {
    i++;
  }
  return i - start >= 1 && i - start <= 2 && text[i] == '\0';
}

/* key ^[A-Ga-g][#b]? (major|minor)$ */
static bool analysis_key_ok(const char *text) {
  if (!((text[0] >= 'A' && text[0] <= 'G') || (text[0] >= 'a' && text[0] <= 'g'))) {
    return false;
  }
  const char *rest = text + 1;
  if (*rest == '#' || *rest == 'b') {
    rest++;
  }
  return strcmp(rest, " major") == 0 || strcmp(rest, " minor") == 0;
}

typedef enum {
  ANALYSIS_DONE,
  ANALYSIS_REFUSED,
  ANALYSIS_ALLOC
} AnalysisOutcome;

static void set_motif_member(cJSON *motif, const char *name, cJSON *value) {
  if (cJSON_GetObjectItemCaseSensitive(motif, name)) {
    cJSON_ReplaceItemInObjectCaseSensitive(motif, name, value);
  } else {
    cJSON_AddItemToObject(motif, name, value);
  }
}

static void set_motif_analysis(cJSON *motif, cJSON *analysis) {
  set_motif_member(motif, "analysis", analysis);
}

/* pickup ^[0-9]{1,2}(/[0-9]{1,2})?$ */
static bool analysis_pickup_ok(const char *text) {
  size_t i = 0;
  while (isdigit((unsigned char)text[i])) {
    i++;
  }
  if (i < 1 || i > 2) {
    return false;
  }
  if (text[i] == '\0') {
    return true;
  }
  if (text[i] != '/') {
    return false;
  }
  size_t start = ++i;
  while (isdigit((unsigned char)text[i])) {
    i++;
  }
  return i - start >= 1 && i - start <= 2 && text[i] == '\0';
}

/* The text the Composer returned for the form: degrees_text, rhythm_text or contour_text, or NULL. */
static const char *analysis_degrees_text(const cJSON *output, const char *form) {
  const cJSON *analysis = cJSON_GetObjectItemCaseSensitive(output, "analysis");
  const char *name = strcmp(form, "rhythm") == 0 ? "rhythm_text" : strcmp(form, "contour") == 0 ? "contour_text" : "degrees_text";
  const cJSON *text = cJSON_GetObjectItemCaseSensitive(analysis, name);
  return cJSON_IsString(text) ? text->valuestring : NULL;
}

/* Runs the Composer's motive analysis on a motif body (full, rhythm or contour) and stores the result as
   motif.analysis and its degree, rhythm or contour list as motif.degrees. A body without a valid motif is left alone
   for the material service to judge. When the Composer cannot run, the analysis and degrees are stored as null and
   *analysis_error names the reason. When the Composer reads the input and refuses it, or a full motif has no key to
   write its degrees in, the result is REFUSED and refusal holds its message. */
static AnalysisOutcome analyze_motif_body(cJSON *version_body, const char **analysis_error, char *refusal,
                                          size_t refusal_size) {
  *analysis_error = NULL;
  cJSON *motif = cJSON_GetObjectItemCaseSensitive(version_body, "motif");
  if (!cJSON_IsObject(motif)) {
    return ANALYSIS_DONE;
  }
  const cJSON *form = cJSON_GetObjectItemCaseSensitive(motif, "form");
  const cJSON *notes = cJSON_GetObjectItemCaseSensitive(motif, "notes");
  const cJSON *meter = cJSON_GetObjectItemCaseSensitive(motif, "meter");
  const cJSON *key = cJSON_GetObjectItemCaseSensitive(motif, "key");
  const cJSON *pickup = cJSON_GetObjectItemCaseSensitive(motif, "pickup");
  if (!cJSON_IsString(form) ||
      (strcmp(form->valuestring, "full") != 0 && strcmp(form->valuestring, "rhythm") != 0 &&
       strcmp(form->valuestring, "contour") != 0)) {
    return ANALYSIS_DONE;
  }
  bool has_key = cJSON_IsString(key) && key->valuestring[0] != '\0';
  if (!cJSON_IsString(notes) || !cJSON_IsString(meter) || !analysis_meter_ok(meter->valuestring) ||
      (has_key && !analysis_key_ok(key->valuestring)) || !cJSON_IsString(pickup) ||
      !analysis_pickup_ok(pickup->valuestring)) {
    return ANALYSIS_DONE;
  }
  bool full = strcmp(form->valuestring, "full") == 0;
  const char *tmp_dir = getenv("TMPDIR");
  char path[512];
  if (!tmp_dir || tmp_dir[0] != '/' || snprintf(path, sizeof(path), "%s/ursatz-motif-XXXXXX", tmp_dir) >= (int)sizeof(path)) {
    snprintf(path, sizeof(path), "/tmp/ursatz-motif-XXXXXX");
  }
  int fd = mkstemp(path);
  if (fd < 0) {
    *analysis_error = "could not write the note list for the Composer";
    set_motif_analysis(motif, cJSON_CreateNull());
    set_motif_member(motif, "degrees", cJSON_CreateNull());
    return ANALYSIS_DONE;
  }
  size_t length = strlen(notes->valuestring);
  bool written = write(fd, notes->valuestring, length) == (ssize_t)length;
  close(fd);
  cJSON *analysis = NULL;
  bool refused = false;
  if (!written) {
    *analysis_error = "could not write the note list for the Composer";
  } else {
    const char *args[14];
    int n = 0;
    args[n++] = "motive";
    args[n++] = path;
    args[n++] = full ? "--format" : "--form";
    args[n++] = full ? "notes" : form->valuestring;
    args[n++] = "--meter";
    args[n++] = meter->valuestring;
    if (has_key) {
      args[n++] = "--key";
      args[n++] = key->valuestring;
    }
    args[n++] = "--pickup";
    args[n++] = pickup->valuestring;
    args[n++] = "--json";
    args[n] = NULL;
    UrsatzGuiComposerOutput out = {0, NULL, NULL};
    UrsatzGuiComposerResult result = ursatz_gui_composer_run(args, &out);
    if (result == URSATZ_GUI_COMPOSER_ERR_NO_BINARY) {
      *analysis_error = "composer not found";
    } else if (result == URSATZ_GUI_COMPOSER_ERR_TIMEOUT) {
      *analysis_error = "composer timed out";
    } else if (result != URSATZ_GUI_COMPOSER_OK) {
      *analysis_error = "composer could not be started";
    } else {
      analysis = out.exit_status == 0 ? cJSON_Parse(out.stdout_text) : NULL;
      if (out.exit_status == 1 || out.exit_status == 2) {
        refused = true;
        snprintf(refusal, refusal_size, "%s", out.stderr_text[0] ? out.stderr_text : "composer could not read the motif");
        size_t end = strlen(refusal);
        while (end > 0 && (refusal[end - 1] == '\n' || refusal[end - 1] == '\r')) {
          refusal[--end] = '\0';
        }
      } else if (!analysis) {
        *analysis_error = "composer could not analyze the motif";
      }
      ursatz_gui_composer_output_destroy(&out);
    }
  }
  unlink(path);
  if (refused) {
    return ANALYSIS_REFUSED;
  }
  const char *degrees_text = analysis ? analysis_degrees_text(analysis, form->valuestring) : NULL;
  if (analysis && full && !degrees_text) {
    cJSON_Delete(analysis);
    snprintf(refusal, refusal_size, "give a key");
    return ANALYSIS_REFUSED;
  }
  cJSON *degrees = degrees_text ? cJSON_CreateString(degrees_text) : cJSON_CreateNull();
  if (!analysis) {
    analysis = cJSON_CreateNull();
  }
  if (!analysis || !degrees) {
    cJSON_Delete(analysis);
    cJSON_Delete(degrees);
    return ANALYSIS_ALLOC;
  }
  set_motif_analysis(motif, analysis);
  set_motif_member(motif, "degrees", degrees);
  return ANALYSIS_DONE;
}

/* Adds "analysis_error" to the item answer when the Composer was unavailable. */
static int send_item_answer(struct mg_connection *conn, cJSON *item, const char *analysis_error) {
  if (item && analysis_error) {
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

/* The key of a motive document as "D minor", or NULL when it has none. */
static bool import_key_text(const cJSON *document, char *out, size_t out_size) {
  static const char *const names[] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
  const cJSON *key = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(document, "motive"), "key");
  const cJSON *tonic = cJSON_GetObjectItemCaseSensitive(key, "tonic_pitch_class");
  const cJSON *mode = cJSON_GetObjectItemCaseSensitive(key, "mode");
  if (!cJSON_IsNumber(tonic) || tonic->valueint < 0 || tonic->valueint > 11 || !cJSON_IsString(mode)) {
    return false;
  }
  snprintf(out, out_size, "%s %s", names[tonic->valueint], mode->valuestring);
  return true;
}

/* POST /api/materials/motifs/import: the body is a MIDI file; ?meter= and ?key= are optional. Answers
   {"ok":true,"notes_text":...,"meter":"4/4","key":"D minor"|null,"pickup":"0","analysis":{...}} and saves nothing. */
static int handle_import_motif(struct mg_connection *conn) {
  char meter[16];
  char key[32];
  bool has_meter = query_value(conn, "meter", meter, sizeof(meter));
  bool has_key = query_value(conn, "key", key, sizeof(key));
  if ((has_meter && !analysis_meter_ok(meter)) || (has_key && !analysis_key_ok(key))) {
    return send_failure(conn, 400, "meter must look like 4/4 and key like D minor");
  }
  int status = 0;
  size_t size = 0;
  unsigned char *bytes = read_file_body(conn, &size, &status);
  if (!bytes) {
    return status;
  }
  const char *tmp_dir = getenv("TMPDIR");
  char path[512];
  if (!tmp_dir || tmp_dir[0] != '/' || snprintf(path, sizeof(path), "%s/ursatz-import-XXXXXX", tmp_dir) >= (int)sizeof(path)) {
    snprintf(path, sizeof(path), "/tmp/ursatz-import-XXXXXX");
  }
  int fd = mkstemp(path);
  if (fd < 0) {
    free(bytes);
    return send_failure(conn, 500, "could not write the MIDI file for the Composer");
  }
  bool written = write(fd, bytes, size) == (ssize_t)size;
  close(fd);
  free(bytes);
  if (!written) {
    unlink(path);
    return send_failure(conn, 500, "could not write the MIDI file for the Composer");
  }
  const char *args[10];
  int n = 0;
  args[n++] = "motive";
  args[n++] = path;
  args[n++] = "--format";
  args[n++] = "midi";
  if (has_meter) {
    args[n++] = "--meter";
    args[n++] = meter;
  }
  if (has_key) {
    args[n++] = "--key";
    args[n++] = key;
  }
  args[n++] = "--json";
  args[n] = NULL;
  UrsatzGuiComposerOutput out = {0, NULL, NULL};
  UrsatzGuiComposerResult result = ursatz_gui_composer_run(args, &out);
  unlink(path);
  if (result == URSATZ_GUI_COMPOSER_ERR_NO_BINARY) {
    return send_failure(conn, 503, "composer not found");
  }
  if (result == URSATZ_GUI_COMPOSER_ERR_TIMEOUT) {
    return send_failure(conn, 504, "composer timed out");
  }
  if (result != URSATZ_GUI_COMPOSER_OK) {
    return send_failure(conn, 500, "composer could not be started");
  }
  if (out.exit_status != 0) {
    char message[512];
    snprintf(message, sizeof(message), "%s", out.stderr_text && out.stderr_text[0] ? out.stderr_text : "composer could not read the MIDI file");
    size_t end = strlen(message);
    while (end > 0 && (message[end - 1] == '\n' || message[end - 1] == '\r')) {
      message[--end] = '\0';
    }
    ursatz_gui_composer_output_destroy(&out);
    return send_failure(conn, 400, message);
  }
  cJSON *document = cJSON_Parse(out.stdout_text);
  ursatz_gui_composer_output_destroy(&out);
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
  cJSON *answer = cJSON_CreateObject();
  if (!answer) {
    cJSON_Delete(document);
    return send_failure(conn, 500, "out of memory");
  }
  cJSON_AddBoolToObject(answer, "ok", 1);
  cJSON_AddStringToObject(answer, "notes_text", notes_text->valuestring);
  cJSON_AddStringToObject(answer, "meter", meter_text);
  if (keyed) {
    cJSON_AddStringToObject(answer, "key", key_text);
  } else {
    cJSON_AddNullToObject(answer, "key");
  }
  cJSON_AddStringToObject(answer, "pickup", pickup->valuestring);
  cJSON_AddItemToObject(answer, "analysis", cJSON_DetachItemFromObjectCaseSensitive(document, "analysis"));
  cJSON_Delete(document);
  return send_object(conn, answer);
}

typedef struct {
  const char *form;
  const char *degrees;
  const char *meter;
  const char *key;
  const char *pickup;
  const char *render_key;
  const char *role;
  int tempo;
} RenderInput;

typedef struct {
  int status;
  char message[512];
} RenderFailure;

static void render_fail(RenderFailure *failure, int status, const char *message) {
  failure->status = status;
  snprintf(failure->message, sizeof(failure->message), "%s", message);
}

static long long fraction_gcd(long long a, long long b) {
  while (b != 0) {
    long long t = a % b;
    a = b;
    b = t;
  }
  return a;
}

static bool fraction_member(const cJSON *object, const char *name, long long *num, long long *den) {
  const cJSON *fraction = cJSON_GetObjectItemCaseSensitive(object, name);
  const cJSON *n = cJSON_GetObjectItemCaseSensitive(fraction, "num");
  const cJSON *d = cJSON_GetObjectItemCaseSensitive(fraction, "den");
  if (!cJSON_IsNumber(n) || !cJSON_IsNumber(d) || n->valuedouble < 0 || d->valuedouble < 1) {
    return false;
  }
  long long g = fraction_gcd((long long)n->valuedouble, (long long)d->valuedouble);
  g = g == 0 ? 1 : g;
  *num = (long long)n->valuedouble / g;
  *den = (long long)d->valuedouble / g;
  return true;
}

static cJSON *fraction_pair(long long num, long long den) {
  cJSON *pair = cJSON_CreateArray();
  if (pair) {
    cJSON_AddItemToArray(pair, cJSON_CreateNumber((double)num));
    cJSON_AddItemToArray(pair, cJSON_CreateNumber((double)den));
  }
  return pair;
}

/* The playback shape of analysis_service.h for the Composer's rendered notes: one part named after the role, each
   position and length in whole notes from the pickup start (the Composer's offsets already start there). */
static cJSON *build_motif_playback(const cJSON *rendered_notes, const char *role, int tempo) {
  cJSON *playback = cJSON_CreateObject();
  cJSON *tempo_list = cJSON_CreateArray();
  cJSON *parts = cJSON_CreateArray();
  cJSON *notes = cJSON_CreateArray();
  cJSON *region = cJSON_CreateObject();
  cJSON *part = cJSON_CreateObject();
  if (!playback || !tempo_list || !parts || !notes || !region || !part) {
    cJSON_Delete(playback);
    cJSON_Delete(tempo_list);
    cJSON_Delete(parts);
    cJSON_Delete(notes);
    cJSON_Delete(region);
    cJSON_Delete(part);
    return NULL;
  }
  cJSON_AddItemToObject(region, "at", fraction_pair(0, 1));
  cJSON_AddNumberToObject(region, "qpm", tempo > 0 ? tempo : 120);
  cJSON_AddItemToArray(tempo_list, region);
  cJSON_AddNumberToObject(part, "index", 0);
  cJSON_AddStringToObject(part, "name", role);
  cJSON_AddNumberToObject(part, "program", 0);
  cJSON_AddItemToArray(parts, part);
  long long end_num = 0;
  long long end_den = 1;
  int index = 0;
  const cJSON *note = NULL;
  cJSON_ArrayForEach(note, rendered_notes) {
    long long on_num, on_den, len_num, len_den;
    const cJSON *pitch = cJSON_GetObjectItemCaseSensitive(note, "midi_pitch");
    if (!cJSON_IsNumber(pitch) || !fraction_member(note, "offset", &on_num, &on_den) ||
        !fraction_member(note, "duration", &len_num, &len_den)) {
      cJSON_Delete(playback);
      cJSON_Delete(tempo_list);
      cJSON_Delete(parts);
      cJSON_Delete(notes);
      return NULL;
    }
    cJSON *entry = cJSON_CreateObject();
    if (!entry) {
      cJSON_Delete(playback);
      cJSON_Delete(tempo_list);
      cJSON_Delete(parts);
      cJSON_Delete(notes);
      return NULL;
    }
    cJSON_AddNumberToObject(entry, "i", index++);
    cJSON_AddItemToObject(entry, "on", fraction_pair(on_num, on_den));
    cJSON_AddItemToObject(entry, "len", fraction_pair(len_num, len_den));
    cJSON_AddNumberToObject(entry, "pitch", pitch->valueint);
    cJSON_AddNumberToObject(entry, "part", 0);
    cJSON_AddItemToArray(notes, entry);
    long long stop_num = on_num * len_den + len_num * on_den;
    long long stop_den = on_den * len_den;
    long long g = fraction_gcd(stop_num, stop_den);
    g = g == 0 ? 1 : g;
    stop_num /= g;
    stop_den /= g;
    if (stop_num * end_den > end_num * stop_den) {
      end_num = stop_num;
      end_den = stop_den;
    }
  }
  cJSON_AddBoolToObject(playback, "ok", 1);
  cJSON_AddStringToObject(playback, "tempo_source", tempo > 0 ? "given" : "default");
  cJSON_AddItemToObject(playback, "tempo", tempo_list);
  cJSON_AddItemToObject(playback, "parts", parts);
  cJSON_AddItemToObject(playback, "end", fraction_pair(end_num, end_den));
  cJSON_AddItemToObject(playback, "notes", notes);
  return playback;
}

static bool temp_file(char *path, size_t size, const char *stem) {
  const char *tmp_dir = getenv("TMPDIR");
  if (!tmp_dir || tmp_dir[0] != '/' || snprintf(path, size, "%s/%s-XXXXXX", tmp_dir, stem) >= (int)size) {
    snprintf(path, size, "/tmp/%s-XXXXXX", stem);
  }
  int fd = mkstemp(path);
  if (fd < 0) {
    return false;
  }
  close(fd);
  return true;
}

static char *read_whole_file(const char *path) {
  FILE *file = fopen(path, "rb");
  if (!file) {
    return NULL;
  }
  size_t capacity = 8192;
  size_t total = 0;
  char *text = malloc(capacity);
  while (text) {
    size_t n = fread(text + total, 1, capacity - total - 1, file);
    total += n;
    if (n == 0) {
      break;
    }
    if (total + 1 >= capacity) {
      char *grown = realloc(text, capacity * 2);
      if (!grown) {
        free(text);
        text = NULL;
        break;
      }
      text = grown;
      capacity *= 2;
    }
  }
  fclose(file);
  if (text) {
    text[total] = '\0';
  }
  return text;
}

/* Renders a degree, rhythm or contour list in render_key with the Composer. On success *out_xml is the MusicXML text
   and *out_playback its playback data (caller-owned). On failure failure holds the status and message. */
static bool render_motif(const RenderInput *in, char **out_xml, cJSON **out_playback, RenderFailure *failure) {
  const char *form = in->form;
  if (!form || (strcmp(form, "full") != 0 && strcmp(form, "rhythm") != 0 && strcmp(form, "contour") != 0)) {
    render_fail(failure, 400, "form must be full, rhythm or contour");
    return false;
  }
  size_t degrees_length = in->degrees ? strlen(in->degrees) : 0;
  if (degrees_length == 0 || degrees_length > 4000) {
    render_fail(failure, 400, "degrees must be a list of 1 to 4000 characters");
    return false;
  }
  bool has_key = in->key && in->key[0] != '\0';
  const char *render_key = in->render_key && in->render_key[0] != '\0' ? in->render_key : (has_key ? in->key : NULL);
  if (!in->meter || !analysis_meter_ok(in->meter) || (has_key && !analysis_key_ok(in->key)) ||
      (render_key && !analysis_key_ok(render_key)) || !in->pickup || !analysis_pickup_ok(in->pickup)) {
    render_fail(failure, 400, "meter must look like 4/4, key like D minor and pickup like 1 or 1/2");
    return false;
  }
  if (!render_key) {
    render_fail(failure, 400, "give a key");
    return false;
  }
  if (in->tempo < 0 || in->tempo > 400 || (in->tempo > 0 && in->tempo < 20)) {
    render_fail(failure, 400, "tempo must be 20 to 400");
    return false;
  }
  char input_path[512];
  char xml_path[512];
  if (!temp_file(input_path, sizeof(input_path), "ursatz-render")) {
    render_fail(failure, 500, "could not write the list for the Composer");
    return false;
  }
  if (!temp_file(xml_path, sizeof(xml_path), "ursatz-render-xml")) {
    unlink(input_path);
    render_fail(failure, 500, "could not write the list for the Composer");
    return false;
  }
  FILE *file = fopen(input_path, "wb");
  bool written = file && fwrite(in->degrees, 1, degrees_length, file) == degrees_length;
  if (file) {
    written = fclose(file) == 0 && written;
  }
  bool ok = false;
  if (!written) {
    render_fail(failure, 500, "could not write the list for the Composer");
  } else {
    char tempo_text[16];
    snprintf(tempo_text, sizeof(tempo_text), "%d", in->tempo);
    const char *args[24];
    int n = 0;
    args[n++] = "motive";
    args[n++] = input_path;
    args[n++] = "--format";
    args[n++] = strcmp(form, "full") == 0 ? "degrees" : form;
    args[n++] = "--form";
    args[n++] = form;
    args[n++] = "--meter";
    args[n++] = in->meter;
    if (has_key) {
      args[n++] = "--key";
      args[n++] = in->key;
    }
    args[n++] = "--pickup";
    args[n++] = in->pickup;
    args[n++] = "--render-key";
    args[n++] = render_key;
    args[n++] = "--musicxml";
    args[n++] = xml_path;
    if (in->tempo > 0) {
      args[n++] = "--tempo";
      args[n++] = tempo_text;
    }
    args[n++] = "--json";
    args[n] = NULL;
    UrsatzGuiComposerOutput out = {0, NULL, NULL};
    UrsatzGuiComposerResult result = ursatz_gui_composer_run(args, &out);
    if (result == URSATZ_GUI_COMPOSER_ERR_NO_BINARY) {
      render_fail(failure, 503, "composer not found");
    } else if (result == URSATZ_GUI_COMPOSER_ERR_TIMEOUT) {
      render_fail(failure, 504, "composer timed out");
    } else if (result != URSATZ_GUI_COMPOSER_OK) {
      render_fail(failure, 500, "composer could not be started");
    } else {
      if (out.exit_status == 1 || out.exit_status == 2) {
        char message[512];
        snprintf(message, sizeof(message), "%s", out.stderr_text[0] ? out.stderr_text : "composer could not render the motif");
        size_t end = strlen(message);
        while (end > 0 && (message[end - 1] == '\n' || message[end - 1] == '\r')) {
          message[--end] = '\0';
        }
        render_fail(failure, 400, message);
      } else {
        cJSON *document = out.exit_status == 0 ? cJSON_Parse(out.stdout_text) : NULL;
        const cJSON *notes = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(document, "rendered"), "notes");
        char *xml = document && cJSON_IsArray(notes) ? read_whole_file(xml_path) : NULL;
        cJSON *playback = xml ? build_motif_playback(notes, in->role && in->role[0] ? in->role : "motif", in->tempo) : NULL;
        if (playback) {
          *out_xml = xml;
          *out_playback = playback;
          ok = true;
        } else {
          free(xml);
          render_fail(failure, 500, "composer could not render the motif");
        }
        cJSON_Delete(document);
      }
      ursatz_gui_composer_output_destroy(&out);
    }
  }
  unlink(input_path);
  unlink(xml_path);
  return ok;
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
  return send_object(conn, answer);
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
  if (!in.pickup) {
    in.pickup = "0";
  }
  const cJSON *tempo = cJSON_GetObjectItemCaseSensitive(body, "tempo");
  if (tempo && !cJSON_IsNull(tempo)) {
    if (!cJSON_IsNumber(tempo) || tempo->valuedouble != (double)tempo->valueint) {
      bad = true;
    } else {
      in.tempo = tempo->valueint;
    }
  }
  if (bad) {
    cJSON_Delete(body);
    return send_failure(conn, 400, "form, degrees, meter, key, pickup, render_key and role must be text and tempo a whole number");
  }
  char *xml = NULL;
  cJSON *playback = NULL;
  RenderFailure failure = {0, ""};
  bool ok = render_motif(&in, &xml, &playback, &failure);
  cJSON_Delete(body);
  if (!ok) {
    return send_failure(conn, failure.status, failure.message);
  }
  return send_render_answer(conn, xml, playback);
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
  const cJSON *version_body = cJSON_GetObjectItemCaseSensitive(body, "body");
  if (bad || !type || !name || !project_id || !cJSON_IsObject(version_body)) {
    cJSON_Delete(body);
    return send_failure(conn, 400, "type, name, project_id and an object body are required");
  }
  const char *analysis_error = NULL;
  char refusal[512] = "";
  AnalysisOutcome analysis = analyze_motif_body((cJSON *)version_body, &analysis_error, refusal, sizeof(refusal));
  if (analysis != ANALYSIS_DONE) {
    cJSON_Delete(body);
    return analysis == ANALYSIS_REFUSED ? send_failure(conn, 400, refusal) : send_failure(conn, 500, "out of memory");
  }
  cJSON *item = NULL;
  UrsatzGuiMaterialResult result = ursatz_gui_material_item_create(type, name, project_id, version_body, &item);
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
  return send_object(conn, object);
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
    if (ursatz_gui_material_used_by(id, &used_by) != URSATZ_GUI_MATERIAL_OK || !object ||
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
    send_json_status(conn, 409, reason_for(409), text);
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
  const char *analysis_error = NULL;
  char refusal[512] = "";
  AnalysisOutcome analysis = analyze_motif_body((cJSON *)version_body, &analysis_error, refusal, sizeof(refusal));
  if (analysis != ANALYSIS_DONE) {
    cJSON_Delete(body);
    return analysis == ANALYSIS_REFUSED ? send_failure(conn, 400, refusal) : send_failure(conn, 500, "out of memory");
  }
  cJSON *item = NULL;
  UrsatzGuiMaterialResult result = ursatz_gui_material_item_add_version(id, version_body, note, &item);
  cJSON_Delete(body);
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
  return send_object(conn, object);
}

/* GET /api/materials/items/<id>/versions/<n>/score and .../playback, with ?key= for the key to sound it in (default:
   the motif's own key). Both render the version's motif.degrees with the Composer. */
static int handle_version_render(struct mg_connection *conn, const char *id, const char *number_text, bool playback_only) {
  char *end = NULL;
  long number = strtol(number_text, &end, 10);
  if (end == number_text || *end != '\0' || number < 1 || number > 1000000) {
    return send_failure(conn, 404, "version not found");
  }
  char render_key[32];
  bool has_render_key = query_value(conn, "key", render_key, sizeof(render_key));
  if (has_render_key && !analysis_key_ok(render_key)) {
    return send_failure(conn, 400, "key must look like D minor");
  }
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
  in.render_key = has_render_key ? render_key : NULL;
  if (!cJSON_IsObject(motif) || !in.degrees) {
    cJSON_Delete(version_body);
    return send_failure(conn, 404, "this version has no degrees to render");
  }
  if (!in.pickup) {
    in.pickup = "0";
  }
  char *xml = NULL;
  cJSON *playback = NULL;
  RenderFailure failure = {0, ""};
  bool ok = render_motif(&in, &xml, &playback, &failure);
  cJSON_Delete(version_body);
  if (!ok) {
    return send_failure(conn, failure.status, failure.message);
  }
  if (playback_only) {
    free(xml);
    return send_object(conn, playback);
  }
  size_t length = strlen(xml);
  mg_send_http_ok(conn, "application/xml", (long long)length);
  mg_write(conn, xml, length);
  cJSON_Delete(playback);
  free(xml);
  return 200;
}

/* ---- variants ---------------------------------------------------------- */

/* Runs the Composer with args and parses its JSON output. Returns NULL after setting *status and message (500 when the
   Composer cannot run, 400 when it refuses the input, which is its exit status 1 or 2). */
static cJSON *composer_json(const char *const *args, int *status, char *message, size_t message_size) {
  UrsatzGuiComposerOutput out = {0, NULL, NULL};
  UrsatzGuiComposerResult result = ursatz_gui_composer_run(args, &out);
  if (result != URSATZ_GUI_COMPOSER_OK) {
    *status = 500;
    snprintf(message, message_size, "%s",
             result == URSATZ_GUI_COMPOSER_ERR_NO_BINARY  ? "composer not found"
             : result == URSATZ_GUI_COMPOSER_ERR_TIMEOUT ? "composer timed out"
                                                          : "composer could not be started");
    return NULL;
  }
  cJSON *json = out.exit_status == 0 ? cJSON_Parse(out.stdout_text) : NULL;
  if (!json) {
    *status = out.exit_status == 1 || out.exit_status == 2 ? 400 : 500;
    snprintf(message, message_size, "%s", out.stderr_text[0] ? out.stderr_text : "composer could not make variants");
    size_t end = strlen(message);
    while (end > 0 && (message[end - 1] == '\n' || message[end - 1] == '\r')) {
      message[--end] = '\0';
    }
  }
  ursatz_gui_composer_output_destroy(&out);
  return json;
}

static cJSON *g_variant_operations = NULL;
#ifdef _WIN32
static SRWLOCK g_variant_operations_lock = SRWLOCK_INIT;
#define OPERATIONS_LOCK() AcquireSRWLockExclusive(&g_variant_operations_lock)
#define OPERATIONS_UNLOCK() ReleaseSRWLockExclusive(&g_variant_operations_lock)
#else
static pthread_mutex_t g_variant_operations_lock = PTHREAD_MUTEX_INITIALIZER;
#define OPERATIONS_LOCK() pthread_mutex_lock(&g_variant_operations_lock)
#define OPERATIONS_UNLOCK() pthread_mutex_unlock(&g_variant_operations_lock)
#endif

/* GET /api/materials/variant-operations: the Composer's operation list, read once and kept for the server's life. */
static int handle_variant_operations(struct mg_connection *conn) {
  OPERATIONS_LOCK();
  if (!g_variant_operations) {
    const char *args[] = {"variants", "--list-ops", "--json", NULL};
    int status = 0;
    char message[512];
    g_variant_operations = composer_json(args, &status, message, sizeof(message));
    if (!g_variant_operations) {
      OPERATIONS_UNLOCK();
      return send_failure(conn, status, message);
    }
  }
  cJSON *copy = cJSON_Duplicate(g_variant_operations, 1);
  OPERATIONS_UNLOCK();
  if (!copy || !cJSON_AddBoolToObject(copy, "ok", 1)) {
    cJSON_Delete(copy);
    return send_failure(conn, 500, "out of memory");
  }
  return send_object(conn, copy);
}

static int handle_update_use(struct mg_connection *conn, const char *id, const char *source_id) {
  cJSON *item = NULL;
  UrsatzGuiMaterialResult result = ursatz_gui_material_update_use(id, source_id, &item);
  if (result != URSATZ_GUI_MATERIAL_OK) {
    return send_material_failure(conn, result);
  }
  return send_ok_with(conn, "item", item);
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
  if (count == 4 && strcmp(segments[2], "versions") == 0) {
    return get ? handle_get_version(conn, segments[1], segments[3]) : send_method_not_allowed(conn);
  }
  if (count == 5 && strcmp(segments[2], "versions") == 0 &&
      (strcmp(segments[4], "score") == 0 || strcmp(segments[4], "playback") == 0)) {
    return get ? handle_version_render(conn, segments[1], segments[3], strcmp(segments[4], "playback") == 0)
               : send_method_not_allowed(conn);
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
  return send_object(conn, object);
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
  char segments[6][256];
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
  char segments[6][256];
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
  if (count == 1 && strcmp(segments[0], "render") == 0) {
    return strcmp(method, "POST") == 0 ? handle_render(conn) : send_method_not_allowed(conn);
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
