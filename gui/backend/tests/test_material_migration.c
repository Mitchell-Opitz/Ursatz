/* Confirms the GUI's migration of the old mode members (R-597): a motif, a theme and a piece stored the way an old store holds
 * them (a motif's key text "C minor", a theme form's and a piece structure's `mode` word, and the copies of the motifs in
 * their uses) are read with the member `material`, compose the same reply as the request the old members made, and are
 * saved with `material` and no old member. Also the key text a motif gives the Composer (ursatz_gui_motif_key_text).
 *
 * The test makes its own store in the build tree (URSATZ_GUI_TEST_STORE_DIR), writes the old version files over what the
 * service saved (a save writes the new members), and removes the store. */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <cJSON.h>

#include "service/knowledge_service.h"
#include "service/material_service.h"
#include "service/motif_service.h"
#include "service/piece_service.h"
#include "service/theme_service.h"
#include "gui_check.h"
#include "test_knowledge.h"

#define OK URSATZ_GUI_MATERIAL_OK
#define MINOR "omt.material.natural-minor"
#define MAJOR "omt.material.major"

static char g_dir[512];
/* The old motif body as the old route saved it: the analysis and the degrees the theme and the piece read, the key text. */
static char *g_old_motif;

static const char *const kOldMotif =
    "{\"uses\":[],\"motif\":{\"role\":\"melodic\",\"form\":\"full\",\"notes\":\"C4:e Eb4:e G4:e Eb4:e C4:e Eb4:e G4:e C5:e\",\"meter\":\"4/4\","
    "\"key\":\"C minor\",\"pickup\":\"0\",\"features\":[],\"analysis\":null,\"derived\":null}}";
/* The theme and the piece of the old store: the mode word, no material. */
static const char *const kOldTheme =
    "{\"uses\":[],\"theme\":{\"form\":{\"into\":\"sentence\",\"hybrid\":null,\"mode\":\"minor\",\"meter\":\"4/4\",\"bars\":8,"
    "\"cadence\":\"PAC\",\"first_cadence\":null},\"slots\":[{\"slot\":\"basic\",\"item_id\":\"%s\",\"version\":1}]}}";
static const char *const kOldPiece =
    "{\"uses\":[],\"piece\":{\"structure\":{\"form\":\"rounded-binary\",\"key\":\"C\",\"mode\":\"major\",\"meter\":\"4/4\"},"
    "\"motive\":{\"item_id\":\"%s\",\"version\":1}}}";

static const cJSON *member_of(const cJSON *object, const char *name) {
  return cJSON_GetObjectItemCaseSensitive(object, name);
}

static const char *str(const cJSON *object, const char *name) {
  const cJSON *item = member_of(object, name);
  return cJSON_IsString(item) ? item->valuestring : "";
}

/* A stored version file as it is on disk, with no migration (caller deletes). */
static cJSON *stored_body(const char *id, int number) {
  char path[700];
  snprintf(path, sizeof(path), "%s/items/%s/v%d.json", g_dir, id, number);
  char *text = test_read_file(path, NULL);
  cJSON *body = text ? cJSON_Parse(text) : NULL;
  free(text);
  return body;
}

/* Writes the old body (a copy of the motif in its uses, when motif_id is not NULL) over the version the service saved. */
static void store_old(const char *id, const char *old_text, const char *motif_id) {
  cJSON *old_file = cJSON_Parse(old_text);
  if (old_file && motif_id) {
    cJSON *use = cJSON_CreateObject();
    cJSON_AddStringToObject(use, "item_id", motif_id);
    cJSON_AddNumberToObject(use, "version", 1);
    cJSON_AddItemToObject(use, "copy", cJSON_Parse(g_old_motif));
    cJSON_AddItemToArray(cJSON_GetObjectItemCaseSensitive(old_file, "uses"), use);
  }
  char *text = old_file ? cJSON_Print(old_file) : NULL;
  char path[700];
  snprintf(path, sizeof(path), "%s/items/%s/v1.json", g_dir, id);
  check_quiet(text && test_write_file(path, text, strlen(text)), "an old version file is written");
  free(text);
  cJSON_Delete(old_file);
}

