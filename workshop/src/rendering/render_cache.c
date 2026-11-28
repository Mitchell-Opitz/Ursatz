#include "rendering/render_cache.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <common/files.h>
#include <common/json.h>

#include "base/http_status.h"
#include "base/knowledge.h"
#include "base/last_error.h"
#include "composition/composer_client.h"
#include "composition/motifs.h"
#include "items/formats.h"
#include "items/layout.h"
#include "items/store.h"
#include "rendering/playback.h"
#include "base/params.h"

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

/* Fills error and *http_status for a failure. */
static UrsatzStatus refuse(int *http_status, int status, UrsatzStatus kind, UrsatzError *error, const char *path, const char *message) {
  *http_status = status;
  return ursatz_error_set(error, kind, path, "%s", message);
}

/* A store failure: the message the store left, or the fallback. */
static UrsatzStatus store_failure(UrsatzStatus status, int *http_status, UrsatzError *error) {
  const UrsatzError *last = workshop_last_error();
  *http_status = workshop_http_status(status, last);
  return ursatz_error_set(error, status, NULL, "%s", last->message[0] != '\0' ? last->message : "material store failure");
}

/* Renders a degree, rhythm or contour list in render_key with the Composer. On success *out_xml is the MusicXML text and *out_midi
   (*out_midi_size bytes) the render's MIDI file, both caller-owned; playback is built from the MIDI by the Analyzer (R-275). */
static UrsatzStatus render_motif(const RenderInput *in, char **out_xml, uint8_t **out_midi, size_t *out_midi_size, int *http_status,
                                 UrsatzError *error) {
  const char *form = in->form;
  if (!form) {
    return refuse(http_status, 400, URSATZ_ERR_REQUEST, error, "", "form is needed");
  }
  size_t degrees_length = in->degrees ? strlen(in->degrees) : 0;
  if (degrees_length == 0 || degrees_length > 4000) {
    return refuse(http_status, 400, URSATZ_ERR_REQUEST, error, "", "degrees must be a list of 1 to 4000 characters");
  }
  bool has_key = in->key && in->key[0] != '\0';
  const char *render_key = in->render_key && in->render_key[0] != '\0' ? in->render_key : (has_key ? in->key : NULL);
  if (!in->meter || !in->pickup) {
    return refuse(http_status, 400, URSATZ_ERR_REQUEST, error, "", "meter and pickup are needed");
  }
  if (!render_key) {
    return refuse(http_status, 400, URSATZ_ERR_REQUEST, error, "", "give a key");
  }
  bool full = strcmp(form, "full") == 0;
  cJSON *request = workshop_motif_motive_request(full ? "degrees" : form, form, in->degrees, in->meter, has_key ? in->key : NULL, in->pickup);
  if (!request || !workshop_motif_add_extras(request, in->harmony, in->features, in->role) ||
      !cJSON_AddStringToObject(request, "render_key", render_key) || !cJSON_AddTrueToObject(request, "musicxml") ||
      !cJSON_AddTrueToObject(request, "midi") ||
      (in->tempo && !cJSON_AddStringToObject(request, "tempo", in->tempo))) {
    cJSON_Delete(request);
    return refuse(http_status, 500, URSATZ_ERR_ALLOC, error, "", "out of memory");
  }
  WorkshopComposerReply reply;
  bool called = workshop_motif_motive_render(request, &reply);
  cJSON_Delete(request);
  if (!called) {
    return refuse(http_status, 500, URSATZ_ERR_ALLOC, error, "", "out of memory");
  }
  UrsatzStatus status = URSATZ_OK;
  if (workshop_composer_reply_refused(&reply)) {
    status = refuse(http_status, workshop_composer_reply_failure_status(&reply), reply.status, error, reply.path,
                    reply.message[0] ? reply.message : "composer could not render the motif");
  } else {
    char *xml = reply.status == URSATZ_OK && reply.musicxml && reply.midi ? strdup(reply.musicxml) : NULL;
    if (xml) {
      *out_xml = xml;
      *out_midi = reply.midi;
      *out_midi_size = reply.midi_size;
      reply.midi = NULL;
      reply.midi_size = 0;
    } else {
      status = refuse(http_status, 500, URSATZ_ERR_INTERNAL, error, "", "composer could not render the motif");
    }
  }
  workshop_composer_reply_destroy(&reply);
  return status;
}

