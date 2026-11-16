/* The replies for a piece's detail (note ids), score (MusicXML and its meta) and playback, compared with the
 * snapshot in fixtures/pieces taken before the Analyzer produced them (B7). JSON is compared as values, MusicXML
 * byte for byte. Set URSATZ_GUI_SNAPSHOT_CAPTURE=1 to rewrite the snapshot from the current replies. */

#include <civetweb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#include <unistd.h>
#define MKDIR(path) mkdir(path, 0755)
#endif

#include <cJSON.h>

#include "server.h"
#include "gui_check.h"
#include "test_http.h"

#ifndef URSATZ_GUI_TEST_PIECE_FIXTURES
#error "URSATZ_GUI_TEST_PIECE_FIXTURES must name fixtures/pieces"
#endif

#define PORT "18775"

static bool capture = false;

/* GET route for the piece. The body is returned (caller frees) when the status is 200. */
static char *get_body(const char *id, const char *suffix) {
  char path[256];
  snprintf(path, sizeof(path), "/api/analyzer/pieces/%s%s", id, suffix);
  char *response = test_http_exchange(PORT, "GET", path, NULL, NULL, 0);
  char *body = test_http_status(response) == 200 ? strdup(test_http_body(response)) : NULL;
  free(response);
  return body;
}

/* Compares text with fixtures/pieces/<name>/<file>, or writes it in capture mode. JSON is compared as values. */
static void compare(const char *name, const char *file, const char *text, bool json) {
  char path[768];
  char what[256];
  snprintf(path, sizeof(path), "%s/%s", URSATZ_GUI_TEST_PIECE_FIXTURES, name);
  if (capture) {
    MKDIR(path);
  }
  snprintf(path, sizeof(path), "%s/%s/%s", URSATZ_GUI_TEST_PIECE_FIXTURES, name, file);
  snprintf(what, sizeof(what), "%s: %s equals the snapshot", name, file);
  if (!text) {
    check(false, what);
    return;
  }
  if (capture) {
    check(test_write_file(path, text, strlen(text)), what);
    return;
  }
  char *expected = test_read_file(path, NULL);
  bool same = false;
  if (expected && json) {
    cJSON *a = cJSON_Parse(expected);
    cJSON *b = cJSON_Parse(text);
    same = a && b && cJSON_Compare(a, b, true);
    cJSON_Delete(a);
    cJSON_Delete(b);
  } else if (expected) {
    same = strcmp(expected, text) == 0;
  }
  free(expected);
  check(same, what);
}

static void snapshot_piece(const char *name, const char *source_path, const char *encoded_filename) {
  char *id = test_http_upload(PORT, source_path, encoded_filename, NULL);
  char what[256];
  snprintf(what, sizeof(what), "%s uploads", name);
  check(id != NULL, what);
  if (!id) {
    return;
  }

  char *detail_text = get_body(id, "");
  cJSON *detail = detail_text ? cJSON_Parse(detail_text) : NULL;
  cJSON *ids = detail ? cJSON_DetachItemFromObjectCaseSensitive(detail, "note_xml_ids") : NULL;
  char *ids_text = ids ? cJSON_PrintUnformatted(ids) : NULL;
  compare(name, "note_xml_ids.json", ids_text, true);
  free(ids_text);
  cJSON_Delete(ids);
  cJSON_Delete(detail);
  free(detail_text);

  char *score = get_body(id, "/musicxml");
  compare(name, "score.musicxml", score, false);
  free(score);
  char *meta = get_body(id, "/musicxml/meta");
  compare(name, "score-meta.json", meta, true);
  free(meta);
  char *playback = get_body(id, "/playback");
  compare(name, "playback.json", playback, true);
  free(playback);
  free(id);
}

int main(void) {
#ifdef _WIN32
  system("rmdir /s /q pieces 2> nul");
#else
  system("rm -rf pieces");
#endif
  MKDIR("pieces");
  const char *capture_env = getenv("URSATZ_GUI_SNAPSHOT_CAPTURE");
  capture = capture_env && capture_env[0] == '1';
  if (capture) {
    MKDIR(URSATZ_GUI_TEST_PIECE_FIXTURES);
  }

  struct mg_context *ctx = ursatz_gui_server_start(PORT, ".");
  check(ctx != NULL, "server starts");
  if (!ctx) {
    return 1;
  }

  const char *corpus = URSATZ_FIXTURES_DIR "/framework-v1-corpus";
  char path[512];
  snprintf(path, sizeof(path), "%s/Bach 114.mid", corpus);
  snapshot_piece("bach-114", path, "Bach%20114.mid");

  /* A MIDI file without a time signature: the meter is defaulted. */
  static const unsigned char kNoMeterMidi[] = {'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, 0x01, 0xE0, 'M', 'T',
                                               'r', 'k', 0, 0, 0, 13, 0x00, 0x90, 0x3C, 0x40, 0x83, 0x60, 0x80,
                                               0x3C, 0x40, 0x00, 0xFF, 0x2F, 0x00};
  test_write_file("no-meter.mid", (const char *)kNoMeterMidi, sizeof(kNoMeterMidi));
  snapshot_piece("no-meter", "no-meter.mid", "no-meter.mid");

  snapshot_piece("chords-backup", URSATZ_FIXTURES_DIR "/musicxml/chords_backup.musicxml",
                 "chords_backup.musicxml");

  ursatz_gui_server_stop(ctx);
  return test_finish("piece_snapshot");
}