static char *new_item(const char *type, const char *name, const char *project_id, const char *body_text) {
  cJSON *body = cJSON_Parse(body_text);
  cJSON *item = NULL;
  char *id = NULL;
  if (ursatz_gui_material_item_create_noted(type, name, project_id, body, NULL, &item) == OK) {
    id = strdup(str(item, "id"));
  }
  cJSON_Delete(item);
  cJSON_Delete(body);
  return id;
}

/* The version body as the store reads it (caller deletes), NULL when it cannot be read. */
static cJSON *read_version(const char *id, int number) {
  cJSON *body = NULL;
  return ursatz_gui_material_item_get_version(id, number, &body) == OK ? body : NULL;
}

/* True when the body text of a version file (a path under the store) equals the Composer's bytes. */
static bool file_equals(const char *id, int number, const char *name, const void *bytes, size_t size) {
  char path[700];
  snprintf(path, sizeof(path), "%s/items/%s/v%d/%s", g_dir, id, number, name);
  size_t have = 0;
  char *text = test_read_file(path, &have);
  bool same = text && bytes && size > 0 && have == size && memcmp(text, bytes, size) == 0;
  free(text);
  return same;
}

/* --- the key text of a motif ------------------------------------------------ */

static void key_text_is(const char *motif_text, const char *expect, const char *what) {
  cJSON *motif = cJSON_Parse(motif_text);
  char out[64] = "";
  const char *text = ursatz_gui_motif_key_text(motif, out, sizeof(out));
  check_quiet(expect ? text != NULL && strcmp(text, expect) == 0 : text == NULL, what);
  cJSON_Delete(motif);
}

static void test_key_text(void) {
  key_text_is("{\"material\":\"" MINOR "\"}", "C minor", "a motif of the material natural minor writes in C minor");
  key_text_is("{\"material\":\"" MAJOR "\"}", "C major", "a motif of the material major writes in C major");
  key_text_is("{\"key\":\"C minor\"}", "C minor", "an old motif writes in its key text");
  key_text_is("{\"key\":\"C major\"}", "C major", "an old motif in C major writes in its key text");
  key_text_is("{\"material\":\"" MAJOR "\",\"key\":\"C minor\"}", "C major", "the material wins over the old key text");
  key_text_is("{\"material\":null,\"key\":\"C minor\"}", "C minor", "a null material leaves the old key text");
  key_text_is("{\"material\":null}", NULL, "a motif with a null material has no key text");
  key_text_is("{\"key\":null}", NULL, "a motif with a null key has no key text");
  key_text_is("{\"key\":\"\"}", NULL, "a motif with an empty key has no key text");
  key_text_is("{}", NULL, "a motif with neither has no key text");
  key_text_is("{\"material\":\"omt.material.nowhere\",\"key\":\"C minor\"}", NULL, "a material the Composer does not offer has no key text");
  key_text_is("{\"material\":\"style\"}", NULL, "the material style is not a key");

  /* The request a motif makes is the same for the old and the new member. */
  cJSON *old_body = cJSON_Parse(kOldMotif);
  cJSON *new_body = cJSON_Parse(kOldMotif);
  ursatz_gui_material_body_migrate(new_body);
  cJSON *old_request = ursatz_gui_motif_request(member_of(old_body, "motif"));
  cJSON *new_request = ursatz_gui_motif_request(member_of(new_body, "motif"));
  char *old_text = old_request ? cJSON_PrintUnformatted(old_request) : NULL;
  char *new_text = new_request ? cJSON_PrintUnformatted(new_request) : NULL;
  check_quiet(old_text && new_text && strcmp(old_text, new_text) == 0 && strstr(new_text, "C minor") != NULL,
              "the motive request of a motif with a material is the one of its old key text");
  free(old_text);
  free(new_text);
  cJSON_Delete(old_request);
  cJSON_Delete(new_request);
  cJSON_Delete(old_body);
  cJSON_Delete(new_body);
}

/* --- a motif ----------------------------------------------------------------- */