/* The text of a refusal to build a playback; every one is a 500 (the file came from the Composer, not the request). */
static UrsatzStatus playback_failure(UrsatzStatus result, int *http_status, UrsatzError *error) {
  return refuse(http_status, 500, result == URSATZ_ERR_ALLOC ? URSATZ_ERR_ALLOC : URSATZ_ERR_INTERNAL, error, "",
                result == URSATZ_ERR_ALLOC ? "out of memory" : "could not build the playback from the MIDI file");
}

/* The playback JSON text the Analyzer built, parsed; frees json. */
static UrsatzStatus playback_tree(char *json, cJSON **playback, int *http_status, UrsatzError *error) {
  *playback = cJSON_Parse(json);
  free(json);
  return *playback ? URSATZ_OK
                   : refuse(http_status, 500, URSATZ_ERR_INTERNAL, error, "", "could not build the playback from the MIDI file");
}

/* The playback of rendered MIDI bytes: an unsaved render has no folder, so its MIDI goes to a temporary file for the Analyzer, which
   imports by path. */
static UrsatzStatus playback_of_bytes(const uint8_t *midi, size_t size, cJSON **playback, int *http_status, UrsatzError *error) {
  char tmp_dir[512];
  char *json = NULL;
  UrsatzStatus built = workshop_material_render_tmp_dir(tmp_dir, sizeof(tmp_dir))
                           ? workshop_playback_of_midi_bytes(midi, size, tmp_dir, &json)
                           : workshop_fail(URSATZ_ERR_IO, NULL);
  if (built != URSATZ_OK) {
    return playback_failure(built, http_status, error);
  }
  return playback_tree(json, playback, http_status, error);
}

UrsatzStatus workshop_motif_render(const cJSON *body, cJSON **answer_out, int *http_status, UrsatzError *error) {
  *answer_out = NULL;
  bool bad = false;
  RenderInput in = {0};
  in.form = workshop_param_string(body, "form", &bad);
  in.degrees = workshop_param_string(body, "degrees", &bad);
  in.meter = workshop_param_string(body, "meter", &bad);
  in.key = workshop_param_string(body, "key", &bad);
  in.pickup = workshop_param_string(body, "pickup", &bad);
  in.render_key = workshop_param_string(body, "render_key", &bad);
  in.role = workshop_param_string(body, "role", &bad);
  in.harmony = workshop_param_string(body, "harmony", &bad);
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
    return refuse(http_status, 400, URSATZ_ERR_REQUEST, error, "", "form, degrees, meter, key, pickup, render_key, role and harmony must be text and tempo a number or text like 195/2");
  }
  char *xml = NULL;
  uint8_t *midi = NULL;
  size_t midi_size = 0;
  UrsatzStatus status = render_motif(&in, &xml, &midi, &midi_size, http_status, error);
  if (status != URSATZ_OK) {
    return status;
  }
  cJSON *playback = NULL;
  status = playback_of_bytes(midi, midi_size, &playback, http_status, error);
  free(midi);
  if (status != URSATZ_OK) {
    free(xml);
    return status;
  }
  cJSON *answer = cJSON_CreateObject();
  if (!answer || !cJSON_AddBoolToObject(answer, "ok", 1) || !cJSON_AddStringToObject(answer, "musicxml", xml)) {
    cJSON_Delete(answer);
    cJSON_Delete(playback);
    free(xml);
    return refuse(http_status, 500, URSATZ_ERR_ALLOC, error, "", "out of memory");
  }
  free(xml);
  cJSON_AddItemToObject(answer, "playback", playback);
  *answer_out = answer;
  return URSATZ_OK;
}

