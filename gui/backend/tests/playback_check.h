#ifndef URSATZ_GUI_TEST_PLAYBACK_CHECK_H
#define URSATZ_GUI_TEST_PLAYBACK_CHECK_H

/* Comparison of a served playback with one captured before P27 B9 (fixtures/playback). The notes (on, len, pitch,
 * part), the end and the tempo positions must be equal as values: the browser wrote [ticks, 4 * ticks per quarter], the
 * Analyzer writes the reduced fraction. Notes are compared as a set (the order of notes that start together, and so
 * their index i, differs). What else may differ is only what the phase declares (R-275):
 * tempo_source is the Analyzer's ("file" or "default", the browser said "given"), tempo values are rounded to
 * thousandths (the browser rounded to hundredths) and part names are the Analyzer's (empty for a MIDI file, which the
 * score workspace shows as "Part <n>"). */

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

/* [num, den] pairs equal as fractions. */
static bool playback_fraction_equal(const cJSON *a, const cJSON *b) {
  if (!cJSON_IsArray(a) || !cJSON_IsArray(b) || cJSON_GetArraySize(a) != 2 || cJSON_GetArraySize(b) != 2) {
    return false;
  }
  double a_num = cJSON_GetArrayItem(a, 0)->valuedouble;
  double a_den = cJSON_GetArrayItem(a, 1)->valuedouble;
  double b_num = cJSON_GetArrayItem(b, 0)->valuedouble;
  double b_den = cJSON_GetArrayItem(b, 1)->valuedouble;
  return a_den > 0 && b_den > 0 && a_num * b_den == b_num * a_den;
}

/* The same notes (on, len, pitch, part), in any order: the browser ordered notes that start together by pitch, the
 * Analyzer by part, and the index i follows the order. The player sounds notes by their time. */
static bool playback_notes_equal(const cJSON *got, const cJSON *want) {
  int count = cJSON_GetArraySize(want);
  if (!cJSON_IsArray(got) || cJSON_GetArraySize(got) != count) {
    return false;
  }
  bool *used = calloc((size_t)count + 1, sizeof(bool));
  bool all = used != NULL;
  for (int i = 0; all && i < count; i++) {
    const cJSON *b = cJSON_GetArrayItem(want, i);
    bool found = false;
    for (int j = 0; !found && j < count; j++) {
      const cJSON *a = cJSON_GetArrayItem(got, j);
      found = !used[j] &&
              playback_fraction_equal(cJSON_GetObjectItemCaseSensitive(a, "on"), cJSON_GetObjectItemCaseSensitive(b, "on")) &&
              playback_fraction_equal(cJSON_GetObjectItemCaseSensitive(a, "len"), cJSON_GetObjectItemCaseSensitive(b, "len")) &&
              cJSON_Compare(cJSON_GetObjectItemCaseSensitive(a, "pitch"), cJSON_GetObjectItemCaseSensitive(b, "pitch"), 1) &&
              cJSON_Compare(cJSON_GetObjectItemCaseSensitive(a, "part"), cJSON_GetObjectItemCaseSensitive(b, "part"), 1);
      used[j] = used[j] || found;
    }
    all = found;
  }
  free(used);
  return all;
}

static bool playback_matches_capture(const cJSON *got, const cJSON *want) {
  const cJSON *got_source = cJSON_GetObjectItemCaseSensitive(got, "tempo_source");
  if (!cJSON_IsString(got_source) ||
      (strcmp(got_source->valuestring, "file") != 0 && strcmp(got_source->valuestring, "default") != 0)) {
    return false;
  }
  if (!playback_notes_equal(cJSON_GetObjectItemCaseSensitive(got, "notes"), cJSON_GetObjectItemCaseSensitive(want, "notes")) ||
      !playback_fraction_equal(cJSON_GetObjectItemCaseSensitive(got, "end"), cJSON_GetObjectItemCaseSensitive(want, "end"))) {
    return false;
  }
  const cJSON *got_parts = cJSON_GetObjectItemCaseSensitive(got, "parts");
  const cJSON *want_parts = cJSON_GetObjectItemCaseSensitive(want, "parts");
  if (!cJSON_IsArray(got_parts) || cJSON_GetArraySize(got_parts) != cJSON_GetArraySize(want_parts)) {
    return false;
  }
  const cJSON *got_tempo = cJSON_GetObjectItemCaseSensitive(got, "tempo");
  const cJSON *want_tempo = cJSON_GetObjectItemCaseSensitive(want, "tempo");
  if (!cJSON_IsArray(got_tempo) || cJSON_GetArraySize(got_tempo) != cJSON_GetArraySize(want_tempo)) {
    return false;
  }
  for (int i = 0; i < cJSON_GetArraySize(want_tempo); i++) {
    const cJSON *a = cJSON_GetArrayItem(got_tempo, i);
    const cJSON *b = cJSON_GetArrayItem(want_tempo, i);
    const cJSON *a_qpm = cJSON_GetObjectItemCaseSensitive(a, "qpm");
    const cJSON *b_qpm = cJSON_GetObjectItemCaseSensitive(b, "qpm");
    if (!playback_fraction_equal(cJSON_GetObjectItemCaseSensitive(a, "at"), cJSON_GetObjectItemCaseSensitive(b, "at")) ||
        !cJSON_IsNumber(a_qpm) || !cJSON_IsNumber(b_qpm) || fabs(a_qpm->valuedouble - b_qpm->valuedouble) > 0.01) {
      return false;
    }
  }
  return true;
}

#endif /* URSATZ_GUI_TEST_PLAYBACK_CHECK_H */