static void test_motif(const char *project, char **out_id) {
  char *motif = new_item("motif", "Old motif", project, g_old_motif);
  check_quiet(motif != NULL, "an old motif body is saved");
  *out_id = motif;
  if (!motif) {
    return;
  }
  cJSON *saved = stored_body(motif, 1);
  check_quiet(strcmp(str(member_of(saved, "motif"), "material"), MINOR) == 0 && !member_of(member_of(saved, "motif"), "key"),
              "a save writes the material and no key text");
  cJSON_Delete(saved);
  /* An old store holds the key text: write it as it was. */
  char path[700];
  snprintf(path, sizeof(path), "%s/items/%s/v1.json", g_dir, motif);
  check_quiet(test_write_file(path, g_old_motif, strlen(g_old_motif)), "the old motif file is written");
  saved = stored_body(motif, 1);
  check_quiet(strcmp(str(member_of(saved, "motif"), "key"), "C minor") == 0 && !member_of(member_of(saved, "motif"), "material"),
              "the old motif file holds the key text");

  cJSON *read = read_version(motif, 1);
  check_quiet(read && strcmp(str(member_of(read, "motif"), "material"), MINOR) == 0 && !member_of(member_of(read, "motif"), "key"),
              "the old motif is read with the material and no key text");

  /* It composes the same reply as before: the motive of the old body and of the read body. */
  UrsatzGuiComposerReply before;
  UrsatzGuiComposerReply after;
  memset(&before, 0, sizeof(before));
  memset(&after, 0, sizeof(after));
  cJSON *old_request = ursatz_gui_motif_request(member_of(saved, "motif"));
  cJSON *new_request = read ? ursatz_gui_motif_request(member_of(read, "motif")) : NULL;
  bool asked = old_request && new_request && ursatz_gui_motif_motive(old_request, &before) &&
               ursatz_gui_motif_motive(new_request, &after);
  check_quiet(asked && before.status == COMPOSER_OK && after.status == COMPOSER_OK && before.json && after.json &&
                  strcmp(before.json, after.json) == 0,
              "the old motif and the read motif have the same analysis");
  cJSON *reply = after.json ? cJSON_Parse(after.json) : NULL;
  check_quiet(reply && strcmp(str(member_of(member_of(reply, "motive"), "key"), "material"), MINOR) == 0,
              "the Composer names the material of the motif's key");
  cJSON_Delete(reply);
  ursatz_gui_composer_reply_destroy(&before);
  ursatz_gui_composer_reply_destroy(&after);
  cJSON_Delete(old_request);
  cJSON_Delete(new_request);

  /* A save of the read body writes the material, in a new version. */
  cJSON *added = NULL;
  check_quiet(read && ursatz_gui_material_item_add_version(motif, read, "Saved", &added) == OK, "the read motif is saved again");
  cJSON_Delete(added);
  cJSON *second = stored_body(motif, 2);
  check_quiet(strcmp(str(member_of(second, "motif"), "material"), MINOR) == 0 && !member_of(member_of(second, "motif"), "key"),
              "the new version holds the material and no key text");
  cJSON_Delete(second);
  cJSON *first = stored_body(motif, 1);
  check_quiet(strcmp(str(member_of(first, "motif"), "key"), "C minor") == 0, "version 1 stays as it was written");
  cJSON_Delete(first);
  cJSON_Delete(saved);
  cJSON_Delete(read);
}

/* --- a theme ----------------------------------------------------------------- */

/* The Composer's reply for the theme request, as the service asks for it (seed 1, no checks). */
static bool theme_reference(const cJSON *request, ComposerResponse *out) {
  cJSON *api = cJSON_Duplicate(request, 1);
  bool ok = api && cJSON_AddStringToObject(api, "seed", "1") && cJSON_AddBoolToObject(api, "check", 0) &&
            cJSON_AddBoolToObject(api, "midi", 1) && cJSON_AddBoolToObject(api, "musicxml", 1) &&
            cJSON_AddBoolToObject(api, "plain_midi", 0) && cJSON_AddBoolToObject(api, "plans", 0);
  char *text = ok ? cJSON_PrintUnformatted(api) : NULL;
  cJSON_Delete(api);
  memset(out, 0, sizeof(*out));
  ComposerStatus status = text ? composer_api_theme(ursatz_gui_composer_context(), text, NULL, out) : COMPOSER_ERR_ALLOC;
  free(text);
  return status == COMPOSER_OK && out->midi && out->musicxml;
}

