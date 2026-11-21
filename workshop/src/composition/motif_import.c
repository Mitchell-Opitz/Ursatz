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

UrsatzStatus workshop_motif_import(const unsigned char *bytes, size_t size, const char *meter, const char *key, cJSON **answer_out,
                                   int *http_status, UrsatzError *error) {
  *answer_out = NULL;
  cJSON *request = cJSON_CreateObject();
  if (!request || !cJSON_AddTrueToObject(request, "write_in_c") || (meter && !cJSON_AddStringToObject(request, "meter", meter)) ||
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
  snprintf(meter_text, sizeof(meter_text), "%d/%d", beats->valueint, unit->valueint);
  /* Motifs are written with C as the tonic (R-179): the Composer writes a key on another tonic in C in the same mode
     (notes_text_in_c); no key found reads the notes as C major. */
  const cJSON *key_text = cJSON_GetObjectItemCaseSensitive(analysis, "key_text");
  const cJSON *notes_in_c = cJSON_GetObjectItemCaseSensitive(analysis, "notes_text_in_c");
  bool keyed = cJSON_IsString(key_text) && cJSON_IsString(notes_in_c) && strchr(key_text->valuestring, ' ');
  const char *final_key = "C major";
  const char *moved_from = keyed && strncmp(key_text->valuestring, "C ", 2) != 0 ? key_text->valuestring : NULL;
  const char *final_notes = moved_from ? notes_in_c->valuestring : notes_text->valuestring;
  char c_key[16];
  if (keyed) {
    snprintf(c_key, sizeof(c_key), "C %s", strchr(key_text->valuestring, ' ') + 1);
    final_key = c_key;
  }
  cJSON *answer = cJSON_CreateObject();
  if (!answer) {
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
  cJSON_AddStringToObject(answer, "pickup", pickup->valuestring);
  cJSON *kept = cJSON_DetachItemFromObjectCaseSensitive(document, "analysis");
  cJSON_DeleteItemFromObjectCaseSensitive(kept, "key_text");
  cJSON_DeleteItemFromObjectCaseSensitive(kept, "notes_text_in_c");
  cJSON_AddItemToObject(answer, "analysis", kept);
  cJSON_Delete(document);
  *answer_out = answer;
  return URSATZ_OK;
}
