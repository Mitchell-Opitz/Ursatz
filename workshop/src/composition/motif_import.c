#include "composition/motif_import.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "composition/composer_client.h"
#include "composition/motifs.h"

/* Fills error and *http_status for a failure of the request itself. */
static UrsatzStatus refuse(int *http_status, int status, UrsatzStatus kind, UrsatzError *error, const char *path, const char *message) {
  *http_status = status;
  return ursatz_error_set(error, kind, path, "%s", message);
}

/* The key of a motive document as "D minor", or false when it has none. The tonic is spelled as the Composer spells it
   (pitch_class_names of its vocabulary). */
static bool import_key_text(const cJSON *document, char *out, size_t out_size) {
  const cJSON *key = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(document, "motive"), "key");
  const cJSON *tonic = cJSON_GetObjectItemCaseSensitive(key, "tonic_pitch_class");
  const cJSON *material = cJSON_GetObjectItemCaseSensitive(key, "material");
  char name[16];
  char word[32];
  if (!cJSON_IsNumber(tonic) || !cJSON_IsString(material) ||
      !workshop_composer_tonic_name(tonic->valueint, material->valuestring, name, sizeof(name)) ||
      !workshop_composer_key_word(material->valuestring, word, sizeof(word))) {
    return false;
  }
  snprintf(out, out_size, "%s %s", name, word);
  return true;
}

/* Writes a full motif's degree list as C notes in mode ("major" or "minor"): the Composer reads the degrees in "C <mode>" and answers
   analysis.notes_text. Returns the notes text (caller frees), or NULL with the failure in *http_status and error. The import moves a
   motif found on another tonic to C with it. */
static char *degrees_to_c_notes(const char *degrees, const char *mode, const char *meter, const char *pickup, int *http_status,
                                UrsatzStatus *status, UrsatzError *error) {
  size_t length = degrees ? strlen(degrees) : 0;
  if (length == 0 || length > 4000) {
    *status = refuse(http_status, 400, URSATZ_ERR_REQUEST, error, "", "degrees must be a list of 1 to 4000 characters");
    return NULL;
  }
  if (!mode) {
    *status = refuse(http_status, 400, URSATZ_ERR_REQUEST, error, "", "mode is needed");
    return NULL;
  }
  if (!meter || !pickup) {
    *status = refuse(http_status, 400, URSATZ_ERR_REQUEST, error, "", "meter and pickup are needed");
    return NULL;
  }
  char key[16];
  snprintf(key, sizeof(key), "C %s", mode);
  cJSON *request = workshop_motif_motive_request("degrees", NULL, degrees, meter, key, pickup);
  WorkshopComposerReply reply;
  bool called = workshop_motif_motive(request, &reply);
  cJSON_Delete(request);
  if (!called) {
    *status = refuse(http_status, 500, URSATZ_ERR_ALLOC, error, "", "out of memory");
    return NULL;
  }
  char *notes = NULL;
  if (workshop_composer_reply_refused(&reply)) {
    *status = refuse(http_status, workshop_composer_reply_failure_status(&reply), reply.status, error, reply.path,
           reply.message[0] ? reply.message : "composer could not read the degrees");
  } else {
    cJSON *document = reply.status == URSATZ_OK && reply.json ? cJSON_Parse(reply.json) : NULL;
    const cJSON *text = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(document, "analysis"), "notes_text");
    if (cJSON_IsString(text)) {
      notes = strdup(text->valuestring);
    }
    cJSON_Delete(document);
    if (!notes) {
      *status = refuse(http_status, 500, URSATZ_ERR_INTERNAL, error, "", "composer could not write the motif in C");
    }
  }
  workshop_composer_reply_destroy(&reply);
  return notes;
}

