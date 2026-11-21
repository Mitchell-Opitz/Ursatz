#include "motifs.h"
#include "base/knowledge.h"
#include "base/last_error.h"
#include "analysis/styles.h"
#include "settings/settings_keys.h"
#include "settings/settings_resolve.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


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

typedef UrsatzStatus (*MotifEntry)(const ComposerContext *context, const char *request_json, const UrsatzJob *job,
                                     ComposerResponse *out);

/* Prints request, calls entry and moves the answer into reply. False when the request could not be printed. */
static bool motif_call(MotifEntry entry, const cJSON *request, WorkshopComposerReply *reply) {
  memset(reply, 0, sizeof(*reply));
  char *text = request ? cJSON_PrintUnformatted(request) : NULL;
  if (!text) {
    return false;
  }
  ComposerResponse response;
  memset(&response, 0, sizeof(response));
  UrsatzStatus status = entry(workshop_composer_context_global(), text, NULL, &response);
  free(text);
  workshop_composer_reply_take(status, &response, reply);
  return true;
}

bool workshop_motif_motive(const cJSON *request, WorkshopComposerReply *reply) {
  return motif_call(composer_api_motive, request, reply);
}

bool workshop_motif_motive_render(const cJSON *request, WorkshopComposerReply *reply) {
  return motif_call(composer_api_motive_render, request, reply);
}

bool workshop_motif_recognize(const cJSON *request, WorkshopComposerReply *reply) {
  return motif_call(composer_api_recognize, request, reply);
}

bool workshop_motif_random_motive(const cJSON *request, WorkshopComposerReply *reply) {
  return motif_call(composer_api_random_motive, request, reply);
}

bool workshop_motif_motive_from_midi(const unsigned char *bytes, size_t size, const cJSON *request,
                                       WorkshopComposerReply *reply) {
  memset(reply, 0, sizeof(*reply));
  char *text = request ? cJSON_PrintUnformatted(request) : NULL;
  if (!text) {
    return false;
  }
  ComposerResponse response;
  memset(&response, 0, sizeof(response));
  UrsatzStatus status = composer_api_motive_from_midi(workshop_composer_context_global(), bytes, size, text, NULL, &response);
  free(text);
  workshop_composer_reply_take(status, &response, reply);
  return true;
}

void workshop_motif_variant_operations(WorkshopComposerReply *reply) {
  ComposerResponse response;
  memset(&response, 0, sizeof(response));
  UrsatzStatus status = composer_api_variant_operations(&response);
  workshop_composer_reply_take(status, &response, reply);
}

/* Calls composer_api_validate for kind on request and moves the answer into reply. False when it could not be printed. */
bool workshop_motif_validate(const char *kind, const cJSON *request, WorkshopComposerReply *reply) {
  memset(reply, 0, sizeof(*reply));
  char *text = request ? cJSON_PrintUnformatted(request) : NULL;
  if (!text) {
    return false;
  }
  ComposerResponse response;
  memset(&response, 0, sizeof(response));
  UrsatzStatus status = composer_api_validate(workshop_composer_context_global(), kind, text, &response);
  free(text);
  workshop_composer_reply_take(status, &response, reply);
  return true;
}

/* ---- the requests of a motif ------------------------------------------- */

/* The harmony text of a motif member, NULL when it has none. */
/* The harmony text of a motif member, NULL when it has none. */
const char *workshop_motif_harmony(const cJSON *motif) {
  const cJSON *harmony = cJSON_GetObjectItemCaseSensitive(motif, "harmony");
  return cJSON_IsString(harmony) && harmony->valuestring[0] != '\0' ? harmony->valuestring : NULL;
}

/* The key text a motif writes its notes in ("C major"): C and the key word of its material, or the old key text of a body not
   migrated yet. NULL when it has neither. */
