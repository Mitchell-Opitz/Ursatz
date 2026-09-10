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

#include "../service/composer_service.h"
#include "../service/material_service.h"
#include "../service/style_service.h"
#include "../service/subject_service.h"
#include "../service/piece_service.h"
#include "../service/theme_service.h"

static const char *kMaterialsPrefix = "/api/materials";
static const char *kSubjectsPrefix = "/api/subjects";
static const size_t kMaxRequestBodySize = 256 * 1024;
static const int kMaxSegments = 7;

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
    case 422:
      return "Unprocessable Entity";
    case 500:
      return "Internal Server Error";
    case 503:
      return "Service Unavailable";
    case 504:
      return "Gateway Timeout";
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

#define ANALYSIS_ERROR_SIZE 400

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

/* The Composer options for a motif's harmony, features and role (R-140, R-141): --harmony "<text>", one --feature
   label:first-last per feature (at most 8) and --role. The feature texts live in extras, which the caller keeps until
   the Composer has run. harmony and role may be overridden (a variant's own harmony, a render's role); NULL means the
   motif's own. Returns the new argument count. */
#define MOTIF_FEATURES_MAX 8
typedef struct {
  char feature[MOTIF_FEATURES_MAX][64];
} MotifExtras;
#define MOTIF_EXTRA_ARGS (2 + 2 * MOTIF_FEATURES_MAX + 2)

static int append_extras_named(const char **args, int n, const char *harmony_option, const char *feature_option,
                               const char *harmony, const cJSON *features, const char *role, MotifExtras *extras) {
  if (harmony && harmony[0] != '\0') {
    args[n++] = harmony_option;
    args[n++] = harmony;
  }
  int count = 0;
  const cJSON *feature = NULL;
  cJSON_ArrayForEach(feature, features) {
    const cJSON *label = cJSON_GetObjectItemCaseSensitive(feature, "label");
    const cJSON *first = cJSON_GetObjectItemCaseSensitive(feature, "first");
    const cJSON *last = cJSON_GetObjectItemCaseSensitive(feature, "last");
    if (count == MOTIF_FEATURES_MAX || !cJSON_IsString(label) || !cJSON_IsNumber(first) || !cJSON_IsNumber(last)) {
      break;
    }
    snprintf(extras->feature[count], sizeof(extras->feature[count]), "%s:%d-%d", label->valuestring, first->valueint,
             last->valueint);
    args[n++] = feature_option;
    args[n++] = extras->feature[count++];
  }
  if (role && role[0] != '\0') {
    args[n++] = "--role";
    args[n++] = role;
  }
  return n;
}

static int append_motif_extras(const char **args, int n, const char *harmony, const cJSON *features, const char *role,
                               MotifExtras *extras) {
  return append_extras_named(args, n, "--harmony", "--feature", harmony, features, role, extras);
}

/* The harmony text of a motif member, NULL when it has none. */
static const char *motif_harmony(const cJSON *motif) {
  const cJSON *harmony = cJSON_GetObjectItemCaseSensitive(motif, "harmony");
  return cJSON_IsString(harmony) && harmony->valuestring[0] != '\0' ? harmony->valuestring : NULL;
}

/* The role as a Composer option value, NULL for anything but melodic or accompaniment. */
static const char *motif_role_option(const char *role) {
  return role && (strcmp(role, "melodic") == 0 || strcmp(role, "accompaniment") == 0) ? role : NULL;
}

/* "composer could not analyze the motif: <last non-empty stderr line>", the line cut at 300 characters. */
static void last_line_reason(char *reason, size_t size, const char *stderr_text) {
  const char *prefix = "composer could not analyze the motif";
  const char *end = stderr_text + strlen(stderr_text);
  while (end > stderr_text && isspace((unsigned char)end[-1])) {
    end--;
  }
  const char *start = end;
  while (start > stderr_text && start[-1] != '\n') {
    start--;
  }
  int length = (int)(end - start);
  if (length > 300) {
    length = 300;
  }
  if (length > 0) {
    snprintf(reason, size, "%s: %.*s", prefix, length, start);
  } else {
    snprintf(reason, size, "%s", prefix);
  }
}

