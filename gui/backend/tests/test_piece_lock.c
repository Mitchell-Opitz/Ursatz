/* The piece lock (service/piece_lock.h): writers of one piece never interleave and a reader never sees half a write.
 *   - the table: two threads count under one id; entries are freed at 0.
 *   - two threads PATCH (notation, which re-analyzes) and re-analyze one piece 50 times each while a third reads the
 *     detail, the score and the playback: every read parses, and the analysis document's meter agrees with the
 *     notation override in meta.json that the same read returned.
 *   - a delete during reads gives NOT_FOUND or the whole piece.
 * Runs in its own working directory (pieces/ is created there). */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
typedef HANDLE thread_t;
typedef DWORD thread_result_t;
#define THREAD_CALL WINAPI
#define THREAD_RETURN 0
static bool thread_start(thread_t *thread, thread_result_t(THREAD_CALL *fn)(void *), void *arg) {
  *thread = CreateThread(NULL, 0, fn, arg, 0, NULL);
  return *thread != NULL;
}
static void thread_join(thread_t thread) {
  WaitForSingleObject(thread, INFINITE);
  CloseHandle(thread);
}
#else
#include <pthread.h>
typedef pthread_t thread_t;
typedef void *thread_result_t;
#define THREAD_CALL
#define THREAD_RETURN NULL
static bool thread_start(thread_t *thread, thread_result_t (*fn)(void *), void *arg) {
  return pthread_create(thread, NULL, fn, arg) == 0;
}
static void thread_join(thread_t thread) {
  pthread_join(thread, NULL);
}
#endif

#include <cJSON.h>

#include "service/analysis_service.h"
#include "service/link_service.h"
#include "service/piece_lock.h"

#ifndef URSATZ_GUI_TEST_SOURCE
#error "URSATZ_GUI_TEST_SOURCE must name the MusicXML file the pieces are made from"
#endif

#define ROUNDS 50

static const char *kPiecesDir = "pieces";
static const char *kOverrideMeter = "2/4";

static int failures = 0;
static volatile int stop_readers = 0;

static void check(bool cond, const char *what) {
  if (!cond) {
    fprintf(stderr, "FAIL: %s\n", what);
    failures++;
  } else {
    printf("PASS: %s\n", what);
  }
}

static char *read_all(const char *path, size_t *size) {
  FILE *file = fopen(path, "rb");
  if (!file) {
    return NULL;
  }
  fseek(file, 0, SEEK_END);
  long length = ftell(file);
  fseek(file, 0, SEEK_SET);
  char *data = malloc((size_t)length + 1);
  if (data) {
    *size = fread(data, 1, (size_t)length, file);
    data[*size] = '\0';
  }
  fclose(file);
  return data;
}

/* The label of the analysis document's meter ("meter" -> "label"), copied into out; false when absent. */
static bool document_meter_label(const char *json, char *out, size_t out_size) {
  cJSON *root = cJSON_Parse(json);
  if (!root) {
    return false;
  }
  const cJSON *meter = cJSON_GetObjectItemCaseSensitive(root, "meter");
  const cJSON *label = cJSON_IsObject(meter) ? cJSON_GetObjectItemCaseSensitive(meter, "label") : NULL;
  bool ok = cJSON_IsString(label) && label->valuestring && strlen(label->valuestring) < out_size;
  if (ok) {
    snprintf(out, out_size, "%s", label->valuestring);
  }
  cJSON_Delete(root);
  return ok;
}

/* ---- the lock table ---- */

static long g_counter = 0;

static thread_result_t THREAD_CALL count_under_lock(void *arg) {
  (void)arg;
  for (int i = 0; i < 20000; i++) {
    if (ursatz_gui_piece_lock("piece-counter")) {
      long value = g_counter;
      g_counter = value + 1;
      ursatz_gui_piece_unlock("piece-counter");
    }
  }
  return THREAD_RETURN;
}