const char *workshop_motif_key_text(const cJSON *motif, char *out, size_t out_size) {
  const cJSON *material = cJSON_GetObjectItemCaseSensitive(motif, "material");
  const cJSON *key = cJSON_GetObjectItemCaseSensitive(motif, "key");
  char word[32];
  if (cJSON_IsString(material) && workshop_composer_key_word(material->valuestring, word, sizeof(word))) {
    snprintf(out, out_size, "C %s", word);
    return out;
  }
  if (!cJSON_IsString(material) && cJSON_IsString(key) && key->valuestring[0] != '\0') {
    snprintf(out, out_size, "%s", key->valuestring);
    return out;
  }
  return NULL;
}

/* The motive request of the Composer API: input.text, and input.format / input.form when given, meter, key (when not
   NULL) and pickup. NULL on allocation failure. */
cJSON *workshop_motif_motive_request(const char *format, const char *form, const char *text, const char *meter, const char *key,
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
   (one "label:first-last" per feature) and "role" (when not NULL). harmony and role may be overridden (a
   variant's own harmony, a render's role); NULL means none. False on allocation failure. */
bool workshop_motif_add_extras(cJSON *request, const char *harmony, const cJSON *features, const char *role) {
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
    if (!cJSON_IsString(label) || !cJSON_IsNumber(first) || !cJSON_IsNumber(last)) {
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

/* "tones" of a motif's tone_overrides (R-196): one "n=role" per override. False on
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
    if (!entry->string || !cJSON_IsString(entry)) {
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
cJSON *workshop_motif_request(const cJSON *motif) {
  const cJSON *form = cJSON_GetObjectItemCaseSensitive(motif, "form");
  const cJSON *notes = cJSON_GetObjectItemCaseSensitive(motif, "notes");
  const cJSON *meter = cJSON_GetObjectItemCaseSensitive(motif, "meter");
  char key_text[64];
  const char *key = workshop_motif_key_text(motif, key_text, sizeof(key_text));
  const cJSON *pickup = cJSON_GetObjectItemCaseSensitive(motif, "pickup");
  if (!cJSON_IsString(form) || !cJSON_IsString(notes) || !cJSON_IsString(meter) || !cJSON_IsString(pickup)) {
    return NULL;
  }
  bool has_key = key != NULL;
  bool full = strcmp(form->valuestring, "full") == 0;
  const cJSON *role = cJSON_GetObjectItemCaseSensitive(motif, "role");
  cJSON *request = workshop_motif_motive_request(full ? "notes" : NULL, full ? NULL : form->valuestring, notes->valuestring,
                                  meter->valuestring, key, pickup->valuestring);
  if (!request || !workshop_motif_add_extras(request, workshop_motif_harmony(motif), cJSON_GetObjectItemCaseSensitive(motif, "features"),
                                     cJSON_IsString(role) ? role->valuestring : NULL) ||
      (full && has_key && !add_tone_overrides(request, motif))) {
    cJSON_Delete(request);
    return NULL;
  }
  return request;
}

/* ---- variants ------------------------------------------------------------ */

/* Runs the Composer API `variants` on the motif member of a version body (its degrees, form, meter, key, pickup, harmony,
   features and role). chain (an object {"steps": [...]}) is optional and means random chains when NULL; seed is optional
   (negative: the clock). job reports progress and asks for cancel. On success returns the Composer's document; otherwise
   NULL with *status (400 when the Composer refuses the input, 499 for a cancel, else 500) and message set. */
cJSON *workshop_motif_run_variants(const cJSON *motif, const cJSON *basic_body, const cJSON *functions, const char *distance,
                           bool each_op, const cJSON *chain, int count, double seed, const UrsatzJob *job, int *status,
                           UrsatzError *error) {
  bool bad = false;
  const char *form = optional_string(motif, "form", &bad);
  const char *degrees = optional_string(motif, "degrees", &bad);
  const char *meter = optional_string(motif, "meter", &bad);
  char key_text[64];
  const char *key = workshop_motif_key_text(motif, key_text, sizeof(key_text));
  const char *pickup = optional_string(motif, "pickup", &bad);
  if (bad || !form || !degrees || degrees[0] == '\0' || !meter) {
    *status = 400;
    workshop_error_printf(error, "this motif has no degrees to vary");
    return NULL;
  }
  bool full = strcmp(form, "full") == 0;
  cJSON *request = workshop_motif_motive_request(full ? "degrees" : NULL, full ? NULL : form, degrees, meter,
                                  key, pickup);
  const char *role = optional_string(motif, "role", &(bool){false});
  bool ok = request && workshop_motif_add_extras(request, workshop_motif_harmony(motif), cJSON_GetObjectItemCaseSensitive(motif, "features"),
                                         role);
  /* The basic motive (the tree root) when this motif is derived: its list, pickup and harmony. */
  const cJSON *basic = cJSON_GetObjectItemCaseSensitive(basic_body, "motif");
  const char *basic_degrees = optional_string(basic, "degrees", &(bool){false});
  const char *basic_pickup = optional_string(basic, "pickup", &(bool){false});
  if (ok && basic_degrees && basic_degrees[0] != '\0') {
    cJSON *basic_request = workshop_motif_motive_request(full ? "degrees" : form, NULL, basic_degrees, NULL, NULL, basic_pickup);
    ok = basic_request && workshop_motif_add_extras(basic_request, workshop_motif_harmony(basic), NULL, NULL) &&
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
    workshop_error_printf(error, "out of memory");
    return NULL;
  }
  ComposerResponse response;
  memset(&response, 0, sizeof(response));
  UrsatzStatus composer_status = composer_api_variants(workshop_composer_context_global(), text, job, &response);
  free(text);
  WorkshopComposerReply reply;
  workshop_composer_reply_take(composer_status, &response, &reply);
  cJSON *document = reply.status == URSATZ_OK && reply.json ? cJSON_Parse(reply.json) : NULL;
  if (document) {
    /* the command printed the notices on stderr, not in the document */
    cJSON_DeleteItemFromObjectCaseSensitive(document, "notices");
  } else {
    *status = workshop_composer_reply_failure_status(&reply);
    workshop_error_printf(error, "%s", reply.message[0] ? reply.message : "composer could not make variants");
    if (error) {
      snprintf(error->path, sizeof(error->path), "%s", reply.path);
    }
  }
  workshop_composer_reply_destroy(&reply);
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

/* True when a string member differs between two motif members (a missing member equals an empty one). */
static bool member_text_differs(const cJSON *motif, const cJSON *previous, const char *name) {
  const cJSON *now = cJSON_GetObjectItemCaseSensitive(motif, name);
  const cJSON *before = cJSON_GetObjectItemCaseSensitive(previous, name);
  const char *now_text = cJSON_IsString(now) ? now->valuestring : "";
  const char *before_text = cJSON_IsString(before) ? before->valuestring : "";
  return strcmp(now_text, before_text) != 0;
}

/* True when the segments of a motif body are to be proposed by the Composer (R-312): it has no features, or its
   segments are still proposed (segments_proposed) and its notes, pickup, meter or form differ from the previous version.
   previous_body is the previous version's body, NULL when there is none (the segments then stay as they are). */
static bool segments_to_propose(const cJSON *motif, const cJSON *previous_body) {
  const cJSON *features = cJSON_GetObjectItemCaseSensitive(motif, "features");
  if (!cJSON_IsArray(features) || cJSON_GetArraySize(features) == 0) {
    return true;
  }
  if (!cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(motif, "segments_proposed")) || !previous_body) {
    return false;
  }
  const cJSON *previous = cJSON_GetObjectItemCaseSensitive(previous_body, "motif");
  return !cJSON_IsObject(previous) || member_text_differs(motif, previous, "notes") ||
         member_text_differs(motif, previous, "pickup") || member_text_differs(motif, previous, "meter") ||
         member_text_differs(motif, previous, "form");
}

/* Stores the proposed segments of an analysis document as motif.features with segments_proposed true. False on
   allocation failure; a document without segments leaves the motif as it is. */
static bool store_proposed_segments(cJSON *motif, const cJSON *document) {
  const cJSON *analysis = cJSON_GetObjectItemCaseSensitive(document, "analysis");
  const cJSON *segments = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(analysis, "segments"), "items");
  if (!cJSON_IsArray(segments)) {
    return true;
  }
  cJSON *features = cJSON_Duplicate(segments, 1);
  cJSON *flag = cJSON_CreateTrue();
  if (!features || !flag) {
    cJSON_Delete(features);
    cJSON_Delete(flag);
    return false;
  }
  set_motif_member(motif, "features", features);
  set_motif_member(motif, "segments_proposed", flag);
  return true;
}

/* ---- Composer API requests (R-219) ------------------------------------ */

/* Runs the Composer's motive analysis on a motif body (full, rhythm or contour) and stores the result as
   motif.analysis and its degree, rhythm or contour list as motif.degrees. A body without a valid motif is left alone
   for the material service to judge. When the Composer cannot run, the analysis and degrees are stored as null and
   analysis_error (WORKSHOP_MOTIF_ANALYSIS_ERROR_SIZE bytes, empty when analyzed) names the reason; the reason is also stored as
   motif.analysis_error, null when analyzed. When the Composer reads the input and refuses it, or a full motif has no key to
   write its degrees in, the result is REFUSED and refusal holds its message. */
WorkshopMotifAnalysis workshop_motif_analyze_body(cJSON *version_body, const cJSON *previous_body, char *analysis_error,
                                          char *refusal, size_t refusal_size, char *refusal_path, size_t refusal_path_size) {
  analysis_error[0] = '\0';
  cJSON *motif = cJSON_GetObjectItemCaseSensitive(version_body, "motif");
  if (!cJSON_IsObject(motif)) {
    return WORKSHOP_MOTIF_ANALYSIS_DONE;
  }
  const cJSON *form = cJSON_GetObjectItemCaseSensitive(motif, "form");
  const cJSON *notes = cJSON_GetObjectItemCaseSensitive(motif, "notes");
  const cJSON *meter = cJSON_GetObjectItemCaseSensitive(motif, "meter");
  const cJSON *pickup = cJSON_GetObjectItemCaseSensitive(motif, "pickup");
  if (!cJSON_IsString(form) ||
      (strcmp(form->valuestring, "full") != 0 && strcmp(form->valuestring, "rhythm") != 0 &&
       strcmp(form->valuestring, "contour") != 0)) {
    return WORKSHOP_MOTIF_ANALYSIS_DONE;
  }
  if (!cJSON_IsString(notes) || !cJSON_IsString(meter) || !cJSON_IsString(pickup)) {
    return WORKSHOP_MOTIF_ANALYSIS_DONE;
  }
  bool full = strcmp(form->valuestring, "full") == 0;
  /* Segments to propose: the request sends no features, and the proposal comes back as the motif's features. */
  bool propose = segments_to_propose(motif, previous_body);
  cJSON *kept_features = propose ? cJSON_DetachItemFromObjectCaseSensitive(motif, "features") : NULL;
  cJSON *request = workshop_motif_request(motif);
  if (kept_features) {
    cJSON_AddItemToObject(motif, "features", kept_features);
  }
  if (!request) {
    return WORKSHOP_MOTIF_ANALYSIS_ALLOC;
  }
  WorkshopComposerReply reply;
  bool called = workshop_motif_motive(request, &reply);
  cJSON_Delete(request);
  if (!called) {
    return WORKSHOP_MOTIF_ANALYSIS_ALLOC;
  }
  cJSON *analysis = NULL;
  bool refused = false;
  if (workshop_composer_reply_refused(&reply)) {
    refused = true;
    snprintf(refusal, refusal_size, "%s", reply.message[0] ? reply.message : "composer could not read the motif");
    snprintf(refusal_path, refusal_path_size, "%s", reply.path);
  } else {
    analysis = reply.status == URSATZ_OK && reply.json ? cJSON_Parse(reply.json) : NULL;
    if (!analysis) {
      snprintf(analysis_error, WORKSHOP_MOTIF_ANALYSIS_ERROR_SIZE, "composer could not analyze the motif%s%.300s",
               reply.message[0] ? ": " : "", reply.message);
    }
  }
  workshop_composer_reply_destroy(&reply);
  if (refused) {
    return WORKSHOP_MOTIF_ANALYSIS_REFUSED;
  }
  const char *degrees_text = analysis ? analysis_degrees_text(analysis, form->valuestring) : NULL;
  if (analysis && full && !degrees_text) {
    cJSON_Delete(analysis);
    snprintf(refusal, refusal_size, "give a key");
    refusal_path[0] = '\0';
    return WORKSHOP_MOTIF_ANALYSIS_REFUSED;
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
    return WORKSHOP_MOTIF_ANALYSIS_ALLOC;
  }
  bool stored = !propose || !cJSON_IsObject(analysis) || store_proposed_segments(motif, analysis);
  if (!stored) {
    cJSON_Delete(reason);
    cJSON_Delete(analysis);
    cJSON_Delete(degrees);
    return WORKSHOP_MOTIF_ANALYSIS_ALLOC;
  }
  set_motif_analysis(motif, analysis);
  set_motif_member(motif, "degrees", degrees);
  set_motif_member(motif, "analysis_error", reason);
  return WORKSHOP_MOTIF_ANALYSIS_DONE;
}

/* ---- rules of the routes' motif requests --------------------------------- */

/* Fills error and *http_status for a failure. */
static UrsatzStatus refuse(int *http_status, int status, UrsatzStatus kind, UrsatzError *error, const char *path, const char *message) {
  *http_status = status;
  return ursatz_error_set(error, kind, path, "%s", message);
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

/* A motif body is written with C as its tonic (R-179), so it names a material the Composer offers (the old key text "C major"
   or "C minor" is read as before). Bodies without a motif are left alone. */
bool workshop_motif_tonic_ok(const cJSON *version_body) {
  const cJSON *motif = cJSON_GetObjectItemCaseSensitive(version_body, "motif");
  if (!cJSON_IsObject(motif)) {
    return true;
  }
  const cJSON *material = cJSON_GetObjectItemCaseSensitive(motif, "material");
  if (material) {
    return cJSON_IsString(material) && workshop_composer_vocabulary_has("materials", material->valuestring);
  }
  const cJSON *key = cJSON_GetObjectItemCaseSensitive(motif, "key");
  return cJSON_IsString(key) && (strcmp(key->valuestring, "C major") == 0 || strcmp(key->valuestring, "C minor") == 0);
}

bool workshop_motif_version_note_ok(const char *type, const char *note) {
  /* Every new version of a motif or a theme says what changed. */
  return has_text(note) || !type || !(strcmp(type, "motif") == 0 || strcmp(type, "theme") == 0);
}

const char *workshop_motif_first_note(const char *type, const char *note) {
  /* A new motif's or theme's first version is noted "Created" unless the caller gives a note. */
  return has_text(note)                                             ? note
         : strcmp(type, "motif") == 0 || strcmp(type, "theme") == 0 ? "Created"
                                                                     : NULL;
}

UrsatzStatus workshop_motif_random(const cJSON *body, cJSON **document_out, int *http_status, UrsatzError *error) {
  *document_out = NULL;
  bool bad = false;
  const char *material = optional_string(body, "material", &bad);
  const char *old_mode = optional_string(body, "mode", &bad);
  const char *mode = material ? material : old_mode;
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
  bool valid = !bad && mode && !(material && old_mode) && meter &&
               (!count_item || cJSON_IsNumber(count_item)) && (!seed_item || cJSON_IsNumber(seed_item));
  if (!valid) {
    return refuse(http_status, 400, URSATZ_ERR_REQUEST, error, "",
                  "material and meter are required, and count and seed are numbers");
  }
  char style_path[1024] = "";
  if (style_id && workshop_style_model_path(style_id, style_path, sizeof(style_path)) != URSATZ_OK) {
    return refuse(http_status, 400, URSATZ_ERR_REQUEST, error, "", "unknown style");
  }
  cJSON *request = cJSON_CreateObject();
  bool built = request && cJSON_AddStringToObject(request, material ? "material" : "mode", mode) &&
               cJSON_AddStringToObject(request, "meter", meter) &&
               (!key || cJSON_AddStringToObject(request, "key", key)) &&
               (!form || cJSON_AddStringToObject(request, "form", form)) &&
               (!role || cJSON_AddStringToObject(request, "role", role)) &&
               (!style_id || cJSON_AddStringToObject(request, "style", style_path)) &&
               cJSON_AddNumberToObject(request, "count", count_item ? (int)count_item->valuedouble : 6) &&
               (!seed_item || cJSON_AddNumberToObject(request, "seed", seed_item->valuedouble));
  /* The settings (R-671): the project, the style, the taste profile, the switches and the request's own, resolved for a motif; a resolution with a
     document sends it to the Composer as the request's preferences and is answered as `settings`. With none set the request and the reply are as they were. */
  cJSON *resolution = NULL;
  if (built && workshop_settings_key_count() > 0) {
    WorkshopSettingsRequest resolve;
    memset(&resolve, 0, sizeof(resolve));
    const char *project_id = optional_string(body, "project_id", &bad);
    const char *taste = optional_string(body, "taste", &bad);
    const cJSON *values = cJSON_GetObjectItemCaseSensitive(body, "settings");
    const cJSON *off = cJSON_GetObjectItemCaseSensitive(body, "off");
    resolve.project_id = project_id && project_id[0] ? project_id : NULL;
    resolve.style_id = style_id;
    resolve.taste = taste && taste[0] ? taste : NULL;
    resolve.values = cJSON_IsObject(values) ? values : NULL;
    resolve.off = cJSON_IsArray(off) ? off : NULL;
    resolve.kind = "motif";
    UrsatzError failure = {{0}, {0}};
    UrsatzStatus resolved = bad ? URSATZ_ERR_REQUEST : workshop_settings_resolve_request(&resolve, false, &resolution, &failure);
    if (resolved != URSATZ_OK) {
      cJSON_Delete(request);
      return refuse(http_status, resolved == URSATZ_ERR_NOT_FOUND ? 404 : 400, resolved, error, failure.path,
                    failure.message[0] ? failure.message : "the settings are not valid");
    }
    const cJSON *document = cJSON_GetObjectItemCaseSensitive(resolution, "document");
    if (cJSON_IsObject(document)) {
      cJSON *copy = cJSON_Duplicate(document, true);
      if (!copy || !cJSON_AddItemToObject(request, "preferences", copy)) {
        cJSON_Delete(copy);
        cJSON_Delete(resolution);
        cJSON_Delete(request);
        return refuse(http_status, 500, URSATZ_ERR_ALLOC, error, "", "out of memory");
      }
    } else {
      cJSON_Delete(resolution);
      resolution = NULL;
    }
  }
  WorkshopComposerReply reply;
  bool called = built && workshop_motif_random_motive(request, &reply);
  cJSON_Delete(request);
  if (!called) {
    cJSON_Delete(resolution);
    return refuse(http_status, 500, URSATZ_ERR_ALLOC, error, "", "out of memory");
  }
  cJSON *document = reply.status == URSATZ_OK && reply.json ? cJSON_Parse(reply.json) : NULL;
  if (!document) {
    cJSON_Delete(resolution);
    UrsatzStatus failed = refuse(http_status, workshop_composer_reply_failure_status(&reply),
                                 reply.status != URSATZ_OK ? reply.status : URSATZ_ERR_INTERNAL, error, reply.path,
                                 reply.message[0] ? reply.message : "composer could not draw the motives");
    workshop_composer_reply_destroy(&reply);
    return failed;
  }
  workshop_composer_reply_destroy(&reply);
  if (!cJSON_AddBoolToObject(document, "ok", 1) || (resolution && !cJSON_AddItemToObject(document, "settings", resolution))) {
    cJSON_Delete(document);
    return refuse(http_status, 500, URSATZ_ERR_ALLOC, error, "", "out of memory");
  }
  resolution = NULL;
  *document_out = document;
  return URSATZ_OK;
}