static void test_theme(const char *project, const char *motif) {
  char old_text[1400];
  snprintf(old_text, sizeof(old_text), kOldTheme, motif);
  char *theme = new_item("theme", "Old theme", project, old_text);
  check_quiet(theme != NULL, "an old theme body is saved");
  if (!theme) {
    return;
  }
  cJSON *saved = stored_body(theme, 1);
  check_quiet(strcmp(str(member_of(member_of(saved, "theme"), "form"), "material"), MINOR) == 0 &&
                  !member_of(member_of(member_of(saved, "theme"), "form"), "mode"),
              "a save writes the form material and no mode");
  cJSON_Delete(saved);
  store_old(theme, old_text, motif);
  saved = stored_body(theme, 1);
  check_quiet(strcmp(str(member_of(member_of(saved, "theme"), "form"), "mode"), "minor") == 0 &&
                  !member_of(member_of(member_of(saved, "theme"), "form"), "material") &&
                  strcmp(str(member_of(member_of(member_of(saved, "uses")->child, "copy"), "motif"), "key"), "C minor") == 0,
              "the old theme file holds the mode word and the old copy of the motif");

  cJSON *read = read_version(theme, 1);
  const cJSON *form = member_of(member_of(read, "theme"), "form");
  check_quiet(read && strcmp(str(form, "material"), MINOR) == 0 && !member_of(form, "mode") && strcmp(str(form, "into"), "sentence") == 0,
              "the old theme is read with the form material and no mode");
  const cJSON *copy = member_of(member_of(read, "uses")->child, "copy");
  check_quiet(strcmp(str(member_of(copy, "motif"), "material"), MINOR) == 0 && !member_of(member_of(copy, "motif"), "key"),
              "the copy of the motif in the old theme is read with the material");

  /* The old body's request and the read body's request give the same score. */
  bool complete = false;
  cJSON *old_request = NULL;
  cJSON *new_request = NULL;
  char message[256];
  check_quiet(ursatz_gui_theme_validate(saved, &complete, &old_request, message, sizeof(message)) == URSATZ_GUI_THEME_OK && complete,
              "the old theme makes a request");
  complete = false;
  check_quiet(read && ursatz_gui_theme_validate(read, &complete, &new_request, message, sizeof(message)) == URSATZ_GUI_THEME_OK && complete,
              "the read theme makes a request");
  check_quiet(old_request && !member_of(old_request, "material") && strcmp(str(old_request, "mode"), "minor") == 0 &&
                  new_request && strcmp(str(new_request, "material"), MINOR) == 0 && !member_of(new_request, "mode"),
              "the old body asks with the mode word, the read body with the material");
  ComposerResponse before;
  ComposerResponse after;
  bool both = old_request && new_request && theme_reference(old_request, &before) && theme_reference(new_request, &after);
  check_quiet(both && before.midi_size == after.midi_size && memcmp(before.midi, after.midi, before.midi_size) == 0 &&
                  strcmp(before.musicxml, after.musicxml) == 0,
              "the old theme and the read theme compose the same MIDI and MusicXML");

  /* The service composes the read body: the same score, and the new version holds the material. */
  cJSON *item = NULL;
  UrsatzGuiThemeResult composed = ursatz_gui_theme_compose(theme, true, 1, false, NULL, &item, message, sizeof(message));
  check_quiet(composed == URSATZ_GUI_THEME_OK && both, "the old theme is composed by the service");
  check_quiet(both && file_equals(theme, 2, "theme.mid", before.midi, before.midi_size) &&
                  file_equals(theme, 2, "theme.musicxml", before.musicxml, strlen(before.musicxml)),
              "the composed version holds the score the old members made");
  cJSON *second = stored_body(theme, 2);
  const cJSON *second_form = member_of(member_of(second, "theme"), "form");
  check_quiet(strcmp(str(second_form, "material"), MINOR) == 0 && !member_of(second_form, "mode"),
              "the composed version holds the material and no mode");
  cJSON_Delete(second);
  cJSON_Delete(item);
  if (both) {
    composer_response_destroy(&before);
    composer_response_destroy(&after);
  }
  cJSON_Delete(old_request);
  cJSON_Delete(new_request);
  cJSON_Delete(saved);
  cJSON_Delete(read);
  free(theme);
}

/* --- a piece ----------------------------------------------------------------- */