static void test_lock_table(void) {
  thread_t threads[4];
  bool started[4] = {false};
  for (int i = 0; i < 4; i++) {
    started[i] = thread_start(&threads[i], count_under_lock, NULL);
  }
  for (int i = 0; i < 4; i++) {
    if (started[i]) {
      thread_join(threads[i]);
    }
  }
  check(g_counter == 4 * 20000, "four threads counting under one piece lock lose no update");
  check(ursatz_gui_piece_lock_entries() == 0, "the lock table is empty once every hold is released");

  check(ursatz_gui_piece_lock("piece-a") && ursatz_gui_piece_lock("piece-b"), "two different pieces lock at once");
  check(ursatz_gui_piece_lock_entries() == 2, "each held piece has an entry");
  ursatz_gui_piece_unlock("piece-a");
  ursatz_gui_piece_unlock("piece-b");
  check(ursatz_gui_piece_lock_entries() == 0, "entries are freed at count 0");
}

/* ---- concurrent writers and a reader ---- */

typedef struct {
  const char *id;
  int failed; /* operations that did not return OK */
} Writer;

static thread_result_t THREAD_CALL patch_notation(void *arg) {
  Writer *writer = arg;
  for (int i = 0; i < ROUNDS; i++) {
    UrsatzGuiNotation notation;
    memset(&notation, 0, sizeof(notation));
    snprintf(notation.meter, sizeof(notation.meter), "%s", kOverrideMeter);
    UrsatzGuiMetaUpdate update = {0};
    update.has_notation = true;
    update.notation = (i % 2 == 0) ? &notation : NULL;
    char message[96] = "";
    if (ursatz_gui_analysis_update_piece(kPiecesDir, writer->id, &update, message, sizeof(message)) !=
        URSATZ_GUI_ANALYSIS_OK) {
      writer->failed++;
    }
  }
  return THREAD_RETURN;
}

static thread_result_t THREAD_CALL reanalyze_piece(void *arg) {
  Writer *writer = arg;
  for (int i = 0; i < ROUNDS; i++) {
    if (ursatz_gui_analysis_reanalyze(kPiecesDir, writer->id) != URSATZ_GUI_ANALYSIS_OK) {
      writer->failed++;
    }
  }
  return THREAD_RETURN;
}

typedef struct {
  const char *id;
  const char *original_label;
  int reads;
  int bad_result;     /* a read that was not OK */
  int bad_document;   /* a document that does not parse or has no meter */
  int mismatched;     /* meter of the document differs from the notation override returned with it */
} Reader;

static thread_result_t THREAD_CALL read_detail(void *arg) {
  Reader *reader = arg;
  while (!stop_readers) {
    UrsatzGuiPieceDetail *detail = NULL;
    UrsatzGuiAnalysisResult result = ursatz_gui_analysis_get_piece(kPiecesDir, reader->id, &detail);
    reader->reads++;
    if (result != URSATZ_GUI_ANALYSIS_OK) {
      reader->bad_result++;
      continue;
    }
    char label[32];
    if (!document_meter_label(detail->document_json, label, sizeof(label))) {
      reader->bad_document++;
    } else {
      const char *expected =
          detail->has_notation && detail->notation.meter[0] != '\0' ? detail->notation.meter : reader->original_label;
      if (strcmp(label, expected) != 0) {
        reader->mismatched++;
      }
    }
    ursatz_gui_piece_detail_destroy(detail);

    char *xml = NULL;
    bool defaulted = false;
    char message[256];
    if (ursatz_gui_analysis_get_piece_musicxml(kPiecesDir, reader->id, &xml, &defaulted, message) !=
        URSATZ_GUI_ANALYSIS_OK) {
      reader->bad_result++;
    }
    free(xml);

    char *playback = NULL;
    if (ursatz_gui_analysis_get_piece_playback(kPiecesDir, reader->id, &playback) != URSATZ_GUI_ANALYSIS_OK) {
      reader->bad_result++;
    }
    free(playback);
  }
  return THREAD_RETURN;
}

static char *upload_piece(const char *source, const char *name) {
  size_t size = 0;
  char *data = read_all(source, &size);
  if (!data) {
    return NULL;
  }
  char *id = NULL;
  UrsatzGuiAnalysisResult result =
      ursatz_gui_analysis_import_and_save(kPiecesDir, (const unsigned char *)data, size, name, &id);
  free(data);
  return result == URSATZ_GUI_ANALYSIS_OK ? id : NULL;
}

