#include "motif_service.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MOTIF_FEATURES_MAX 8
#define TONE_OVERRIDES_MAX 64

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

/* ---- the Composer calls of a motif (R-219) ----------------------------- */

typedef ComposerStatus (*MotifEntry)(const char *request_json, const ComposerJob *job, ComposerResponse *out);

/* Prints request, calls entry and moves the answer into reply. False when the request could not be printed. */
static bool motif_call(MotifEntry entry, const cJSON *request, UrsatzGuiComposerReply *reply) {
  memset(reply, 0, sizeof(*reply));
  char *text = request ? cJSON_PrintUnformatted(request) : NULL;
  if (!text) {
    return false;
  }
  ComposerResponse response;
  memset(&response, 0, sizeof(response));
  ComposerStatus status = entry(text, NULL, &response);
  free(text);
  ursatz_gui_composer_reply_take(status, &response, reply);
  return true;
}

bool ursatz_gui_motif_motive(const cJSON *request, UrsatzGuiComposerReply *reply) {
  return motif_call(composer_api_motive, request, reply);
}

bool ursatz_gui_motif_motive_render(const cJSON *request, UrsatzGuiComposerReply *reply) {
  return motif_call(composer_api_motive_render, request, reply);
}

bool ursatz_gui_motif_recognize(const cJSON *request, UrsatzGuiComposerReply *reply) {
  return motif_call(composer_api_recognize, request, reply);
}

bool ursatz_gui_motif_random_motive(const cJSON *request, UrsatzGuiComposerReply *reply) {
  return motif_call(composer_api_random_motive, request, reply);
}

bool ursatz_gui_motif_motive_from_midi(const unsigned char *bytes, size_t size, const cJSON *request,
                                       UrsatzGuiComposerReply *reply) {
  memset(reply, 0, sizeof(*reply));
  char *text = request ? cJSON_PrintUnformatted(request) : NULL;
  if (!text) {
    return false;
  }
  ComposerResponse response;
  memset(&response, 0, sizeof(response));
  ComposerStatus status = composer_api_motive_from_midi(bytes, size, text, NULL, &response);
  free(text);
  ursatz_gui_composer_reply_take(status, &response, reply);
  return true;
}

void ursatz_gui_motif_variant_operations(UrsatzGuiComposerReply *reply) {
  ComposerResponse response;
  memset(&response, 0, sizeof(response));
  ComposerStatus status = composer_api_variant_operations(&response);
  ursatz_gui_composer_reply_take(status, &response, reply);
}

/* Calls composer_api_validate for kind on request and moves the answer into reply. False when it could not be printed. */
bool ursatz_gui_motif_validate(const char *kind, const cJSON *request, UrsatzGuiComposerReply *reply) {
  memset(reply, 0, sizeof(*reply));
  char *text = request ? cJSON_PrintUnformatted(request) : NULL;
  if (!text) {
    return false;
  }
  ComposerResponse response;
  memset(&response, 0, sizeof(response));
  ComposerStatus status = composer_api_validate(kind, text, &response);
  free(text);
  ursatz_gui_composer_reply_take(status, &response, reply);
  return true;
}

/* ---- the requests of a motif ------------------------------------------- */

/* The harmony text of a motif member, NULL when it has none. */
/* The harmony text of a motif member, NULL when it has none. */
const char *ursatz_gui_motif_harmony(const cJSON *motif) {
  const cJSON *harmony = cJSON_GetObjectItemCaseSensitive(motif, "harmony");
  return cJSON_IsString(harmony) && harmony->valuestring[0] != '\0' ? harmony->valuestring : NULL;
}

/* The role as a Composer option value, NULL for anything but melodic or accompaniment. */
/* The role as a Composer option value, NULL for anything but melodic or accompaniment. */
const char *ursatz_gui_motif_role_option(const char *role) {
  return role && (strcmp(role, "melodic") == 0 || strcmp(role, "accompaniment") == 0) ? role : NULL;
}

/* The motive request of the Composer API: input.text, and input.format / input.form when given, meter, key (when not
   NULL) and pickup. NULL on allocation failure. */
cJSON *ursatz_gui_motif_motive_request(const char *format, const char *form, const char *text, const char *meter, const char *key,
                             const char *pickup) {
  cJSON *request = cJSON_CreateObject();
  cJSON *input = request ? cJSON_AddObjectToObject(request, "input") : NULL;
  if (!input || !cJSON_AddStringToObject(input, "text", text) ||
      (format && !cJSON_AddStringToObject(input, "format", format)) ||
      (form && !cJSON_AddStringToObject(input, "form", form)) ||
      (meter && !cJSON_AddStringToObject(request, "meter", meter)) ||
      (key && !cJSON_AddStringToObject(request, "key", key)) ||
      (pickup && !cJSON_AddStringToObject(request, "pickup", pickup))) {
    cJSON_Delete(request);
    return NULL;
  }
  return request;
}

