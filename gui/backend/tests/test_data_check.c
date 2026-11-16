/* Confirms the startup data check (service/data_check.h): a data folder the one-time migrations of the GUI before the
 * overhaul never ran on is refused with a message that names what is missing, and everything else passes and gets the
 * `.pre-overhaul-checked` marker. Each case makes its own data folder in the working directory and removes it. */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#define MKDIR(path) mkdir(path, 0755)
#endif

#include "service/data_check.h"
#include "gui_check.h"

static const char *kRoot = "case";

static void make_dirs(const char *relative) {
  char path[512];
  snprintf(path, sizeof(path), "%s/%s", kRoot, relative);
  for (char *c = path + strlen(kRoot) + 1;; c++) {
    if (*c == '/' || *c == '\0') {
      char saved = *c;
      *c = '\0';
      MKDIR(path);
      *c = saved;
      if (saved == '\0') {
        break;
      }
    }
  }
}

static void write_file(const char *relative, const char *text) {
  char path[512];
  snprintf(path, sizeof(path), "%s/%s", kRoot, relative);
  test_write_file(path, text, strlen(text));
}

static bool exists(const char *relative) {
  char path[512];
  snprintf(path, sizeof(path), "%s/%s", kRoot, relative);
  return test_file_exists(path);
}

static void reset_case(void) {
#ifdef _WIN32
  system("rmdir /s /q case 2> nul");
#else
  system("rm -rf case");
#endif
  MKDIR(kRoot);
}

static bool run(char *message, size_t size) {
  return ursatz_gui_data_check(kRoot, message, size);
}

static bool has(const char *text, const char *needle) {
  return strstr(text, needle) != NULL;
}

/* An item of type with one version per key, the last one current. The key text "null" writes JSON null. */
static void make_item(const char *id, const char *type, const char *const *keys, int versions) {
  char path[256];
  char text[512];
  snprintf(path, sizeof(path), "materials/items/%s", id);
  make_dirs(path);
  for (int i = 0; i < versions; i++) {
    snprintf(path, sizeof(path), "materials/items/%s/v%d.json", id, i + 1);
    if (strcmp(keys[i], "null") == 0) {
      snprintf(text, sizeof(text), "{\"uses\":[],\"motif\":{\"key\":null}}");
    } else {
      snprintf(text, sizeof(text), "{\"uses\":[],\"motif\":{\"key\":\"%s\"}}", keys[i]);
    }
    write_file(path, text);
  }
  snprintf(path, sizeof(path), "materials/items/%s/item.json", id);
  snprintf(text, sizeof(text), "{\"id\":\"%s\",\"type\":\"%s\",\"current_version\":%d}", id, type, versions);
  write_file(path, text);
}

static void make_store_markers(int count) {
  static const char *names[] = {"tones-migrated", "segments-migrated", "themes-cleared"};
  for (int i = 0; i < count; i++) {
    char path[128];
    snprintf(path, sizeof(path), "materials/%s", names[i]);
    write_file(path, "done\n");
  }
}

static void test_empty_folder(void) {
  printf("-- an empty folder\n");
  reset_case();
  char message[4096];
  check_quiet(run(message, sizeof(message)) && message[0] == '\0', "an empty folder passes");
  check_quiet(exists(".pre-overhaul-checked"), "an empty folder gets the marker");
  check_quiet(run(message, sizeof(message)), "an empty folder passes again");
}

static void test_store_without_items(void) {
  printf("-- a store without items\n");
  reset_case();
  make_dirs("materials/projects");
  make_dirs("materials/items");
  char message[4096];
  check_quiet(run(message, sizeof(message)), "a store with projects and no items passes");
  check_quiet(!exists("materials/tones-migrated") && !exists("materials/segments-migrated") && !exists("materials/themes-cleared"),
        "a store with no items needs no migration markers and gets none");
}