/* Runs the Composer's motive analysis on a motif body (full, rhythm or contour) and stores the result as
   motif.analysis and its degree, rhythm or contour list as motif.degrees. A body without a valid motif is left alone
   for the material service to judge. When the Composer cannot run, the analysis and degrees are stored as null and
   analysis_error (ANALYSIS_ERROR_SIZE bytes, empty when analyzed) names the reason; the reason is also stored as
   motif.analysis_error, null when analyzed. When the Composer reads the input and refuses it, or a full motif has no key to
   write its degrees in, the result is REFUSED and refusal holds its message. */
static AnalysisOutcome analyze_motif_body(cJSON *version_body, char *analysis_error, char *refusal,
                                          size_t refusal_size) {
  analysis_error[0] = '\0';
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
    snprintf(analysis_error, ANALYSIS_ERROR_SIZE, "could not write the note list for the Composer");
    set_motif_analysis(motif, cJSON_CreateNull());
    set_motif_member(motif, "degrees", cJSON_CreateNull());
    set_motif_member(motif, "analysis_error", cJSON_CreateString(analysis_error));
    return ANALYSIS_DONE;
  }
  size_t length = strlen(notes->valuestring);
  bool written = write(fd, notes->valuestring, length) == (ssize_t)length;
  close(fd);
  cJSON *analysis = NULL;
  bool refused = false;
  if (!written) {
    snprintf(analysis_error, ANALYSIS_ERROR_SIZE, "could not write the note list for the Composer");
  } else {
    const char *args[14 + MOTIF_EXTRA_ARGS];
    MotifExtras extras;
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
    const cJSON *role = cJSON_GetObjectItemCaseSensitive(motif, "role");
    n = append_motif_extras(args, n, motif_harmony(motif), cJSON_GetObjectItemCaseSensitive(motif, "features"),
                            cJSON_IsString(role) ? role->valuestring : NULL, &extras);
    args[n++] = "--json";
    args[n] = NULL;
    UrsatzGuiComposerOutput out = {0, NULL, NULL};
    UrsatzGuiComposerResult result = ursatz_gui_composer_run(args, &out);
    if (result == URSATZ_GUI_COMPOSER_ERR_NO_BINARY) {
      snprintf(analysis_error, ANALYSIS_ERROR_SIZE, "composer not found");
    } else if (result == URSATZ_GUI_COMPOSER_ERR_TIMEOUT) {
      snprintf(analysis_error, ANALYSIS_ERROR_SIZE, "composer timed out");
    } else if (result != URSATZ_GUI_COMPOSER_OK) {
      snprintf(analysis_error, ANALYSIS_ERROR_SIZE, "composer could not be started");
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
        if (out.exit_status > 128) {
          snprintf(analysis_error, ANALYSIS_ERROR_SIZE, "composer crashed (signal %d)", out.exit_status - 128);
        } else {
          last_line_reason(analysis_error, ANALYSIS_ERROR_SIZE, out.stderr_text);
        }
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
  cJSON *reason = analysis_error[0] ? cJSON_CreateString(analysis_error) : cJSON_CreateNull();
  if (!analysis || !degrees || !reason) {
    cJSON_Delete(reason);
    cJSON_Delete(analysis);
    cJSON_Delete(degrees);
    return ANALYSIS_ALLOC;
  }
  set_motif_analysis(motif, analysis);
  set_motif_member(motif, "degrees", degrees);
  set_motif_member(motif, "analysis_error", reason);
  return ANALYSIS_DONE;
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

/* ---- degrees to C ------------------------------------------------------ */

typedef struct {
  int status;
  char message[512];
} ToCFailure;

static void to_c_fail(ToCFailure *failure, int status, const char *message) {
  failure->status = status;
  snprintf(failure->message, sizeof(failure->message), "%s", message);
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
  if (!mode || (strcmp(mode, "major") != 0 && strcmp(mode, "minor") != 0)) {
    to_c_fail(failure, 400, "mode must be major or minor");
    return NULL;
  }
  if (!meter || !analysis_meter_ok(meter) || !pickup || !analysis_pickup_ok(pickup)) {
    to_c_fail(failure, 400, "meter must look like 4/4 and pickup like 1 or 1/2");
    return NULL;
  }
  const char *tmp_dir = getenv("TMPDIR");
  char path[512];
  if (!tmp_dir || tmp_dir[0] != '/' || snprintf(path, sizeof(path), "%s/ursatz-to-c-XXXXXX", tmp_dir) >= (int)sizeof(path)) {
    snprintf(path, sizeof(path), "/tmp/ursatz-to-c-XXXXXX");
  }
  int fd = mkstemp(path);
  if (fd < 0) {
    to_c_fail(failure, 500, "could not write the list for the Composer");
    return NULL;
  }
  bool written = write(fd, degrees, length) == (ssize_t)length;
  close(fd);
  if (!written) {
    unlink(path);
    to_c_fail(failure, 500, "could not write the list for the Composer");
    return NULL;
  }
  char key[16];
  snprintf(key, sizeof(key), "C %s", mode);
  const char *args[14];
  int n = 0;
  args[n++] = "motive";
  args[n++] = path;
  args[n++] = "--format";
  args[n++] = "degrees";
  args[n++] = "--key";
  args[n++] = key;
  args[n++] = "--meter";
  args[n++] = meter;
  args[n++] = "--pickup";
  args[n++] = pickup;
  args[n++] = "--json";
  args[n] = NULL;
  UrsatzGuiComposerOutput out = {0, NULL, NULL};
  UrsatzGuiComposerResult result = ursatz_gui_composer_run(args, &out);
  unlink(path);
  if (result == URSATZ_GUI_COMPOSER_ERR_NO_BINARY) {
    to_c_fail(failure, 503, "composer not found");
    return NULL;
  }
  if (result == URSATZ_GUI_COMPOSER_ERR_TIMEOUT) {
    to_c_fail(failure, 504, "composer timed out");
    return NULL;
  }
  if (result != URSATZ_GUI_COMPOSER_OK) {
    to_c_fail(failure, 500, "composer could not be started");
    return NULL;
  }
  char *notes = NULL;
  if (out.exit_status == 1 || out.exit_status == 2) {
    char message[512];
    snprintf(message, sizeof(message), "%s", out.stderr_text[0] ? out.stderr_text : "composer could not read the degrees");
    size_t end = strlen(message);
    while (end > 0 && (message[end - 1] == '\n' || message[end - 1] == '\r')) {
      message[--end] = '\0';
    }
    to_c_fail(failure, 400, message);
  } else {
    cJSON *document = out.exit_status == 0 ? cJSON_Parse(out.stdout_text) : NULL;
    const cJSON *text = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(document, "analysis"), "notes_text");
    if (cJSON_IsString(text)) {
      notes = strdup(text->valuestring);
    }
    cJSON_Delete(document);
    if (!notes) {
      to_c_fail(failure, 500, "composer could not write the motif in C");
    }
  }
  ursatz_gui_composer_output_destroy(&out);
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
    return send_failure(conn, failure.status, failure.message);
  }
  cJSON *answer = cJSON_CreateObject();
  if (!answer || !cJSON_AddBoolToObject(answer, "ok", 1) || !cJSON_AddStringToObject(answer, "notes_text", notes)) {
    cJSON_Delete(answer);
    free(notes);
    return send_failure(conn, 500, "out of memory");
  }
  free(notes);
  return send_object(conn, answer);
}

