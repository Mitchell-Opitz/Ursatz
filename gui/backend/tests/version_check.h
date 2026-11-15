#ifndef URSATZ_GUI_TEST_VERSION_CHECK_H
#define URSATZ_GUI_TEST_VERSION_CHECK_H

/* What the route tests check of the files a compose stored and of the playback a route serves (R-432). The tests do not
 * compare the Composer's answers with captures: a plan, a MIDI file and a MusicXML file are what they claim to be, and
 * a served playback is what the GUI's own playback service builds from the stored MIDI file.
 *
 * The including test defines check(cond, what) and exchange(method, url, body, &status) (the TCP client that answers
 * the body as JSON) as static functions; they are declared here and defined by the test. */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include "service/playback_service.h"

static void check(bool cond, const char *what);
static cJSON *exchange(const char *method, const char *path, const char *body, int *status);

/* The whole file, newly allocated and NUL terminated; its size goes to *size when that is not NULL. NULL when absent. */
static char *version_check_read(const char *path, size_t *size) {
  FILE *file = fopen(path, "rb");
  if (!file) {
    return NULL;
  }
  size_t capacity = 4096;
  size_t length = 0;
  char *buffer = malloc(capacity);
  while (buffer) {
    size_t n = fread(buffer + length, 1, capacity - length - 1, file);
    length += n;
    if (n == 0) {
      break;
    }
    if (length + 1 >= capacity) {
      char *grown = realloc(buffer, capacity * 2);
      if (!grown) {
        free(buffer);
        buffer = NULL;
        break;
      }
      buffer = grown;
      capacity *= 2;
    }
  }
  fclose(file);
  if (buffer) {
    buffer[length] = '\0';
    if (size) {
      *size = length;
    }
  }
  return buffer;
}

/* A Composer plan: JSON with "format": "ursatz-composer-plan". */
static bool check_plan_file(const char *path) {
  char *text = version_check_read(path, NULL);
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
  char *text = version_check_read(path, &size);
  bool ok = text && size > 14 && memcmp(text, "MThd", 4) == 0;
  free(text);
  return ok;
}

/* A MusicXML file: it holds a partwise score. */
static bool check_musicxml_file(const char *path) {
  char *text = version_check_read(path, NULL);
  bool ok = text && strstr(text, "<score-partwise") != NULL;
  free(text);
  return ok;
}

/* The playback the route serves at url equals, as JSON values, the playback service's own for the stored MIDI file. */
static void check_playback_of(const char *url, const char *midi_path, const char *what) {
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