static void test_corpus_db(void) {
  printf("-- the old corpus.db\n");
  reset_case();
  make_dirs("data");
  write_file("data/corpus.db", "old");
  char message[4096];
  check_quiet(!run(message, sizeof(message)), "an unimported data/corpus.db is refused");
  check_quiet(has(message, "data/corpus.db") && has(message, "collections/.imported-from-corpus-db"),
        "the refusal names the database and the import marker");
  check_quiet(has(message, "pre-overhaul") && has(message, "then start this one"), "the refusal says what to run first");
  check_quiet(!exists(".pre-overhaul-checked"), "a refusal writes no marker");
  make_dirs("collections");
  write_file("collections/.imported-from-corpus-db", "imported\n");
  check_quiet(run(message, sizeof(message)), "data/corpus.db passes once the import marker exists");
}

static void test_store_markers(void) {
  printf("-- the store markers\n");
  reset_case();
  const char *keys[] = {"C major"};
  make_item("mot-a", "motif", keys, 1);
  char message[4096];
  check_quiet(!run(message, sizeof(message)), "a motif without store markers is refused");
  check_quiet(has(message, "tones-migrated") && has(message, "segments-migrated") && has(message, "themes-cleared"),
        "the refusal names all three markers");
  make_store_markers(2);
  check_quiet(!run(message, sizeof(message)), "a motif with two store markers is refused");
  check_quiet(!has(message, "tones-migrated") && !has(message, "segments-migrated") && has(message, "themes-cleared"),
        "only the missing marker is named");
  make_store_markers(3);
  check_quiet(run(message, sizeof(message)), "a motif with all three markers passes");
}

static void test_tonics(void) {
  printf("-- motifs not in C\n");
  reset_case();
  make_dirs("materials");
  make_store_markers(3);
  const char *old_d[] = {"D minor"};
  const char *old_eb[] = {"C major", "Eb major"};
  const char *lower[] = {"c major"};
  const char *rewritten[] = {"D minor", "C minor"};
  const char *spaced[] = {"  C major "};
  const char *none[] = {"null"};
  const char *empty[] = {"  "};
  const char *theme[] = {"D minor"};
  make_item("mot-old-d", "motif", old_d, 1);
  make_item("mot-old-eb", "motif", old_eb, 2);
  make_item("mot-lower", "motif", lower, 1);
  make_item("mot-rewritten", "motif", rewritten, 2);
  make_item("mot-spaced", "motif", spaced, 1);
  make_item("mot-none", "motif", none, 1);
  make_item("mot-empty", "motif", empty, 1);
  make_item("thm-key", "theme", theme, 1);
  char message[4096];
  check_quiet(!run(message, sizeof(message)), "motifs on another tonic are refused");
  check_quiet(has(message, "mot-old-d") && has(message, "mot-old-eb") && has(message, "mot-lower"),
        "the current key decides: D minor, Eb major and c major are named");
  check_quiet(!has(message, "mot-rewritten") && !has(message, "mot-spaced") && !has(message, "mot-none") &&
            !has(message, "mot-empty") && !has(message, "thm-key"),
        "a rewritten, spaced, keyless or empty-key motif and a theme are not named");
  check_quiet(has(message, "Rewrite in C"), "the refusal says how to fix a motif");
  check_quiet(strstr(message, "mot-lower") < strstr(message, "mot-old-d") && strstr(message, "mot-old-d") < strstr(message, "mot-old-eb"),
        "the motifs are listed in name order");
  check_quiet(!exists(".pre-overhaul-checked"), "a refusal writes no marker");
  system("rm -rf case/materials/items/mot-old-d case/materials/items/mot-old-eb case/materials/items/mot-lower");
  check_quiet(run(message, sizeof(message)), "the store passes once the three are removed");
}

static void test_marker_wins(void) {
  printf("-- a checked folder\n");
  reset_case();
  make_dirs("data");
  write_file("data/corpus.db", "old");
  const char *keys[] = {"D minor"};
  make_item("mot-old", "motif", keys, 1);
  write_file(".pre-overhaul-checked", "checked\n");
  char message[4096] = "left over";
  check_quiet(run(message, sizeof(message)) && message[0] == '\0', "a folder with the marker passes whatever it holds");
}

int main(void) {
  test_empty_folder();
  test_store_without_items();
  test_corpus_db();
  test_store_markers();
  test_tonics();
  test_marker_wins();
  reset_case();
#ifdef _WIN32
  system("rmdir /s /q case 2> nul");
#else
  system("rm -rf case");
#endif
  return test_finish("data_check");
}