/* The harmony, features and role of a motif (R-140, R-141) as request members: "harmony" (a non-empty text), "features"
   (one "label:first-last" per feature, at most 8) and "role" (when not NULL). harmony and role may be overridden (a
   variant's own harmony, a render's role); NULL means none. False on allocation failure. */
bool ursatz_gui_motif_add_extras(cJSON *request, const char *harmony, const cJSON *features, const char *role) {
  if (harmony && harmony[0] != '\0' && !cJSON_AddStringToObject(request, "harmony", harmony)) {
    return false;
  }
  cJSON *list = cJSON_CreateArray();
  if (!list) {
    return false;
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
    char text[64];
    snprintf(text, sizeof(text), "%s:%d-%d", label->valuestring, first->valueint, last->valueint);
    cJSON *item = cJSON_CreateString(text);
    if (!item) {
      cJSON_Delete(list);
      return false;
    }
    cJSON_AddItemToArray(list, item);
    count++;
  }
  if (count == 0) {
    cJSON_Delete(list);
  } else if (!cJSON_AddItemToObject(request, "features", list)) {
    cJSON_Delete(list);
    return false;
  }
  return !role || role[0] == '\0' || cJSON_AddStringToObject(request, "role", role) != NULL;
}

/* "tones" of a motif's tone_overrides (R-196): one "n=role" per override, at most TONE_OVERRIDES_MAX. False on
   allocation failure. */
static bool add_tone_overrides(cJSON *request, const cJSON *motif) {
  const cJSON *overrides = cJSON_GetObjectItemCaseSensitive(motif, "tone_overrides");
  cJSON *list = cJSON_CreateArray();
  if (!list) {
    return false;
  }
  int count = 0;
  const cJSON *entry = NULL;
  cJSON_ArrayForEach(entry, overrides) {
    if (count == TONE_OVERRIDES_MAX || !entry->string || !cJSON_IsString(entry)) {
      break;
    }
    char text[40];
    snprintf(text, sizeof(text), "%s=%s", entry->string, entry->valuestring);
    cJSON *item = cJSON_CreateString(text);
    if (!item) {
      cJSON_Delete(list);
      return false;
    }
    cJSON_AddItemToArray(list, item);
    count++;
  }
  if (count == 0) {
    cJSON_Delete(list);
    return true;
  }
  if (!cJSON_AddItemToObject(request, "tones", list)) {
    cJSON_Delete(list);
    return false;
  }
  return true;
}

/* The motive request (composer_api_motive) of a motif: form full, rhythm or contour with notes, meter and pickup, its
   key when it has one, and its harmony, features, role and tone overrides. NULL on allocation failure or when the motif
   lacks the members of a request (a draft); nothing is judged here, the Composer refuses a value it does not read. */
cJSON *ursatz_gui_motif_request(const cJSON *motif) {
  const cJSON *form = cJSON_GetObjectItemCaseSensitive(motif, "form");
  const cJSON *notes = cJSON_GetObjectItemCaseSensitive(motif, "notes");
  const cJSON *meter = cJSON_GetObjectItemCaseSensitive(motif, "meter");
  const cJSON *key = cJSON_GetObjectItemCaseSensitive(motif, "key");
  const cJSON *pickup = cJSON_GetObjectItemCaseSensitive(motif, "pickup");
  if (!cJSON_IsString(form) || !cJSON_IsString(notes) || !cJSON_IsString(meter) || !cJSON_IsString(pickup)) {
    return NULL;
  }
  bool has_key = cJSON_IsString(key) && key->valuestring[0] != '\0';
  bool full = strcmp(form->valuestring, "full") == 0;
  const cJSON *role = cJSON_GetObjectItemCaseSensitive(motif, "role");
  cJSON *request = ursatz_gui_motif_motive_request(full ? "notes" : NULL, full ? NULL : form->valuestring, notes->valuestring,
                                  meter->valuestring, has_key ? key->valuestring : NULL, pickup->valuestring);
  if (!request || !ursatz_gui_motif_add_extras(request, ursatz_gui_motif_harmony(motif), cJSON_GetObjectItemCaseSensitive(motif, "features"),
                                     cJSON_IsString(role) ? role->valuestring : NULL) ||
      (full && has_key && !add_tone_overrides(request, motif))) {
    cJSON_Delete(request);
    return NULL;
  }
  return request;
}

/* ---- variants ------------------------------------------------------------ */

/* The request member at fault of the last refused ursatz_gui_motif_run_variants on this thread (a job runs on one
   thread), "" when none. */
static _Thread_local char g_variants_path[128];

