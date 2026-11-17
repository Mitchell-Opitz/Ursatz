#ifndef URSATZ_GUI_TEST_VERSION_CHECK_H
#define URSATZ_GUI_TEST_VERSION_CHECK_H

/* What the route tests check of the files a compose stored and of the playback a route serves (R-432). The tests do not
 * compare the Composer's answers with captures: a plan, a MIDI file and a MusicXML file are what they claim to be, and
 * a served playback is what the GUI's own playback service builds from the stored MIDI file.
 *
 * check_playback_of takes the exchange of the including test (its port bound; the HTTP client that answers the body as
 * JSON) as a function pointer; the checks are gui_check.h's, the files are read with the shared test support. */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include "gui_check.h"
#include "service/playback_service.h"

/* An exchange with a JSON body: the answer's body parsed as JSON (the caller deletes it), its status in *status. */
typedef cJSON *(*VersionCheckExchange)(const char *method, const char *path, const char *body, int *status);

/* A Composer plan: JSON with "format": "ursatz-composer-plan". */
static bool check_plan_file(const char *path) {
  char *text = test_read_file(path, NULL);
  cJSON *plan = text ? cJSON_Parse(text) : NULL;
  const cJSON *format = plan ? cJSON_GetObjectItemCaseSensitive(plan, "format") : NULL;
  bool ok = cJSON_IsString(format) && strcmp(format->valuestring, "ursatz-composer-plan") == 0;
  cJSON_Delete(plan);
  free(text);
  return ok;
}

/* A MIDI file: it starts with the header chunk. */
static bool check_midi_file(const char *path) {
  size_t size = 0;
  char *text = test_read_file(path, &size);
  bool ok = text && size > 14 && memcmp(text, "MThd", 4) == 0;
  free(text);
  return ok;
}

/* A MusicXML file: it holds a partwise score. */
static bool check_musicxml_file(const char *path) {
  char *text = test_read_file(path, NULL);
  bool ok = text && strstr(text, "<score-partwise") != NULL;
  free(text);
  return ok;
}

/* The playback the route serves at url equals, as JSON values, the playback service's own for the stored MIDI file. */
static void check_playback_of(VersionCheckExchange exchange, const char *url, const char *midi_path, const char *what) {
  int status = 0;
  char *json = NULL;
  cJSON *want = NULL;
  if (ursatz_gui_playback_of_midi(midi_path, &json) == URSATZ_GUI_PLAYBACK_OK && json) {
    want = cJSON_Parse(json);
  }
  free(json);
  cJSON *got = exchange("GET", url, NULL, &status);
  check(status == 200 && want && got && cJSON_Compare(got, want, 1), what);
  cJSON_Delete(want);
  cJSON_Delete(got);
}

#endif /* URSATZ_GUI_TEST_VERSION_CHECK_H */