/* POST /api/materials/motifs/import: the body is a MIDI file; ?meter= and ?key= are optional. Answers
   {"ok":true,"notes_text":...,"meter":"4/4","key":"C minor","moved_from":"D minor"|null,"pickup":"0",
   "analysis":{...}} and saves nothing. The notes are moved to C; with no key found they are read as C major. */
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
        return send_failure(conn, failure.status, failure.message);
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
  const char *harmony;
  const cJSON *features;
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
    const char *args[24 + MOTIF_EXTRA_ARGS];
    MotifExtras extras;
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
    n = append_motif_extras(args, n, in->harmony, in->features, motif_role_option(in->role), &extras);
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
  in.harmony = optional_string(body, "harmony", &bad);
  in.features = cJSON_GetObjectItemCaseSensitive(body, "features");
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
    return send_failure(conn, 400, "form, degrees, meter, key, pickup, render_key, role and harmony must be text and tempo a whole number");
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
  char analysis_error[ANALYSIS_ERROR_SIZE];
  char refusal[512] = "";
  AnalysisOutcome analysis = analyze_motif_body((cJSON *)version_body, analysis_error, refusal, sizeof(refusal));
  if (analysis != ANALYSIS_DONE) {
    cJSON_Delete(body);
    return analysis == ANALYSIS_REFUSED ? send_failure(conn, 400, refusal) : send_failure(conn, 500, "out of memory");
  }
  cJSON *item = NULL;
  /* A new motif's first version is noted "Created" unless the caller gives a note. */
  const char *first_note = has_text(note) ? note : strcmp(type, "motif") == 0 ? "Created" : NULL;
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
  /* Every new version of a motif says what changed. */
  if (cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(version_body, "motif")) && !has_text(note)) {
    cJSON_Delete(body);
    return send_failure(conn, 400, "give a version note");
  }
  if (!motif_tonic_ok(version_body)) {
    cJSON_Delete(body);
    return send_failure(conn, 400, "motifs are written with C as the tonic");
  }
  char analysis_error[ANALYSIS_ERROR_SIZE];
  char refusal[512] = "";
  AnalysisOutcome analysis = analyze_motif_body((cJSON *)version_body, analysis_error, refusal, sizeof(refusal));
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
  char analysis_error[ANALYSIS_ERROR_SIZE];
  char refusal[512] = "";
  AnalysisOutcome analysis = analyze_motif_body(version_body, analysis_error, refusal, sizeof(refusal));
  if (analysis != ANALYSIS_DONE) {
    cJSON_Delete(version_body);
    return analysis == ANALYSIS_REFUSED ? send_failure(conn, 400, refusal) : send_failure(conn, 500, "out of memory");
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
  return send_object(conn, object);
}