static void test_concurrent_writers(void) {
  char *id = upload_piece(URSATZ_GUI_TEST_SOURCE, "lock.musicxml");
  check(id != NULL, "a piece is uploaded");
  if (!id) {
    return;
  }

  UrsatzGuiPieceDetail *detail = NULL;
  char original[32] = "";
  bool have_original = ursatz_gui_analysis_get_piece(kPiecesDir, id, &detail) == URSATZ_GUI_ANALYSIS_OK &&
                       document_meter_label(detail->document_json, original, sizeof(original));
  ursatz_gui_piece_detail_destroy(detail);
  check(have_original, "the stored document has a meter");
  check(strcmp(original, kOverrideMeter) != 0, "the override meter differs from the piece's own");

  Writer patcher = {id, 0};
  Writer reanalyzer = {id, 0};
  Reader reader = {id, original, 0, 0, 0, 0};
  stop_readers = 0;
  thread_t patch_thread, reanalyze_thread, read_thread;
  bool a = thread_start(&read_thread, read_detail, &reader);
  bool b = thread_start(&patch_thread, patch_notation, &patcher);
  bool c = thread_start(&reanalyze_thread, reanalyze_piece, &reanalyzer);
  if (b) {
    thread_join(patch_thread);
  }
  if (c) {
    thread_join(reanalyze_thread);
  }
  stop_readers = 1;
  if (a) {
    thread_join(read_thread);
  }

  check(a && b && c, "the threads started");
  check(patcher.failed == 0, "every PATCH returned OK");
  check(reanalyzer.failed == 0, "every re-analyze returned OK");
  check(reader.reads > 0, "the reader read the piece while the writers ran");
  check(reader.bad_result == 0, "every read between the writes returned OK");
  check(reader.bad_document == 0, "every document read between the writes parses");
  check(reader.mismatched == 0, "every document read matches the notation override read with it");

  UrsatzGuiNotation final_notation;
  bool has_final = true;
  ursatz_gui_analysis_get_notation(kPiecesDir, id, &has_final, &final_notation);
  check(!has_final, "the last PATCH left the piece without an override");
  ursatz_gui_analysis_delete_piece(kPiecesDir, id);
  free(id);
}

/* ---- delete during reads ---- */

static thread_result_t THREAD_CALL read_until_gone(void *arg) {
  Reader *reader = arg;
  for (;;) {
    char *xml = NULL;
    bool defaulted = false;
    char message[256];
    UrsatzGuiAnalysisResult result =
        ursatz_gui_analysis_get_piece_musicxml(kPiecesDir, reader->id, &xml, &defaulted, message);
    reader->reads++;
    free(xml);
    if (result == URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND) {
      return THREAD_RETURN;
    }
    if (result != URSATZ_GUI_ANALYSIS_OK) {
      reader->bad_result++;
      return THREAD_RETURN;
    }

    UrsatzGuiPieceDetail *detail = NULL;
    result = ursatz_gui_analysis_get_piece(kPiecesDir, reader->id, &detail);
    reader->reads++;
    if (result == URSATZ_GUI_ANALYSIS_OK) {
      char label[32];
      if (!document_meter_label(detail->document_json, label, sizeof(label))) {
        reader->bad_document++;
      }
      ursatz_gui_piece_detail_destroy(detail);
    } else if (result == URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND) {
      return THREAD_RETURN;
    } else {
      reader->bad_result++;
      return THREAD_RETURN;
    }
  }
}

static void test_delete_during_reads(void) {
  int bad = 0;
  int document_bad = 0;
  int joined = 0;
  for (int round = 0; round < 10; round++) {
    char *id = upload_piece(URSATZ_GUI_TEST_SOURCE, "gone.musicxml");
    if (!id) {
      bad++;
      continue;
    }
    Reader readers[2] = {{id, "", 0, 0, 0, 0}, {id, "", 0, 0, 0, 0}};
    thread_t threads[2];
    bool started[2];
    for (int i = 0; i < 2; i++) {
      started[i] = thread_start(&threads[i], read_until_gone, &readers[i]);
    }
    /* Let the readers get going, then delete. */
    for (volatile int spin = 0; spin < 2000000 * (round % 3 + 1); spin++) {
    }
    UrsatzGuiAnalysisResult deleted = ursatz_gui_analysis_delete_piece(kPiecesDir, id);
    if (deleted != URSATZ_GUI_ANALYSIS_OK) {
      bad++;
    }
    for (int i = 0; i < 2; i++) {
      if (started[i]) {
        thread_join(threads[i]);
        joined++;
      }
      bad += readers[i].bad_result;
      document_bad += readers[i].bad_document;
    }
    free(id);
  }
  check(joined == 20, "the readers ran in every round");
  check(bad == 0, "a delete during reads gives NOT_FOUND or the whole piece, never a partial one");
  check(document_bad == 0, "every document read before the delete parses");
}