UrsatzStatus workshop_motif_import(const unsigned char *bytes, size_t size, const char *meter, const char *key, cJSON **answer_out,
                                   int *http_status, UrsatzError *error) {
  *answer_out = NULL;
  cJSON *request = cJSON_CreateObject();
  if (!request || (meter && !cJSON_AddStringToObject(request, "meter", meter)) ||
      (key && !cJSON_AddStringToObject(request, "key", key))) {
    cJSON_Delete(request);
    return refuse(http_status, 500, URSATZ_ERR_ALLOC, error, "", "out of memory");
  }
  WorkshopComposerReply reply;
  bool called = workshop_motif_motive_from_midi(bytes, size, request, &reply);
  cJSON_Delete(request);
  if (!called) {
    return refuse(http_status, 500, URSATZ_ERR_ALLOC, error, "", "out of memory");
  }
  if (reply.status != URSATZ_OK) {
    UrsatzStatus failed = refuse(http_status, workshop_composer_reply_failure_status(&reply), reply.status, error, reply.path,
                                 reply.message[0] ? reply.message : "composer could not read the MIDI file");
    workshop_composer_reply_destroy(&reply);
    return failed;
  }
  cJSON *document = reply.json ? cJSON_Parse(reply.json) : NULL;
  workshop_composer_reply_destroy(&reply);
  const cJSON *analysis = cJSON_GetObjectItemCaseSensitive(document, "analysis");
  const cJSON *notes_text = cJSON_GetObjectItemCaseSensitive(analysis, "notes_text");
  const cJSON *pickup = cJSON_GetObjectItemCaseSensitive(analysis, "pickup");
  const cJSON *bar = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(document, "motive"), "meter");
  const cJSON *beats = cJSON_GetObjectItemCaseSensitive(bar, "beats_per_bar");
  const cJSON *unit = cJSON_GetObjectItemCaseSensitive(bar, "beat_unit");
  if (!cJSON_IsString(notes_text) || !cJSON_IsString(pickup) || !cJSON_IsNumber(beats) || !cJSON_IsNumber(unit)) {
    cJSON_Delete(document);
    return refuse(http_status, 500, URSATZ_ERR_INTERNAL, error, "", "composer could not analyze the MIDI file");
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
  UrsatzStatus failed = URSATZ_ERR_INTERNAL;
  if (keyed) {
    const char *mode = strchr(key_text, ' ') + 1;
    snprintf(c_key, sizeof(c_key), "C %s", mode);
    final_key = c_key;
    if (strncmp(key_text, "C ", 2) != 0) {
      const cJSON *degrees_text = cJSON_GetObjectItemCaseSensitive(analysis, "degrees_text");
      moved_notes = degrees_to_c_notes(cJSON_IsString(degrees_text) ? degrees_text->valuestring : NULL, mode, meter_text,
                                       pickup->valuestring, http_status, &failed, error);
      if (!moved_notes) {
        cJSON_Delete(document);
        return failed;
      }
      final_notes = moved_notes;
      moved_from = key_text;
    }
  }
  cJSON *answer = cJSON_CreateObject();
  if (!answer) {
    free(moved_notes);
    cJSON_Delete(document);
    return refuse(http_status, 500, URSATZ_ERR_ALLOC, error, "", "out of memory");
  }
  cJSON_AddBoolToObject(answer, "ok", 1);
  cJSON_AddStringToObject(answer, "notes_text", final_notes);
  cJSON_AddStringToObject(answer, "meter", meter_text);
  cJSON_AddStringToObject(answer, "key", final_key);
  /* The material of the key the notes are written in (C major when the file names none). */
  const cJSON *found_material =
      cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(document, "motive"), "key"), "material");
  char material_id[64];
  if (keyed && cJSON_IsString(found_material)) {
    cJSON_AddStringToObject(answer, "material", found_material->valuestring);
  } else if (workshop_composer_material_of_word("major", material_id, sizeof(material_id))) {
    cJSON_AddStringToObject(answer, "material", material_id);
  }
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
  *answer_out = answer;
  return URSATZ_OK;
}