static bool piece_reference(const cJSON *request, ComposerResponse *out) {
  cJSON *api = cJSON_CreateObject();
  bool ok = api && cJSON_AddItemToObject(api, "request", cJSON_Duplicate(request, 1)) && cJSON_AddStringToObject(api, "seed", "1") &&
            cJSON_AddBoolToObject(api, "check", 0) && cJSON_AddBoolToObject(api, "plain_midi", 0) && cJSON_AddBoolToObject(api, "midi", 1) &&
            cJSON_AddBoolToObject(api, "musicxml", 1) && cJSON_AddBoolToObject(api, "musicxml_versions", 1);
  char *text = ok ? cJSON_PrintUnformatted(api) : NULL;
  cJSON_Delete(api);
  memset(out, 0, sizeof(*out));
  ComposerStatus status = text ? composer_api_piece(ursatz_gui_composer_context(), text, NULL, out) : COMPOSER_ERR_ALLOC;
  free(text);
  return status == COMPOSER_OK && out->midi && out->musicxml;
}

static void test_piece(const char *project, const char *motif) {
  char old_text[1400];
  snprintf(old_text, sizeof(old_text), kOldPiece, motif);
  char *piece = new_item("piece", "Old piece", project, old_text);
  check_quiet(piece != NULL, "an old piece body is saved");
  if (!piece) {
    return;
  }
  cJSON *saved = stored_body(piece, 1);
  check_quiet(strcmp(str(member_of(member_of(saved, "piece"), "structure"), "material"), MAJOR) == 0 &&
                  !member_of(member_of(member_of(saved, "piece"), "structure"), "mode"),
              "a save writes the structure material and no mode");
  cJSON_Delete(saved);
  store_old(piece, old_text, motif);
  saved = stored_body(piece, 1);
  check_quiet(strcmp(str(member_of(member_of(saved, "piece"), "structure"), "mode"), "major") == 0 &&
                  !member_of(member_of(member_of(saved, "piece"), "structure"), "material"),
              "the old piece file holds the mode word");

  cJSON *read = read_version(piece, 1);
  const cJSON *structure = member_of(member_of(read, "piece"), "structure");
  check_quiet(read && strcmp(str(structure, "material"), MAJOR) == 0 && !member_of(structure, "mode") && strcmp(str(structure, "key"), "C") == 0 &&
                  strcmp(str(structure, "form"), "rounded-binary") == 0,
              "the old piece is read with the structure material and no mode");

  bool complete = false;
  cJSON *old_request = NULL;
  cJSON *new_request = NULL;
  char message[256];
  check_quiet(ursatz_gui_piece_validate(saved, &complete, &old_request, message, sizeof(message)) == URSATZ_GUI_THEME_OK && complete,
              "the old piece makes a request");
  complete = false;
  check_quiet(read && ursatz_gui_piece_validate(read, &complete, &new_request, message, sizeof(message)) == URSATZ_GUI_THEME_OK && complete,
              "the read piece makes a request");
  check_quiet(old_request && strcmp(str(old_request, "mode"), "major") == 0 && !member_of(old_request, "material") && new_request &&
                  strcmp(str(new_request, "material"), MAJOR) == 0 && !member_of(new_request, "mode"),
              "the old body asks with the mode word, the read body with the material");
  ComposerResponse before;
  ComposerResponse after;
  bool both = old_request && new_request && piece_reference(old_request, &before) && piece_reference(new_request, &after);
  check_quiet(both && before.midi_size == after.midi_size && memcmp(before.midi, after.midi, before.midi_size) == 0 &&
                  strcmp(before.musicxml, after.musicxml) == 0,
              "the old piece and the read piece compose the same MIDI and MusicXML");

  cJSON *item = NULL;
  UrsatzGuiThemeResult composed = ursatz_gui_piece_compose(piece, true, 1, false, NULL, &item, message, sizeof(message));
  check_quiet(composed == URSATZ_GUI_THEME_OK && both, "the old piece is composed by the service");
  check_quiet(both && file_equals(piece, 2, "piece.mid", before.midi, before.midi_size) &&
                  file_equals(piece, 2, "piece.musicxml", before.musicxml, strlen(before.musicxml)),
              "the composed version holds the score the old members made");
  cJSON *second = stored_body(piece, 2);
  const cJSON *second_structure = member_of(member_of(second, "piece"), "structure");
  check_quiet(strcmp(str(second_structure, "material"), MAJOR) == 0 && !member_of(second_structure, "mode"),
              "the composed version holds the material and no mode");
  cJSON_Delete(second);
  cJSON_Delete(item);
  if (both) {
    composer_response_destroy(&before);
    composer_response_destroy(&after);
  }
  cJSON_Delete(old_request);
  cJSON_Delete(new_request);
  cJSON_Delete(saved);
  cJSON_Delete(read);
  free(piece);
}