/* ---- the reads of the link and style services ---- */

static void sleep_ms(unsigned ms) {
#ifdef _WIN32
  Sleep(ms);
#else
  struct timespec pause = {ms / 1000, (long)(ms % 1000) * 1000000L};
  nanosleep(&pause, NULL);
#endif
}

typedef struct {
  const char *id;
  volatile int done;
  bool document_ok;  /* the whole stored document came back and parses */
  bool filename_ok;  /* the display name came back */
  bool title_ok;     /* the link service's title is the display name without its extension */
} ServiceRead;

/* What the link and style services read of a piece: the whole stored document, and the display name. */
static thread_result_t THREAD_CALL read_like_services(void *arg) {
  ServiceRead *read = arg;
  char *document = NULL;
  read->document_ok = false;
  if (ursatz_gui_analysis_get_document(kPiecesDir, read->id, NULL, 0, &document) == URSATZ_GUI_ANALYSIS_OK && document) {
    cJSON *parsed = cJSON_Parse(document);
    read->document_ok = cJSON_IsObject(parsed);
    cJSON_Delete(parsed);
  }
  free(document);
  char *filename = NULL;
  read->filename_ok = ursatz_gui_analysis_get_filename(kPiecesDir, read->id, &filename) == URSATZ_GUI_ANALYSIS_OK &&
                      filename && strcmp(filename, "services.musicxml") == 0;
  free(filename);
  char *title = ursatz_gui_link_piece_title(read->id);
  read->title_ok = title && strcmp(title, "services") == 0;
  free(title);
  read->done = 1;
  return THREAD_RETURN;
}

static void test_service_reads(void) {
  char *id = upload_piece(URSATZ_GUI_TEST_SOURCE, "services.musicxml");
  check(id != NULL, "a piece is uploaded for the service reads");
  if (!id) {
    return;
  }
  char *stored = NULL;
  char path[512];
  size_t size = 0;
  if (snprintf(path, sizeof(path), "%s/%s/analysis.json", kPiecesDir, id) < (int)sizeof(path)) {
    stored = read_all(path, &size);
  }
  char *whole = NULL;
  check(stored != NULL && ursatz_gui_analysis_get_document(kPiecesDir, id, NULL, 0, &whole) == URSATZ_GUI_ANALYSIS_OK &&
            whole && strcmp(whole, stored) == 0,
        "a document read without names is the stored analysis.json, unchanged");
  free(whole);
  free(stored);

  ServiceRead read = {id, 0, false, false, false};
  check(ursatz_gui_piece_lock(id), "the test holds the piece lock");
  thread_t thread;
  bool started = thread_start(&thread, read_like_services, &read);
  sleep_ms(300);
  check(started && read.done == 0, "the document, name and title reads wait while the piece lock is held");
  ursatz_gui_piece_unlock(id);
  if (started) {
    thread_join(thread);
  }
  check(read.done == 1 && read.document_ok && read.filename_ok && read.title_ok,
        "the reads answer once the lock is released");

  char *missing = NULL;
  check(ursatz_gui_analysis_get_document(kPiecesDir, "no-such-piece", NULL, 0, &missing) ==
                URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND &&
            ursatz_gui_analysis_get_filename(kPiecesDir, "no-such-piece", &missing) == URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND &&
            !ursatz_gui_analysis_piece_exists(kPiecesDir, "no-such-piece") &&
            ursatz_gui_analysis_piece_exists(kPiecesDir, id),
        "a missing piece is not found by the reads and exists only once uploaded");
  ursatz_gui_analysis_delete_piece(kPiecesDir, id);
  free(id);
}

int main(void) {
  test_lock_table();
  test_concurrent_writers();
  test_delete_during_reads();
  test_service_reads();
  check(ursatz_gui_piece_lock_entries() == 0, "no lock entry is left behind");
  return failures == 0 ? 0 : 1;
}