/* The answer for a Composer-backed call that did not succeed: the status follows the result (see theme_service.h). */
static int send_composer_failure(struct mg_connection *conn, UrsatzGuiThemeResult result, const char *message,
                                 const char *invalid_text, const char *failed_text) {
  switch (result) {
    case URSATZ_GUI_THEME_ERR_NOT_FOUND:
      return send_failure(conn, 404, message[0] ? message : "not found");
    case URSATZ_GUI_THEME_ERR_INVALID:
      return send_failure(conn, 400, message[0] ? message : invalid_text);
    case URSATZ_GUI_THEME_ERR_UNPROCESSABLE:
      return send_failure(conn, 422, message[0] ? message : "stuck");
    case URSATZ_GUI_THEME_ERR_NO_BINARY:
      return send_failure(conn, 503, "composer not found");
    case URSATZ_GUI_THEME_ERR_TIMEOUT:
      return send_failure(conn, 504, "composer timed out");
    default:
      return send_failure(conn, 500, message[0] ? message : failed_text);
  }
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
  cJSON *item = NULL;
  char message[512];
  long long chosen = has_seed ? (long long)seed->valuedouble : 0;
  UrsatzGuiThemeResult result =
      piece ? ursatz_gui_piece_compose(id, has_seed, chosen, cJSON_IsTrue(update_uses), &item, message, sizeof(message))
            : ursatz_gui_theme_compose(id, has_seed, chosen, cJSON_IsTrue(update_uses), &item, message, sizeof(message));
  cJSON_Delete(body);
  if (result == URSATZ_GUI_THEME_OK) {
    return send_ok_with(conn, "item", item);
  }
  return send_composer_failure(conn, result, message, piece ? "invalid piece" : "invalid theme",
                               piece ? "the piece could not be composed" : "the theme could not be composed");
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
  cJSON *item = NULL;
  char message[512];
  UrsatzGuiThemeResult result =
      ursatz_gui_piece_regenerate(id, version, section->valuestring, cJSON_IsString(lane) ? lane->valuestring : NULL, has_seed,
                                  seed, &item, message, sizeof(message));
  cJSON_Delete(body);
  if (result == URSATZ_GUI_THEME_OK) {
    return send_ok_with(conn, "item", item);
  }
  return send_composer_failure(conn, result, message, "invalid piece", "the section could not be written again");
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
  cJSON *made = NULL;
  char message[512];
  UrsatzGuiThemeResult result = ursatz_gui_piece_variations(id, version, section->valuestring, lane->valuestring,
                                                            (int)count->valuedouble, has_seed, seed, &made, message, sizeof(message));
  cJSON_Delete(body);
  if (result != URSATZ_GUI_THEME_OK) {
    return send_composer_failure(conn, result, message, "invalid piece", "the variations could not be made");
  }
  if (!cJSON_AddBoolToObject(made, "ok", 1)) {
    cJSON_Delete(made);
    return send_failure(conn, 500, "out of memory");
  }
  return send_object(conn, made);
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
  cJSON *item = NULL;
  char message[512];
  UrsatzGuiThemeResult result =
      ursatz_gui_piece_choose(id, version, set, (int)index->valuedouble, &item, message, sizeof(message));
  cJSON_Delete(body);
  if (result == URSATZ_GUI_THEME_OK) {
    return send_ok_with(conn, "item", item);
  }
  return send_composer_failure(conn, result, message, "invalid piece", "the variation could not be chosen");
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
  in.harmony = motif_harmony(motif);
  in.features = cJSON_GetObjectItemCaseSensitive(motif, "features");
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

/* Writes text to a new temp file named after stem; path receives its name. */
static bool write_temp_text(char *path, size_t size, const char *stem, const char *text) {
  if (!temp_file(path, size, stem)) {
    return false;
  }
  FILE *file = fopen(path, "wb");
  size_t length = strlen(text);
  bool written = file && fwrite(text, 1, length, file) == length;
  if (file && fclose(file) != 0) {
    written = false;
  }
  if (!written) {
    unlink(path);
  }
  return written;
}

/* The names --function takes; the same four the stored derived.functions hold. */
static bool function_name_ok(const char *name) {
  return strcmp(name, "beginning") == 0 || strcmp(name, "continuation") == 0 || strcmp(name, "contrast") == 0 ||
         strcmp(name, "cadential") == 0;
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

/* The largest seed the Composer takes: its cap is 4294967295 less the 16 variants of a run. */
static const double kMaxVariantSeed = 4294967279.0;

/* Runs `variants` on the motif member of a version body (its degrees, form, meter, key and pickup). chain (an object
   {"steps": [...]}) is optional and means random chains when NULL; seed is optional (negative: the clock). On success
   returns the Composer's document; otherwise NULL with *status and message set. */
static cJSON *run_variants(const cJSON *motif, const cJSON *basic_body, const cJSON *functions, const cJSON *chain,
                           int count, double seed, int *status, char *message, size_t message_size) {
  bool bad = false;
  const char *form = optional_string(motif, "form", &bad);
  const char *degrees = optional_string(motif, "degrees", &bad);
  const char *meter = optional_string(motif, "meter", &bad);
  const char *key = optional_string(motif, "key", &bad);
  const char *pickup = optional_string(motif, "pickup", &bad);
  if (bad || !form || !degrees || degrees[0] == '\0' || !meter) {
    *status = 400;
    snprintf(message, message_size, "this motif has no degrees to vary");
    return NULL;
  }
  char input_path[512];
  char chain_path[512] = "";
  char basic_path[512] = "";
  if (!write_temp_text(input_path, sizeof(input_path), "ursatz-variants", degrees)) {
    *status = 500;
    snprintf(message, message_size, "could not write the degrees for the Composer");
    return NULL;
  }
  bool full = strcmp(form, "full") == 0;
  /* The basic motive (the tree root) when this motif is derived: its list, pickup and harmony. */
  const cJSON *basic = cJSON_GetObjectItemCaseSensitive(basic_body, "motif");
  const char *basic_degrees = optional_string(basic, "degrees", &(bool){false});
  const char *basic_pickup = optional_string(basic, "pickup", &(bool){false});
  if (basic_degrees && basic_degrees[0] != '\0') {
    if (!write_temp_text(basic_path, sizeof(basic_path), "ursatz-basic", basic_degrees)) {
      unlink(input_path);
      *status = 500;
      snprintf(message, message_size, "could not write the basic motive for the Composer");
      return NULL;
    }
  }
  if (chain) {
    char *chain_text = cJSON_PrintUnformatted(chain);
    bool written = chain_text && write_temp_text(chain_path, sizeof(chain_path), "ursatz-chain", chain_text);
    free(chain_text);
    if (!written) {
      unlink(input_path);
      if (basic_path[0] != '\0') {
        unlink(basic_path);
      }
      *status = 500;
      snprintf(message, message_size, "could not write the chain for the Composer");
      return NULL;
    }
  }
  char count_text[16];
  char seed_text[32];
  snprintf(count_text, sizeof(count_text), "%d", count);
  snprintf(seed_text, sizeof(seed_text), "%.0f", seed);
  const char *args[40 + MOTIF_EXTRA_ARGS];
  MotifExtras extras;
  int n = 0;
  args[n++] = "variants";
  args[n++] = input_path;
  args[n++] = full ? "--format" : "--form";
  args[n++] = full ? "degrees" : form;
  args[n++] = "--meter";
  args[n++] = meter;
  if (key && key[0] != '\0') {
    args[n++] = "--key";
    args[n++] = key;
  }
  if (pickup) {
    args[n++] = "--pickup";
    args[n++] = pickup;
  }
  const char *role = optional_string(motif, "role", &(bool){false});
  n = append_motif_extras(args, n, motif_harmony(motif), cJSON_GetObjectItemCaseSensitive(motif, "features"),
                          motif_role_option(role), &extras);
  if (basic_path[0] != '\0') {
    args[n++] = "--basic";
    args[n++] = basic_path;
    args[n++] = "--basic-format";
    args[n++] = full ? "degrees" : form;
    if (basic_pickup) {
      args[n++] = "--basic-pickup";
      args[n++] = basic_pickup;
    }
    if (motif_harmony(basic)) {
      args[n++] = "--basic-harmony";
      args[n++] = motif_harmony(basic);
    }
  }
  const cJSON *function = NULL;
  int function_count = 0;
  cJSON_ArrayForEach(function, functions) {
    if (function_count++ == 4) {
      break;
    }
    args[n++] = "--function";
    args[n++] = function->valuestring;
  }
  if (chain) {
    args[n++] = "--chain";
    args[n++] = chain_path;
  }
  args[n++] = "--count";
  args[n++] = count_text;
  if (seed >= 0) {
    args[n++] = "--seed";
    args[n++] = seed_text;
  }
  args[n++] = "--json";
  args[n] = NULL;
  cJSON *document = composer_json(args, status, message, message_size);
  unlink(input_path);
  if (chain) {
    unlink(chain_path);
  }
  if (basic_path[0] != '\0') {
    unlink(basic_path);
  }
  return document;
}

/* POST /api/materials/items/<id>/variants {chain?, count?, seed?}: candidates from the item's current degrees; nothing is
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
  bool functions_ok = !functions || cJSON_IsArray(functions);
  const cJSON *function = NULL;
  cJSON_ArrayForEach(function, functions) {
    functions_ok = functions_ok && cJSON_IsString(function) && function_name_ok(function->valuestring);
  }
  functions_ok = functions_ok && (!functions || cJSON_GetArraySize(functions) <= 4);
  bool valid = functions_ok &&
               (!chain || (cJSON_IsObject(chain) && cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(chain, "steps")))) &&
               (!count_item || (cJSON_IsNumber(count_item) && count_item->valuedouble >= 1 &&
                                count_item->valuedouble <= 16 && count_item->valuedouble == (int)count_item->valuedouble)) &&
               (!seed_item || (cJSON_IsNumber(seed_item) && seed_item->valuedouble >= 0 &&
                               seed_item->valuedouble <= kMaxVariantSeed &&
                               seed_item->valuedouble == (double)(long long)seed_item->valuedouble));
  if (!valid) {
    cJSON_Delete(body);
    return send_failure(conn, 400, "chain is {steps: [...]}, count is 1 to 16, seed a whole number from 0 and functions at most 4 of beginning, continuation, contrast, cadential");
  }
  if (count_item) {
    count = (int)count_item->valuedouble;
  }
  if (seed_item) {
    seed = seed_item->valuedouble;
  }
  cJSON *item_body = NULL;
  UrsatzGuiMaterialResult result = ursatz_gui_material_item_get(id, NULL, &item_body);
  if (result != URSATZ_GUI_MATERIAL_OK) {
    cJSON_Delete(body);
    return send_material_failure(conn, result);
  }
  const cJSON *motif = cJSON_GetObjectItemCaseSensitive(item_body, "motif");
  char message[512];
  cJSON *basic_body = cJSON_IsObject(motif) ? lineage_root(motif) : NULL;
  cJSON *document = cJSON_IsObject(motif) ? run_variants(motif, basic_body, functions, chain, count, seed, &status,
                                                         message, sizeof(message))
                                          : NULL;
  cJSON_Delete(basic_body);
  if (!cJSON_IsObject(motif)) {
    status = 400;
    snprintf(message, sizeof(message), "only a motif has variants");
  }
  cJSON_Delete(body);
  cJSON_Delete(item_body);
  if (!document) {
    return send_failure(conn, status, message);
  }
  const cJSON *run_seed = cJSON_GetObjectItemCaseSensitive(document, "seed");
  if (!cJSON_AddBoolToObject(document, "ok", 1) ||
      (!run_seed && !cJSON_AddNumberToObject(document, "seed", seed >= 0 ? seed : 0))) {
    cJSON_Delete(document);
    return send_failure(conn, 500, "out of memory");
  }
  return send_object(conn, document);
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
  char source_path[512];
  char target_path[512];
  if (!write_temp_text(source_path, sizeof(source_path), "ursatz-recognize-source", source_degrees)) {
    cJSON_Delete(source_body);
    cJSON_Delete(target_body);
    return send_failure(conn, 500, "could not write the degrees for the Composer");
  }
  if (!write_temp_text(target_path, sizeof(target_path), "ursatz-recognize-target", target_degrees)) {
    unlink(source_path);
    cJSON_Delete(source_body);
    cJSON_Delete(target_body);
    return send_failure(conn, 500, "could not write the degrees for the Composer");
  }
  bool full = strcmp(form, "full") == 0;
  const char *args[16 + 2 * MOTIF_EXTRA_ARGS + 2];
  MotifExtras source_extras;
  MotifExtras target_extras;
  int n = 0;
  args[n++] = "recognize";
  args[n++] = source_path;
  args[n++] = target_path;
  args[n++] = full ? "--format" : "--form";
  args[n++] = full ? "degrees" : form;
  if (full) {
    args[n++] = "--target-format";
    args[n++] = "degrees";
  }
  args[n++] = "--meter";
  args[n++] = meter;
  if (key && key[0] != '\0') {
    args[n++] = "--key";
    args[n++] = key;
  }
  if (pickup) {
    args[n++] = "--pickup";
    args[n++] = pickup;
  }
  if (target_pickup) {
    args[n++] = "--target-pickup";
    args[n++] = target_pickup;
  }
  n = append_motif_extras(args, n, motif_harmony(source), cJSON_GetObjectItemCaseSensitive(source, "features"),
                          motif_role_option(optional_string(source, "role", &(bool){false})), &source_extras);
  n = append_extras_named(args, n, "--target-harmony", "--target-feature", motif_harmony(target),
                          cJSON_GetObjectItemCaseSensitive(target, "features"), NULL, &target_extras);
  args[n++] = "--json";
  args[n] = NULL;
  cJSON *document = composer_json(args, &status, message, sizeof(message));
  unlink(source_path);
  unlink(target_path);
  cJSON_Delete(source_body);
  cJSON_Delete(target_body);
  if (!document) {
    return send_failure(conn, status, message);
  }
  if (!cJSON_AddBoolToObject(document, "ok", 1)) {
    cJSON_Delete(document);
    return send_failure(conn, 500, "out of memory");
  }
  return send_object(conn, document);
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

/* Sets derived.harmony (the variant's own chords as text, or null), derived.functions (the names it suits) and
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
  return replace_member(derived, "harmony",
                        cJSON_IsString(harmony) && harmony->valuestring[0] != '\0' ? cJSON_CreateString(harmony->valuestring)
                                                                                    : cJSON_CreateNull()) &&
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
  const char *harmony = motif_harmony(target_motif);
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

/* POST /api/materials/items/<id>/reroll: reruns the stored chain on the source's stored version with a new seed and
   stores the result as a new version. */
static int handle_reroll(struct mg_connection *conn, const char *id) {
  cJSON *current = NULL;
  UrsatzGuiMaterialResult result = ursatz_gui_material_item_get(id, NULL, &current);
  if (result != URSATZ_GUI_MATERIAL_OK) {
    return send_material_failure(conn, result);
  }
  cJSON *motif = cJSON_GetObjectItemCaseSensitive(current, "motif");
  cJSON *derived = derived_of(current);
  const cJSON *source_item = cJSON_GetObjectItemCaseSensitive(derived, "source_item");
  const cJSON *source_version = cJSON_GetObjectItemCaseSensitive(derived, "source_version");
  const cJSON *stored_seed = cJSON_GetObjectItemCaseSensitive(derived, "seed");
  const cJSON *form_item = cJSON_GetObjectItemCaseSensitive(motif, "form");
  if (!derived || !cJSON_IsString(source_item) || !cJSON_IsNumber(source_version) || !cJSON_IsString(form_item)) {
    cJSON_Delete(current);
    return send_failure(conn, 400, "only a derived motif can be rerolled");
  }
  const char *form = form_item->valuestring;
  cJSON *source = NULL;
  result = ursatz_gui_material_item_get_version(source_item->valuestring, source_version->valueint, &source);
  if (result != URSATZ_GUI_MATERIAL_OK) {
    cJSON_Delete(current);
    return send_failure(conn, 404, "the source motif or its version is gone");
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
    seed = fmod(base + 1000003.0 + attempt, kMaxVariantSeed + 1);
    cJSON_Delete(document);
    document = run_variants(cJSON_GetObjectItemCaseSensitive(source, "motif"), basic_body, NULL, chain, 1, seed, &status,
                            message, sizeof(message));
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
    return same ? send_failure(conn, 409, "rerolling gives the same motif") : send_failure(conn, status, message);
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
    return send_material_failure(conn, result);
  }
  return send_ok_with(conn, "item", item);
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
   Composer; nothing is saved. The answer is the Composer's document plus "ok". */
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
  bool valid = !bad && mode && (!role || motif_role_option(role)) && (strcmp(mode, "major") == 0 || strcmp(mode, "minor") == 0) && meter &&
               analysis_meter_ok(meter) && (!key || analysis_key_ok(key)) &&
               (!form || strcmp(form, "full") == 0 || strcmp(form, "rhythm") == 0 || strcmp(form, "contour") == 0) &&
               (!count_item || (cJSON_IsNumber(count_item) && count_item->valuedouble >= 1 &&
                                count_item->valuedouble <= 16 && count_item->valuedouble == (int)count_item->valuedouble)) &&
               (!seed_item || (cJSON_IsNumber(seed_item) && seed_item->valuedouble >= 0 &&
                               seed_item->valuedouble <= kMaxVariantSeed &&
                               seed_item->valuedouble == (double)(long long)seed_item->valuedouble));
  if (!valid) {
    cJSON_Delete(body);
    return send_failure(conn, 400,
                        "mode is major or minor, meter like 3/4, key like D minor, form full, rhythm or contour, role melodic or accompaniment, "
                        "count 1 to 16 and seed a whole number from 0");
  }
  char style_path[1024] = "";
  if (style_id && ursatz_gui_style_model_path(style_id, style_path, sizeof(style_path)) != URSATZ_GUI_STYLE_OK) {
    cJSON_Delete(body);
    return send_failure(conn, 400, "unknown style");
  }
  char count_text[16];
  char seed_text[32];
  snprintf(count_text, sizeof(count_text), "%d", count_item ? (int)count_item->valuedouble : 6);
  snprintf(seed_text, sizeof(seed_text), "%.0f", seed_item ? seed_item->valuedouble : 0.0);
  const char *args[24];
  int n = 0;
  args[n++] = "random-motive";
  args[n++] = "--mode";
  args[n++] = mode;
  args[n++] = "--meter";
  args[n++] = meter;
  if (key) {
    args[n++] = "--key";
    args[n++] = key;
  }
  args[n++] = "--form";
  args[n++] = form ? form : "full";
  if (role) {
    args[n++] = "--role";
    args[n++] = role;
  }
  if (style_id) {
    args[n++] = "--style";
    args[n++] = style_path;
  }
  args[n++] = "--count";
  args[n++] = count_text;
  if (seed_item) {
    args[n++] = "--seed";
    args[n++] = seed_text;
  }
  args[n++] = "--json";
  args[n] = NULL;
  char message[512];
  cJSON *document = composer_json(args, &status, message, sizeof(message));
  cJSON_Delete(body);
  if (!document) {
    return send_failure(conn, status, message);
  }
  if (!cJSON_AddBoolToObject(document, "ok", 1)) {
    cJSON_Delete(document);
    return send_failure(conn, 500, "out of memory");
  }
  return send_object(conn, document);
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
  char segments[7][256];
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
  char segments[7][256];
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