const char *ursatz_gui_motif_variants_path(void) {
  return g_variants_path;
}

/* Runs the Composer API `variants` on the motif member of a version body (its degrees, form, meter, key, pickup, harmony,
   features and role). chain (an object {"steps": [...]}) is optional and means random chains when NULL; seed is optional
   (negative: the clock). job reports progress and asks for cancel. On success returns the Composer's document; otherwise
   NULL with *status (400 when the Composer refuses the input, 499 for a cancel, else 500) and message set. */
cJSON *ursatz_gui_motif_run_variants(const cJSON *motif, const cJSON *basic_body, const cJSON *functions, const char *distance,
                           bool each_op, const cJSON *chain, int count, double seed, const ComposerJob *job, int *status,
                           char *message, size_t message_size) {
  g_variants_path[0] = '\0';
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
  bool full = strcmp(form, "full") == 0;
  cJSON *request = ursatz_gui_motif_motive_request(full ? "degrees" : NULL, full ? NULL : form, degrees, meter,
                                  key && key[0] != '\0' ? key : NULL, pickup);
  const char *role = optional_string(motif, "role", &(bool){false});
  bool ok = request && ursatz_gui_motif_add_extras(request, ursatz_gui_motif_harmony(motif), cJSON_GetObjectItemCaseSensitive(motif, "features"),
                                         ursatz_gui_motif_role_option(role));
  /* The basic motive (the tree root) when this motif is derived: its list, pickup and harmony. */
  const cJSON *basic = cJSON_GetObjectItemCaseSensitive(basic_body, "motif");
  const char *basic_degrees = optional_string(basic, "degrees", &(bool){false});
  const char *basic_pickup = optional_string(basic, "pickup", &(bool){false});
  if (ok && basic_degrees && basic_degrees[0] != '\0') {
    cJSON *basic_request = ursatz_gui_motif_motive_request(full ? "degrees" : form, NULL, basic_degrees, NULL, NULL, basic_pickup);
    ok = basic_request && ursatz_gui_motif_add_extras(basic_request, ursatz_gui_motif_harmony(basic), NULL, NULL) &&
         cJSON_AddItemToObject(request, "basic", basic_request);
    if (!ok) {
      cJSON_Delete(basic_request);
    }
  }
  const cJSON *function = NULL;
  cJSON *function_list = ok && functions && cJSON_GetArraySize(functions) > 0 ? cJSON_AddArrayToObject(request, "functions") : NULL;
  cJSON_ArrayForEach(function, functions) {
    ok = ok && function_list && cJSON_AddItemToArray(function_list, cJSON_CreateString(function->valuestring));
  }
  char count_text[16];
  char seed_text[32];
  snprintf(count_text, sizeof(count_text), "%d", count);
  snprintf(seed_text, sizeof(seed_text), "%.0f", seed);
  ok = ok && (!distance || cJSON_AddStringToObject(request, "distance", distance)) &&
       (!chain || cJSON_AddItemToObject(request, "chain", cJSON_Duplicate(chain, 1))) &&
       /* One variant per operation in registry order; the Composer takes no count, chain, distance or function. */
       (each_op ? cJSON_AddTrueToObject(request, "each_op") != NULL : cJSON_AddStringToObject(request, "count", count_text) != NULL) &&
       (seed < 0 || cJSON_AddStringToObject(request, "seed", seed_text));
  char *text = ok ? cJSON_PrintUnformatted(request) : NULL;
  cJSON_Delete(request);
  if (!text) {
    *status = 500;
    snprintf(message, message_size, "out of memory");
    return NULL;
  }
  ComposerResponse response;
  memset(&response, 0, sizeof(response));
  ComposerStatus composer_status = composer_api_variants(text, job, &response);
  free(text);
  UrsatzGuiComposerReply reply;
  ursatz_gui_composer_reply_take(composer_status, &response, &reply);
  cJSON *document = reply.status == COMPOSER_OK && reply.json ? cJSON_Parse(reply.json) : NULL;
  if (document) {
    /* the command printed the notices on stderr, not in the document */
    cJSON_DeleteItemFromObjectCaseSensitive(document, "notices");
  } else {
    *status = ursatz_gui_composer_reply_failure_status(&reply);
    snprintf(message, message_size, "%s", reply.message[0] ? reply.message : "composer could not make variants");
    snprintf(g_variants_path, sizeof(g_variants_path), "%s", reply.path);
  }
  ursatz_gui_composer_reply_destroy(&reply);
  return document;
}

/* ---- analysis on save ---------------------------------------------------- */

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