/* A stored progression fill is the section's harmony and an empty fill (R-610); a section's own harmony is kept. */
static void test_progression_fill(void) {
  static const char *const text =
      "{\"uses\":[],\"piece\":{\"structure\":{\"sections\":["
      "{\"id\":\"s0\",\"part\":\"body\",\"beats\":16,\"fill\":{\"kind\":\"progression\",\"numerals\":\"i iv V i\",\"cells\":2}},"
      "{\"id\":\"s1\",\"part\":\"body\",\"beats\":16,\"harmony\":{\"schema\":\"romanesca\"},"
      "\"fill\":{\"kind\":\"progression\",\"numerals\":\"I V\",\"cells\":1}},"
      "{\"id\":\"s2\",\"part\":\"body\",\"beats\":16,\"fill\":{\"kind\":\"empty\"}}]}}}";
  cJSON *body = cJSON_Parse(text);
  ursatz_gui_material_body_migrate(body);
  const cJSON *sections = member_of(member_of(member_of(body, "piece"), "structure"), "sections");
  const cJSON *first = cJSON_GetArrayItem(sections, 0);
  const cJSON *second = cJSON_GetArrayItem(sections, 1);
  const cJSON *third = cJSON_GetArrayItem(sections, 2);
  check_quiet(strcmp(str(member_of(first, "harmony"), "progression"), "i iv V i") == 0 &&
                  member_of(member_of(first, "harmony"), "cells") && member_of(member_of(first, "harmony"), "cells")->valuedouble == 2 &&
                  strcmp(str(member_of(first, "fill"), "kind"), "empty") == 0,
              "a progression fill is read as the section's harmony and an empty fill");
  check_quiet(strcmp(str(member_of(second, "harmony"), "schema"), "romanesca") == 0 && !member_of(member_of(second, "harmony"), "progression") &&
                  strcmp(str(member_of(second, "fill"), "kind"), "empty") == 0,
              "a section's own harmony is kept and the fill's numerals are dropped");
  check_quiet(!member_of(third, "harmony") && strcmp(str(member_of(third, "fill"), "kind"), "empty") == 0,
              "a section with no progression fill is left as it is");
  cJSON_Delete(body);
}

/* Removes the store: the items hold version folders the service deletes only with the item. */
static void remove_store(void) {
  ursatz_gui_material_service_shutdown();
  char command[600];
  snprintf(command, sizeof(command), "rm -rf '%s'", g_dir);
  if (system(command) != 0) {
    fprintf(stderr, "the test store could not be removed\n");
  }
}

int main(void) {
  test_knowledge();
  snprintf(g_dir, sizeof(g_dir), "%s/materials_%d", URSATZ_GUI_TEST_STORE_DIR, (int)getpid());
  check_quiet(ursatz_gui_material_service_init(g_dir), "init");

  test_key_text();

  /* The route analyzes a motif before it saves it (degrees, analysis); the old body is that, with the key text. */
  cJSON *old_body = cJSON_Parse(kOldMotif);
  char analysis_error[URSATZ_GUI_MOTIF_ANALYSIS_ERROR_SIZE];
  char refusal[256];
  char refusal_path[128];
  check_quiet(ursatz_gui_motif_analyze_body(old_body, NULL, analysis_error, refusal, sizeof(refusal), refusal_path, sizeof(refusal_path)) ==
                  URSATZ_GUI_MOTIF_ANALYSIS_DONE,
              "the old motif is analyzed");
  g_old_motif = cJSON_PrintUnformatted(old_body);
  cJSON_Delete(old_body);
  check_quiet(g_old_motif && strstr(g_old_motif, "\"key\":\"C minor\"") && strstr(g_old_motif, "\"degrees\""),
              "the old motif body has its key text and its degrees");

  cJSON *project = NULL;
  check_quiet(ursatz_gui_material_project_create("Migration", "", &project) == OK, "create a project");
  char *project_id = strdup(str(project, "id"));
  cJSON_Delete(project);
  char *motif = NULL;
  test_motif(project_id, &motif);
  if (motif) {
    test_theme(project_id, motif);
    test_piece(project_id, motif);
    test_progression_fill();
  }

  remove_store();
  free(g_old_motif);
  free(motif);
  free(project_id);
  return test_finish("gui_material_migration");
}