UrsatzStatus workshop_progression_preview(const cJSON *request, cJSON **answer_out, int *http_status, UrsatzError *error) {
  *answer_out = NULL;
  cJSON *copy = cJSON_IsObject(request) ? cJSON_Duplicate(request, 1) : NULL;
  if (!copy) {
    return refuse(http_status, 400, URSATZ_ERR_REQUEST, error, "", "params is a progression request");
  }
  cJSON_DeleteItemFromObjectCaseSensitive(copy, "midi");
  cJSON_DeleteItemFromObjectCaseSensitive(copy, "musicxml");
  char *text = cJSON_AddTrueToObject(copy, "midi") && cJSON_AddTrueToObject(copy, "musicxml") ? cJSON_PrintUnformatted(copy) : NULL;
  cJSON_Delete(copy);
  if (!text) {
    return refuse(http_status, 500, URSATZ_ERR_ALLOC, error, "", "out of memory");
  }
  ComposerResponse response;
  memset(&response, 0, sizeof(response));
  UrsatzStatus composed = composer_api_progression(workshop_composer_context_global(), text, NULL, &response);
  free(text);
  WorkshopComposerReply reply;
  workshop_composer_reply_take(composed, &response, &reply);
  UrsatzStatus status = URSATZ_OK;
  cJSON *document = NULL;
  cJSON *playback = NULL;
  if (workshop_composer_reply_refused(&reply)) {
    status = refuse(http_status, workshop_composer_reply_failure_status(&reply), reply.status, error, reply.path,
                    reply.message[0] ? reply.message : "composer could not write the progression");
  } else if (reply.status != URSATZ_OK || !reply.json || !reply.midi || !reply.musicxml || !(document = cJSON_Parse(reply.json))) {
    status = refuse(http_status, 500, URSATZ_ERR_INTERNAL, error, "", "composer could not write the progression");
  } else {
    status = playback_of_bytes(reply.midi, reply.midi_size, &playback, http_status, error);
  }
  if (status == URSATZ_OK && (!cJSON_AddBoolToObject(document, "ok", 1) || !cJSON_AddStringToObject(document, "musicxml", reply.musicxml) ||
                              !cJSON_AddItemToObject(document, "playback", playback))) {
    cJSON_Delete(playback);
    status = refuse(http_status, 500, URSATZ_ERR_ALLOC, error, "", "out of memory");
  }
  workshop_composer_reply_destroy(&reply);
  if (status != URSATZ_OK) {
    cJSON_Delete(document);
    return status;
  }
  *answer_out = document;
  return URSATZ_OK;
}

/* The render cache of a motif version, see render_cache.h. */
static bool render_cache_dir(const char *id, int number, char *out, size_t out_size) {
  return workshop_layout_render_dir(id, number, out, out_size);
}

static bool render_cache_file(const char *dir, const char *name, char *out, size_t out_size) {
  return common_path_join(out, out_size, dir, name);
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
  cJSON *meta = NULL;
  workshop_format_read(path, WORKSHOP_FORMAT_RENDER, true, &meta, NULL);
  WorkshopComposerBuild build = workshop_composer_build();
  const cJSON *key = cJSON_GetObjectItemCaseSensitive(meta, "render_key");
  const cJSON *cached_tempo = cJSON_GetObjectItemCaseSensitive(meta, "tempo");
  bool same = render_meta_is(meta, "api_version", build.api_version) &&
              render_meta_is(meta, "composer_version", build.composer_version) &&
              render_meta_is(meta, "composition_key", build.composition_key) && cJSON_IsString(key) &&
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
  char *text = NULL;
  common_file_read(path, &text, NULL, NULL);
  return text;
}

/* Stores a render. The files go first and render.json last, so a half-written cache never matches. A failure is
   silent: the next request renders again. */
static void render_cache_write(const char *id, int number, const char *render_key, int tempo, const char *xml,
                               const uint8_t *midi, size_t midi_size) {
  char dir[576];
  char path[640];
  if (!render_cache_dir(id, number, dir, sizeof(dir)) || !(common_dir_make(dir, NULL) == URSATZ_OK)) {
    return;
  }
  cJSON *meta = cJSON_CreateObject();
  if (!meta) {
    return;
  }
  WorkshopComposerBuild build = workshop_composer_build();
  cJSON_AddStringToObject(meta, "api_version", build.api_version);
  cJSON_AddStringToObject(meta, "composer_version", build.composer_version);
  cJSON_AddStringToObject(meta, "composition_key", build.composition_key);
  cJSON_AddStringToObject(meta, "render_key", render_key);
  cJSON_AddNumberToObject(meta, "tempo", tempo);
  if (render_cache_file(dir, "score.musicxml", path, sizeof(path)) && (common_file_write_atomic(path, xml, strlen(xml), NULL) == URSATZ_OK) &&
      render_cache_file(dir, "motif.mid", path, sizeof(path)) && (common_file_write_atomic(path, midi, midi_size, NULL) == URSATZ_OK) &&
      render_cache_file(dir, "render.json", path, sizeof(path))) {
    (void)workshop_format_write(path, WORKSHOP_FORMAT_RENDER, meta, NULL);
  }
  cJSON_Delete(meta);
}

/* The playback of a theme version's MIDI file or of the MIDI file of a version of a piece's form (loop, ending, piece...), built by
   the Analyzer (R-275, R-430). */