/* The text the Composer returned for the form: degrees_text, rhythm_text or contour_text, or NULL. */
static const char *analysis_degrees_text(const cJSON *output, const char *form) {
  const cJSON *analysis = cJSON_GetObjectItemCaseSensitive(output, "analysis");
  const char *name = strcmp(form, "rhythm") == 0 ? "rhythm_text" : strcmp(form, "contour") == 0 ? "contour_text" : "degrees_text";
  const cJSON *text = cJSON_GetObjectItemCaseSensitive(analysis, name);
  return cJSON_IsString(text) ? text->valuestring : NULL;
}

/* ---- Composer API requests (R-219) ------------------------------------ */

/* Runs the Composer's motive analysis on a motif body (full, rhythm or contour) and stores the result as
   motif.analysis and its degree, rhythm or contour list as motif.degrees. A body without a valid motif is left alone
   for the material service to judge. When the Composer cannot run, the analysis and degrees are stored as null and
   analysis_error (URSATZ_GUI_MOTIF_ANALYSIS_ERROR_SIZE bytes, empty when analyzed) names the reason; the reason is also stored as
   motif.analysis_error, null when analyzed. When the Composer reads the input and refuses it, or a full motif has no key to
   write its degrees in, the result is REFUSED and refusal holds its message. */
UrsatzGuiMotifAnalysis ursatz_gui_motif_analyze_body(cJSON *version_body, char *analysis_error, char *refusal,
                                          size_t refusal_size, char *refusal_path, size_t refusal_path_size) {
  analysis_error[0] = '\0';
  cJSON *motif = cJSON_GetObjectItemCaseSensitive(version_body, "motif");
  if (!cJSON_IsObject(motif)) {
    return URSATZ_GUI_MOTIF_ANALYSIS_DONE;
  }
  const cJSON *form = cJSON_GetObjectItemCaseSensitive(motif, "form");
  const cJSON *notes = cJSON_GetObjectItemCaseSensitive(motif, "notes");
  const cJSON *meter = cJSON_GetObjectItemCaseSensitive(motif, "meter");
  const cJSON *key = cJSON_GetObjectItemCaseSensitive(motif, "key");
  const cJSON *pickup = cJSON_GetObjectItemCaseSensitive(motif, "pickup");
  if (!cJSON_IsString(form) ||
      (strcmp(form->valuestring, "full") != 0 && strcmp(form->valuestring, "rhythm") != 0 &&
       strcmp(form->valuestring, "contour") != 0)) {
    return URSATZ_GUI_MOTIF_ANALYSIS_DONE;
  }
  bool has_key = cJSON_IsString(key) && key->valuestring[0] != '\0';
  if (!cJSON_IsString(notes) || !cJSON_IsString(meter) || !cJSON_IsString(pickup)) {
    return URSATZ_GUI_MOTIF_ANALYSIS_DONE;
  }
  bool full = strcmp(form->valuestring, "full") == 0;
  cJSON *request = ursatz_gui_motif_request(motif);
  if (!request) {
    return URSATZ_GUI_MOTIF_ANALYSIS_ALLOC;
  }
  UrsatzGuiComposerReply reply;
  bool called = ursatz_gui_motif_motive(request, &reply);
  cJSON_Delete(request);
  if (!called) {
    return URSATZ_GUI_MOTIF_ANALYSIS_ALLOC;
  }
  cJSON *analysis = NULL;
  bool refused = false;
  if (ursatz_gui_composer_reply_refused(&reply)) {
    refused = true;
    snprintf(refusal, refusal_size, "%s", reply.message[0] ? reply.message : "composer could not read the motif");
    snprintf(refusal_path, refusal_path_size, "%s", reply.path);
  } else {
    analysis = reply.status == COMPOSER_OK && reply.json ? cJSON_Parse(reply.json) : NULL;
    if (!analysis) {
      snprintf(analysis_error, URSATZ_GUI_MOTIF_ANALYSIS_ERROR_SIZE, "composer could not analyze the motif%s%.300s",
               reply.message[0] ? ": " : "", reply.message);
    }
  }
  ursatz_gui_composer_reply_destroy(&reply);
  if (refused) {
    return URSATZ_GUI_MOTIF_ANALYSIS_REFUSED;
  }
  const char *degrees_text = analysis ? analysis_degrees_text(analysis, form->valuestring) : NULL;
  if (analysis && full && !degrees_text) {
    cJSON_Delete(analysis);
    snprintf(refusal, refusal_size, "give a key");
    refusal_path[0] = '\0';
    return URSATZ_GUI_MOTIF_ANALYSIS_REFUSED;
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
    return URSATZ_GUI_MOTIF_ANALYSIS_ALLOC;
  }
  set_motif_analysis(motif, analysis);
  set_motif_member(motif, "degrees", degrees);
  set_motif_member(motif, "analysis_error", reason);
  return URSATZ_GUI_MOTIF_ANALYSIS_DONE;
}