static UrsatzStatus version_file_playback(const char *id, int number, const char *file, cJSON **playback, int *http_status,
                                          UrsatzError *error) {
  char name[48];
  char dir[512];
  char path[640];
  cJSON *version_body = NULL;
  if (strcmp(file, "theme") != 0 && !workshop_material_version_name_ok(file)) {
    return refuse(http_status, 400, URSATZ_ERR_REQUEST, error, "",
                  "file must be theme or a version name of a lowercase letter, then up to 30 lowercase letters, digits or hyphens");
  }
  snprintf(name, sizeof(name), "%s.mid", file);
  UrsatzStatus result = workshop_material_item_get_version(id, number, &version_body);
  if (result != URSATZ_OK) {
    return store_failure(result, http_status, error);
  }
  cJSON_Delete(version_body);
  FILE *exists = workshop_material_version_dir(id, number, dir, sizeof(dir)) &&
                         common_path_join(path, sizeof(path), dir, name)
                     ? fopen(path, "rb")
                     : NULL;
  if (!exists) {
    return refuse(http_status, 404, URSATZ_ERR_NOT_FOUND, error, "", "file not found");
  }
  fclose(exists);
  char *json = NULL;
  UrsatzStatus built = workshop_playback_of_midi(path, &json);
  if (built != URSATZ_OK) {
    return playback_failure(built, http_status, error);
  }
  return playback_tree(json, playback, http_status, error);
}

UrsatzStatus workshop_motif_version_render(const char *id, int number, const char *render_key_in, bool playback_only, const char *file,
                                           char **musicxml, cJSON **playback, int *http_status, UrsatzError *error) {
  *musicxml = NULL;
  *playback = NULL;
  if (playback_only && file) {
    return version_file_playback(id, number, file, playback, http_status, error);
  }
  cJSON *version_body = NULL;
  UrsatzStatus result = workshop_material_item_get_version(id, number, &version_body);
  if (result != URSATZ_OK) {
    return store_failure(result, http_status, error);
  }
  const cJSON *motif = cJSON_GetObjectItemCaseSensitive(version_body, "motif");
  RenderInput in = {0};
  in.form = workshop_param_string(motif, "form", &(bool){false});
  in.degrees = workshop_param_string(motif, "degrees", &(bool){false});
  in.meter = workshop_param_string(motif, "meter", &(bool){false});
  char motif_key[64];
  in.key = workshop_motif_key_text(motif, motif_key, sizeof(motif_key));
  in.pickup = workshop_param_string(motif, "pickup", &(bool){false});
  in.role = workshop_param_string(version_body, "role", &(bool){false});
  if (!in.role) {
    in.role = workshop_param_string(motif, "role", &(bool){false});
  }
  in.harmony = workshop_motif_harmony(motif);
  in.features = cJSON_GetObjectItemCaseSensitive(motif, "features");
  in.render_key = render_key_in;
  if (!cJSON_IsObject(motif) || !in.degrees) {
    cJSON_Delete(version_body);
    return refuse(http_status, 404, URSATZ_ERR_NOT_FOUND, error, "", "this version has no degrees to render");
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
  bool cached = effective_key && render_cache_dir(id, number, cache_dir, sizeof(cache_dir)) &&
                render_cache_matches(cache_dir, effective_key, 0) &&
                snprintf(midi_path, sizeof(midi_path), "%s/motif.mid", cache_dir) < (int)sizeof(midi_path);
  if (cached && !playback_only) {
    char *score = render_cache_read_score(id, number, effective_key, 0);
    if (score) {
      cJSON_Delete(version_body);
      *musicxml = score;
      return URSATZ_OK;
    }
  }
  if (cached && playback_only) {
    char *json = NULL;
    if (workshop_playback_of_midi(midi_path, &json) == URSATZ_OK) {
      cJSON_Delete(version_body);
      return playback_tree(json, playback, http_status, error);
    }
  }
  char *xml = NULL;
  uint8_t *midi = NULL;
  size_t midi_size = 0;
  UrsatzStatus status = render_motif(&in, &xml, &midi, &midi_size, http_status, error);
  cJSON_Delete(version_body);
  if (status != URSATZ_OK) {
    return status;
  }
  if (effective_key) {
    render_cache_write(id, number, effective_key, 0, xml, midi, midi_size);
  }
  if (playback_only) {
    status = playback_of_bytes(midi, midi_size, playback, http_status, error);
    free(xml);
  } else {
    *musicxml = xml;
  }
  free(midi);
  return status;
}
