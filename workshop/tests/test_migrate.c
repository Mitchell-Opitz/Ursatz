/* The migration of the GUI's data folder (layout 0) to layout 1 (P63 B7, R-642), on the version 0 fixture (fixtures/data-v0, made by
 * fixtures/make-data-v0.sh from the GUI of db98084): a dry run writes nothing and reports the counts; a real run backs the folder up
 * first, keeps every project, item, version, body, Library piece, collection, subject, style and link run, migrates the old member
 * forms, and is a no-op the second time; an interrupted run starts again or finishes; a folder the GUI's one-time migrations never ran
 * on is refused with the data check's message; and an unmigrated folder is refused by workshop_open. The cases of the data check
 * (the corpus.db import, the store markers, motifs not in C) are in the second half. Each case works in its own folder of the working
 * directory. */

#include <dirent.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cJSON.h>

#include <common/files.h>
#include <ursatz_workshop/workshop_api.h>

#include "base/knowledge.h"
#include "items/data_check.h"
#include "items/layout.h"
#include "items/store.h"
#include "ursatz_check.h"

#ifndef URSATZ_WORKSHOP_DATA_V0
#error "URSATZ_WORKSHOP_DATA_V0 must name the version 0 fixture folder"
#endif

static void sh(const char *format, ...) __attribute__((format(printf, 1, 2)));

static void sh(const char *format, ...) {
  char command[2048];
  va_list args;
  va_start(args, format);
  vsnprintf(command, sizeof(command), format, args);
  va_end(args);
  if (system(command) != 0) {
    fprintf(stderr, "command failed: %s\n", command);
  }
}

static void fresh_copy(const char *name) {
  sh("rm -rf '%s' '%s'-backup-* && cp -r '%s' '%s'", name, name, URSATZ_WORKSHOP_DATA_V0, name);
}

typedef struct {
  long files;
  long long bytes;
} Totals;

static void totals_of(const char *path, Totals *totals) {
  CommonNames names = {0};
  if (common_dir_list(path, &names, NULL) != URSATZ_OK) {
    return;
  }
  for (size_t i = 0; i < names.count; i++) {
    char child[1024];
    struct stat info;
    snprintf(child, sizeof(child), "%s/%s", path, names.items[i]);
    if (lstat(child, &info) != 0) {
      continue;
    }
    if (S_ISDIR(info.st_mode)) {
      totals_of(child, totals);
    } else if (S_ISREG(info.st_mode)) {
      totals->files++;
      totals->bytes += (long long)info.st_size;
    }
  }
  common_names_free(&names);
}

static const char *text(const cJSON *object, const char *name) {
  const cJSON *item = object ? cJSON_GetObjectItemCaseSensitive(object, name) : NULL;
  return cJSON_IsString(item) ? item->valuestring : "";
}

static double number(const cJSON *object, const char *name) {
  const cJSON *item = object ? cJSON_GetObjectItemCaseSensitive(object, name) : NULL;
  return cJSON_IsNumber(item) ? item->valuedouble : -1;
}

/* A JSON file of a folder, or NULL. */
static cJSON *read_at(const char *folder, const char *relative) {
  char path[1024];
  snprintf(path, sizeof(path), "%s/%s", folder, relative);
  return test_read_json(path);
}

/* Equal as JSON apart from the members named in ignore (a comma list, top level only). */
static bool same_json(const cJSON *a, const cJSON *b, const char *ignore) {
  cJSON *left = cJSON_Duplicate(a, 1);
  cJSON *right = cJSON_Duplicate(b, 1);
  char list[256];
  snprintf(list, sizeof(list), "%s", ignore);
  for (char *name = strtok(list, ","); name; name = strtok(NULL, ",")) {
    cJSON_DeleteItemFromObjectCaseSensitive(left, name);
    cJSON_DeleteItemFromObjectCaseSensitive(right, name);
  }
  bool same = left && right && cJSON_Compare(left, right, 1);
  cJSON_Delete(left);
  cJSON_Delete(right);
  return same;
}

static void write_text(const char *path, const char *content) {
  test_write_file(path, content, strlen(content));
}

static bool names_of(const char *folder, CommonNames *names) {
  return common_dir_list(folder, names, NULL) == URSATZ_OK;
}

/* --- the dry run, the real run and the content -------------------------------------------------------------------------------- */

static void check_counts(const cJSON *report, bool dry) {
  const cJSON *counts = cJSON_GetObjectItemCaseSensitive(report, "counts");
  check_quiet(number(counts, "projects") == 2 && number(counts, "items") == 11 && number(counts, "versions") == 17 &&
                  number(counts, "subjects") == 2 && number(counts, "library_pieces") == 2 && number(counts, "collections") == 1 &&
                  number(counts, "styles") == 1 && number(counts, "link_runs") == 1,
              dry ? "the dry run counts every kind of the fixture" : "the run counts every kind of the fixture");
  check_quiet(number(counts, "bodies_rewritten") == 10, "ten bodies are rewritten: a motif with the old key and the nine theme and piece bodies, which become stored requests");
}

static void test_dry_run(void) {
  printf("-- a dry run\n");
  fresh_copy("dry");
  Totals before = {0, 0};
  totals_of("dry", &before);
  cJSON *report = NULL;
  UrsatzError error = {{0}};
  WorkshopMigrateOptions options = {true, NULL};
  check_quiet(workshop_migrate("dry", &options, &report, &error) == URSATZ_OK && report, "the dry run answers");
  check_counts(report, true);
  check_quiet(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(report, "dry_run")) &&
                  !cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(report, "changed")),
              "it says it changed nothing");
  check_quiet(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(report, "moves")) == 6, "it names the six moves of the old entries");
  cJSON_Delete(report);
  Totals after = {0, 0};
  totals_of("dry", &after);
  check_quiet(before.files == after.files && before.bytes == after.bytes, "the dry run left every file as it was");
  check_quiet(!common_path_exists("dry/.migrate-1") && !common_path_exists("dry/data-folder.json") && !common_path_exists("dry/.pre-1") &&
                  !common_path_exists("dry/projects"),
              "it made no folder and no marker");
  CommonNames beside = {0};
  int backups = 0;
  if (names_of(".", &beside)) {
    for (size_t i = 0; i < beside.count; i++) {
      backups += strncmp(beside.items[i], "dry-backup-", 11) == 0;
    }
  }
  common_names_free(&beside);
  check_quiet(backups == 0, "and no backup");
}

static void check_item_content(const char *old_root, const char *new_root) {
  /* every item: the header, every version's body (the old members migrated) and envelope, and the composed files */
  CommonNames projects = {0};
  char old_items[1024];
  snprintf(old_items, sizeof(old_items), "%s/materials/items", old_root);
  CommonNames items = {0};
  bool listed = names_of(old_items, &items);
  check_quiet(listed && items.count == 11, "the old store holds the eleven items");
  int found = 0;
  int bodies_equal = 0;
  int versions_seen = 0;
  int files_kept = 0;
  char projects_dir[1024];
  snprintf(projects_dir, sizeof(projects_dir), "%s/projects", new_root);
  names_of(projects_dir, &projects);
  for (size_t i = 0; i < items.count; i++) {
    char relative[1024];
    snprintf(relative, sizeof(relative), "materials/items/%s/item.json", items.items[i]);
    cJSON *old_item = read_at(old_root, relative);
    if (!old_item) {
      continue;
    }
    const char *project = text(old_item, "project_id");
    snprintf(relative, sizeof(relative), "projects/%s/items/%s/item.json", project, items.items[i]);
    cJSON *header = read_at(new_root, relative);
    if (header && strcmp(text(header, "format"), "ursatz-item") == 0 && strcmp(text(header, "name"), text(old_item, "name")) == 0 &&
        number(header, "current_version") == number(old_item, "current_version") &&
        strcmp(text(header, "created_at"), text(old_item, "created_at")) == 0 &&
        strcmp(text(header, "updated_at"), text(old_item, "updated_at")) == 0 && !cJSON_GetObjectItemCaseSensitive(header, "project_id") &&
        !cJSON_GetObjectItemCaseSensitive(header, "versions")) {
      found++;
    }
    const cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, cJSON_GetObjectItemCaseSensitive(old_item, "versions")) {
      int number_of_version = (int)number(entry, "number");
      char old_body[1024];
      snprintf(old_body, sizeof(old_body), "materials/items/%s/v%d.json", items.items[i], number_of_version);
      cJSON *body = read_at(old_root, old_body);
      workshop_material_body_migrate(body);
      workshop_body_requests_migrate(body);
      snprintf(relative, sizeof(relative), "projects/%s/items/%s/v%d/body.json", project, items.items[i], number_of_version);
      cJSON *new_body = read_at(new_root, relative);
      if (body && new_body && same_json(body, new_body, "format,format_version")) {
        bodies_equal++;
      }
      snprintf(relative, sizeof(relative), "projects/%s/items/%s/v%d/envelope.json", project, items.items[i], number_of_version);
      cJSON *envelope = read_at(new_root, relative);
      if (envelope && strcmp(text(envelope, "format"), "ursatz-item-version") == 0 && number(envelope, "version") == number_of_version &&
          strcmp(text(envelope, "note"), text(entry, "note")) == 0 && strcmp(text(envelope, "created_at"), text(entry, "created_at")) == 0 &&
          strcmp(text(envelope, "id"), items.items[i]) == 0 && strcmp(text(envelope, "kind"), text(old_item, "type")) == 0) {
        versions_seen++;
      }
      /* the composed files of the version are beside its body */
      char old_dir[1024];
      snprintf(old_dir, sizeof(old_dir), "%s/materials/items/%s/v%d", old_root, items.items[i], number_of_version);
      CommonNames files = {0};
      if (common_path_is_dir(old_dir) && names_of(old_dir, &files)) {
        for (size_t f = 0; f < files.count; f++) {
          char old_file[2400];
          char new_file[2400];
          snprintf(old_file, sizeof(old_file), "%s/%s", old_dir, files.items[f]);
          snprintf(new_file, sizeof(new_file), "%s/projects/%s/items/%s/v%d/%s", new_root, project, items.items[i], number_of_version,
                   files.items[f]);
          if (common_path_is_dir(old_file) ? common_path_is_dir(new_file) : common_path_exists(new_file)) {
            files_kept++;
          }
        }
      }
      common_names_free(&files);
      cJSON_Delete(body);
      cJSON_Delete(new_body);
      cJSON_Delete(envelope);
    }
    cJSON_Delete(header);
    cJSON_Delete(old_item);
  }
  check_quiet(found == 11, "every item has its header under its project, without the project and the versions list");
  check_quiet(versions_seen == 17, "every one of the 17 versions has its envelope");
  check_quiet(bodies_equal == 17, "every version body is the old body with the old members migrated, as format 1");
  check_quiet(files_kept >= 6, "the composed versions keep their files");
  common_names_free(&items);
  common_names_free(&projects);
}

static void check_other_kinds(const char *old_root, const char *new_root) {
  CommonNames names = {0};
  char folder[1024];
  /* projects */
  snprintf(folder, sizeof(folder), "%s/materials/projects", old_root);
  int projects_ok = 0;
  if (names_of(folder, &names)) {
    for (size_t i = 0; i < names.count; i++) {
      char relative[1024];
      snprintf(relative, sizeof(relative), "materials/projects/%s", names.items[i]);
      cJSON *old = read_at(old_root, relative);
      char id[128];
      snprintf(id, sizeof(id), "%.*s", (int)strlen(names.items[i]) - 5, names.items[i]);
      snprintf(relative, sizeof(relative), "projects/%s/project.json", id);
      cJSON *now = read_at(new_root, relative);
      if (old && now && strcmp(text(now, "format"), "ursatz-project") == 0 && same_json(old, now, "format,format_version")) {
        projects_ok++;
      }
      cJSON_Delete(old);
      cJSON_Delete(now);
    }
  }
  check_quiet(projects_ok == 2, "both projects are kept with the same content");
  common_names_free(&names);

  /* subjects and collections: the same file, stamped */
  static const struct {
    const char *old_folder;
    const char *new_folder;
    const char *format;
    int expected;
    const char *what;
  } kinds[] = {{"subjects", "subjects", "ursatz-subject", 2, "both subjects are kept with the same content"},
               {"collections", "library/collections", "ursatz-collection", 1, "the collection is kept with the same content"}};
  for (size_t k = 0; k < sizeof(kinds) / sizeof(kinds[0]); k++) {
    snprintf(folder, sizeof(folder), "%s/%s", old_root, kinds[k].old_folder);
    int ok = 0;
    if (names_of(folder, &names)) {
      for (size_t i = 0; i < names.count; i++) {
        char relative[1024];
        snprintf(relative, sizeof(relative), "%s/%s", kinds[k].old_folder, names.items[i]);
        cJSON *old = read_at(old_root, relative);
        snprintf(relative, sizeof(relative), "%s/%s", kinds[k].new_folder, names.items[i]);
        cJSON *now = read_at(new_root, relative);
        if (old && now && strcmp(text(now, "format"), kinds[k].format) == 0 && same_json(old, now, "format,format_version")) {
          ok++;
        }
        cJSON_Delete(old);
        cJSON_Delete(now);
      }
    }
    check_quiet(ok == kinds[k].expected, kinds[k].what);
    common_names_free(&names);
  }

  /* Library pieces: source and analysis byte for byte, piece.json with source_format and producer_key */
  snprintf(folder, sizeof(folder), "%s/pieces", old_root);
  int pieces_ok = 0;
  int keys = 0;
  if (names_of(folder, &names)) {
    for (size_t i = 0; i < names.count; i++) {
      char relative[1024];
      snprintf(relative, sizeof(relative), "pieces/%s/meta.json", names.items[i]);
      cJSON *old = read_at(old_root, relative);
      snprintf(relative, sizeof(relative), "library/pieces/%s/piece.json", names.items[i]);
      cJSON *now = read_at(new_root, relative);
      bool meta_ok = old && now && strcmp(text(now, "format"), "ursatz-library-piece") == 0 &&
                     strcmp(text(now, "source_format"), text(old, "format")) == 0 && strcmp(text(now, "filename"), text(old, "filename")) == 0 &&
                     strcmp(text(now, "uploaded_at"), text(old, "uploaded_at")) == 0 && strcmp(text(now, "producer_key"), text(old, "ursatz_commit")) == 0 &&
                     !cJSON_GetObjectItemCaseSensitive(now, "ursatz_commit");
      keys += cJSON_IsString(cJSON_GetObjectItemCaseSensitive(now, "producer_key"));
      char old_path[1024];
      char new_path[1024];
      snprintf(old_path, sizeof(old_path), "%s/pieces/%s/analysis.json", old_root, names.items[i]);
      snprintf(new_path, sizeof(new_path), "%s/library/pieces/%s/analysis.json", new_root, names.items[i]);
      char *old_analysis = test_read_file(old_path, NULL);
      char *new_analysis = test_read_file(new_path, NULL);
      bool analysis_ok = old_analysis && new_analysis && strcmp(old_analysis, new_analysis) == 0;
      free(old_analysis);
      free(new_analysis);
      pieces_ok += meta_ok && analysis_ok;
      cJSON_Delete(old);
      cJSON_Delete(now);
    }
  }
  check_quiet(pieces_ok == 2, "both Library pieces keep their analysis and their metadata, ursatz_commit renamed producer_key");
  check_quiet(keys == 1, "the piece whose meta.json had no ursatz_commit has no producer_key (it reads stale)");
  common_names_free(&names);

  /* styles and link runs */
  snprintf(folder, sizeof(folder), "%s/styles", old_root);
  int styles_ok = 0;
  if (names_of(folder, &names)) {
    for (size_t i = 0; i < names.count; i++) {
      char relative[1024];
      snprintf(relative, sizeof(relative), "styles/%s/meta.json", names.items[i]);
      cJSON *old = read_at(old_root, relative);
      snprintf(relative, sizeof(relative), "settings/styles/%s/style.json", names.items[i]);
      cJSON *now = read_at(new_root, relative);
      char old_model[1024];
      char new_model[1024];
      snprintf(old_model, sizeof(old_model), "%s/styles/%s/style-model.json", old_root, names.items[i]);
      snprintf(new_model, sizeof(new_model), "%s/settings/styles/%s/style-model.json", new_root, names.items[i]);
      char *a = test_read_file(old_model, NULL);
      char *b = test_read_file(new_model, NULL);
      styles_ok += old && now && strcmp(text(now, "format"), "ursatz-style") == 0 && same_json(old, now, "format,format_version") && a && b && strcmp(a, b) == 0;
      free(a);
      free(b);
      cJSON_Delete(old);
      cJSON_Delete(now);
    }
  }
  check_quiet(styles_ok == 1, "the style keeps its meta and its model");
  common_names_free(&names);
  snprintf(folder, sizeof(folder), "%s/links/runs", old_root);
  int runs_ok = 0;
  if (names_of(folder, &names)) {
    for (size_t i = 0; i < names.count; i++) {
      char relative[1024];
      snprintf(relative, sizeof(relative), "links/runs/%s/run.json", names.items[i]);
      cJSON *old = read_at(old_root, relative);
      snprintf(relative, sizeof(relative), "library/links/%s/run.json", names.items[i]);
      cJSON *now = read_at(new_root, relative);
      runs_ok += old && now && strcmp(text(now, "format"), "ursatz-link-run") == 0 && same_json(old, now, "format,format_version");
      cJSON_Delete(old);
      cJSON_Delete(now);
    }
  }
  check_quiet(runs_ok == 1, "the link run is kept with its result");
  common_names_free(&names);
  char old_cache[1024];
  char new_cache[1024];
  snprintf(old_cache, sizeof(old_cache), "%s/links/motive-link-cache.json", old_root);
  snprintf(new_cache, sizeof(new_cache), "%s/cache/motive-link-cache.json", new_root);
  char *a = test_read_file(old_cache, NULL);
  char *b = test_read_file(new_cache, NULL);
  check_quiet(a && b && strcmp(a, b) == 0, "the motive link cache is moved to cache/ unchanged");
  free(a);
  free(b);
}

static void check_old_forms(const char *root) {
  /* the three old member forms the fixture holds are written new */
  CommonNames projects = {0};
  char folder[1024];
  snprintf(folder, sizeof(folder), "%s/projects", root);
  names_of(folder, &projects);
  int key_gone = 0;
  int mode_gone = 0;
  int fill_gone = 0;
  for (size_t p = 0; p < projects.count; p++) {
    CommonNames items = {0};
    snprintf(folder, sizeof(folder), "%s/projects/%s/items", root, projects.items[p]);
    names_of(folder, &items);
    for (size_t i = 0; i < items.count; i++) {
      char relative[1024];
      snprintf(relative, sizeof(relative), "projects/%s/items/%s/v1/body.json", projects.items[p], items.items[i]);
      cJSON *body = read_at(root, relative);
      const cJSON *motif = cJSON_GetObjectItemCaseSensitive(body, "motif");
      /* a theme or piece body is a stored request now: its form members are the request's and its sections the request's */
      const cJSON *form = strncmp(items.items[i], "thm-", 4) == 0 ? cJSON_GetObjectItemCaseSensitive(body, "request") : NULL;
      const cJSON *sections = strncmp(items.items[i], "pce-", 4) == 0
                                  ? cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(body, "request"), "sections")
                                  : NULL;
      if (motif) {
        key_gone += !cJSON_GetObjectItemCaseSensitive(motif, "key") && cJSON_IsString(cJSON_GetObjectItemCaseSensitive(motif, "material"));
      }
      if (form) {
        mode_gone += !cJSON_GetObjectItemCaseSensitive(form, "mode");
      }
      const cJSON *section = NULL;
      cJSON_ArrayForEach(section, sections) {
        const cJSON *fill = cJSON_GetObjectItemCaseSensitive(section, "fill");
        if (cJSON_IsObject(fill) && strcmp(text(fill, "kind"), "empty") == 0) {
          fill_gone++;
        }
      }
      cJSON_Delete(body);
    }
    common_names_free(&items);
  }
  check_quiet(key_gone == 6, "every motif has the material and no key text");
  check_quiet(mode_gone >= 1, "a theme form has no mode");
  check_quiet(fill_gone >= 1, "a progression fill is an empty fill");
  common_names_free(&projects);
}

static void test_real_run(void) {
  printf("-- a real run\n");
  fresh_copy("run");
  Totals before = {0, 0};
  totals_of("run", &before);
  cJSON *report = NULL;
  UrsatzError error = {{0}};
  WorkshopMigrateOptions options = {false, NULL};
  check_quiet(workshop_migrate("run", &options, &report, &error) == URSATZ_OK && report, "the run answers");
  check_counts(report, false);
  const char *backup = text(report, "backup");
  check_quiet(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(report, "changed")) && backup[0] != '\0' && strstr(backup, "run-backup-") != NULL,
              "it reports a backup beside the folder, named with the time");
  Totals copy = {0, 0};
  totals_of(backup, &copy);
  check_quiet(copy.files == before.files && copy.bytes == before.bytes, "the backup holds every file and every byte of the folder");
  cJSON_Delete(report);

  check_quiet(common_path_exists("run/data-folder.json") && !common_path_exists("run/.migrate-1") && common_path_is_dir("run/.pre-1"),
              "data-folder.json is written, .migrate-1 is gone and the old entries are in .pre-1");
  cJSON *marker = read_at("run", "data-folder.json");
  check_quiet(strcmp(text(marker, "format"), "ursatz-data-folder") == 0 && number(marker, "format_version") == 2 &&
                  number(marker, "migrated_from") == 0,
              "the marker names format 2 and where it came from");
  cJSON_Delete(marker);
  check_quiet(!common_path_exists("run/materials") && !common_path_exists("run/pieces") && !common_path_exists("run/styles") &&
                  common_path_is_dir("run/.pre-1/materials") && common_path_is_dir("run/.pre-1/subjects"),
              "no old entry is left at the top");
  check_item_content("run/.pre-1", "run");
  check_other_kinds("run/.pre-1", "run");
  check_old_forms("run");

  /* a second run is a no-op */
  Totals first = {0, 0};
  totals_of("run", &first);
  report = NULL;
  check_quiet(workshop_migrate("run", &options, &report, &error) == URSATZ_OK && report &&
                  !cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(report, "changed")),
              "a second run is a no-op");
  cJSON_Delete(report);
  Totals second = {0, 0};
  totals_of("run", &second);
  check_quiet(first.files == second.files && first.bytes == second.bytes, "and changes no file");
  sh("rm -rf '%s'", backup);
}

static void test_interrupted(void) {
  printf("-- an interrupted run\n");
  fresh_copy("stopped");
  /* a run that stopped while it built: .migrate-1 holds a part and no .complete */
  sh("mkdir -p stopped/.migrate-1/projects/prj-half && echo '{}' > stopped/.migrate-1/projects/prj-half/project.json");
  cJSON *report = NULL;
  UrsatzError error = {{0}};
  WorkshopMigrateOptions options = {false, NULL};
  check_quiet(workshop_migrate("stopped", &options, &report, &error) == URSATZ_OK, "a run that stopped while it built starts again");
  check_quiet(common_path_exists("stopped/data-folder.json") && !common_path_exists("stopped/projects/prj-half") &&
                  !common_path_exists("stopped/.migrate-1"),
              "the part is deleted and the migration is made whole");
  cJSON_Delete(report);
  sh("rm -rf stopped-backup-*");

  /* a run that stopped during the last moves: the old entries are aside and some new ones are in place */
  fresh_copy("moving");
  report = NULL;
  check_quiet(workshop_migrate("moving", &options, &report, &error) == URSATZ_OK, "a folder to stop half way");
  const char *backup = text(report, "backup");
  char backup_copy[1024];
  snprintf(backup_copy, sizeof(backup_copy), "%s", backup);
  cJSON_Delete(report);
  sh("rm -f moving/data-folder.json && mkdir -p moving/.migrate-1 && mv moving/cache moving/.migrate-1/cache && "
     "mv moving/settings moving/.migrate-1/settings && echo complete > moving/.migrate-1/.complete");
  report = NULL;
  check_quiet(workshop_migrate("moving", &options, &report, &error) == URSATZ_OK, "a run that stopped in the last moves is finished");
  check_quiet(common_path_exists("moving/data-folder.json") && common_path_is_dir("moving/cache") && common_path_is_dir("moving/settings") &&
                  !common_path_exists("moving/.migrate-1") && common_path_is_dir("moving/projects") && common_path_is_dir("moving/.pre-1/materials"),
              "every entry is in place and the marker is written");
  check_item_content("moving/.pre-1", "moving");
  cJSON_Delete(report);
  sh("rm -rf '%s'", backup_copy);

  /* an empty folder and an absent one are made layout 1 */
  sh("rm -rf empty absent && mkdir empty");
  report = NULL;
  check_quiet(workshop_migrate("empty", &options, &report, &error) == URSATZ_OK && common_path_exists("empty/data-folder.json"),
              "an empty folder is made layout 1");
  cJSON_Delete(report);
  report = NULL;
  check_quiet(workshop_migrate("absent", &options, &report, &error) == URSATZ_OK && common_path_exists("absent/data-folder.json"),
              "an absent folder is created as layout 1");
  cJSON_Delete(report);
}

/* --- the refusals -------------------------------------------------------------------------------------------------------------- */

static void test_refusals(void) {
  printf("-- the refusals\n");
  sh("rm -rf old && mkdir -p old/data && echo old > old/data/corpus.db && mkdir -p old/collections");
  cJSON *report = NULL;
  UrsatzError error = {{0}};
  WorkshopMigrateOptions options = {false, NULL};
  check_quiet(workshop_migrate("old", &options, &report, &error) == URSATZ_ERR_FORMAT && report == NULL,
              "a folder with data/corpus.db and no marker is refused");
  check_quiet(strstr(error.message, "data/corpus.db was never imported") != NULL && strstr(error.message, "pre-overhaul") != NULL &&
                  strstr(error.message, "then start this one") != NULL,
              "with the data check's message");
  check_quiet(!common_path_exists("old/.migrate-1") && !common_path_exists("old/data-folder.json") && !common_path_exists("old/.pre-overhaul-checked"),
              "nothing is written");

  /* an unmigrated folder is refused by workshop_open, and opens once it is migrated */
  fresh_copy("open");
  WorkshopConfig config = {"open", NULL, 9201, 2, 2};
  WorkshopConfig resolved;
  char storage[512];
  check_quiet(workshop_config_resolve(&config, &resolved, storage, sizeof(storage), &error) == URSATZ_OK, "settings resolve");
  Workshop *workshop = NULL;
  check_quiet(workshop_open(&resolved, &workshop, &error) == URSATZ_ERR_FORMAT && workshop == NULL, "an unmigrated folder is refused by open");
  check_quiet(strstr(error.message, "is in the GUI's old layout") != NULL && strstr(error.message, "migrate --dry-run") != NULL &&
                  strstr(error.message, "ursatz --data open migrate") != NULL,
              "the refusal says how to back it up and migrate it");
  check_quiet(!common_path_exists("open/data-folder.json"), "and writes nothing");
  check_quiet(workshop_migrate("open", &options, &report, &error) == URSATZ_OK, "the folder migrates");
  cJSON_Delete(report);
  check_quiet(workshop_open(&resolved, &workshop, &error) == URSATZ_OK && workshop, "and then opens");
  workshop_close(workshop);
  sh("rm -rf open-backup-*");

  /* an empty folder opens and becomes layout 1; a marker of a newer version or another format is refused naming the file */
  sh("rm -rf fresh && mkdir fresh");
  WorkshopConfig fresh = {"fresh", NULL, 9202, 2, 2};
  check_quiet(workshop_config_resolve(&fresh, &resolved, storage, sizeof(storage), &error) == URSATZ_OK &&
                  workshop_open(&resolved, &workshop, &error) == URSATZ_OK,
              "an empty folder opens");
  workshop_close(workshop);
  cJSON *marker = read_at("fresh", "data-folder.json");
  check_quiet(strcmp(text(marker, "format"), "ursatz-data-folder") == 0 && !cJSON_GetObjectItemCaseSensitive(marker, "migrated_from"),
              "and is layout 1 from its first open");
  cJSON_Delete(marker);
  write_text("fresh/data-folder.json", "{\"format\":\"ursatz-data-folder\",\"format_version\":3,\"created_at\":\"2030-01-01T00:00:00Z\"}");
  check_quiet(workshop_open(&resolved, &workshop, &error) == URSATZ_ERR_FORMAT && strstr(error.message, "data-folder.json") != NULL,
              "a marker of a newer version is refused, naming the file");
  write_text("fresh/data-folder.json", "{\"format\":\"ursatz-project\",\"format_version\":1}");
  check_quiet(workshop_open(&resolved, &workshop, &error) == URSATZ_ERR_FORMAT, "a marker of another format is refused");
}

/* --- the data check's cases ---------------------------------------------------------------------------------------------------- */

static const char *kRoot = "case";

static void make_dirs(const char *relative) {
  char path[512];
  snprintf(path, sizeof(path), "%s/%s", kRoot, relative);
  common_dir_make(path, NULL);
}

static void write_file(const char *relative, const char *content) {
  char path[512];
  snprintf(path, sizeof(path), "%s/%s", kRoot, relative);
  test_write_file(path, content, strlen(content));
}

static void reset_case(void) {
  sh("rm -rf case");
  common_dir_make(kRoot, NULL);
}

static bool run_check(char *message, size_t size) {
  return workshop_data_check(kRoot, message, size);
}

static bool has(const char *haystack, const char *needle) {
  return strstr(haystack, needle) != NULL;
}

static void make_item(const char *id, const char *type, const char *const *keys, int versions) {
  char path[256];
  char content[512];
  snprintf(path, sizeof(path), "materials/items/%s", id);
  make_dirs(path);
  for (int i = 0; i < versions; i++) {
    snprintf(path, sizeof(path), "materials/items/%s/v%d.json", id, i + 1);
    if (strcmp(keys[i], "null") == 0) {
      snprintf(content, sizeof(content), "{\"uses\":[],\"%s\":{\"key\":null}}", type);
    } else {
      snprintf(content, sizeof(content), "{\"uses\":[],\"%s\":{\"key\":\"%s\"}}", type, keys[i]);
    }
    write_file(path, content);
  }
  snprintf(path, sizeof(path), "materials/items/%s/item.json", id);
  snprintf(content, sizeof(content), "{\"id\":\"%s\",\"type\":\"%s\",\"current_version\":%d}", id, type, versions);
  write_file(path, content);
}

static void make_motif_body(const char *id, const char *motif_text) {
  char path[256];
  char content[512];
  snprintf(path, sizeof(path), "materials/items/%s", id);
  make_dirs(path);
  snprintf(path, sizeof(path), "materials/items/%s/v1.json", id);
  snprintf(content, sizeof(content), "{\"uses\":[],\"motif\":%s}", motif_text);
  write_file(path, content);
  snprintf(path, sizeof(path), "materials/items/%s/item.json", id);
  snprintf(content, sizeof(content), "{\"id\":\"%s\",\"type\":\"motif\",\"current_version\":1}", id);
  write_file(path, content);
}

static void make_store_markers(int count) {
  static const char *const kNames[] = {"tones-migrated", "segments-migrated", "themes-cleared"};
  make_dirs("materials");
  for (int i = 0; i < count; i++) {
    char path[256];
    snprintf(path, sizeof(path), "materials/%s", kNames[i]);
    write_file(path, "done\n");
  }
}

static void test_data_check(void) {
  printf("-- the data check\n");
  char message[4096];
  reset_case();
  check_quiet(run_check(message, sizeof(message)) && message[0] == '\0', "an empty folder passes");
  check_quiet(!common_path_exists("case/.pre-overhaul-checked"), "and the check writes no marker");

  reset_case();
  make_dirs("materials/projects");
  make_dirs("materials/items");
  check_quiet(run_check(message, sizeof(message)), "a store with projects and no items passes without markers");

  reset_case();
  make_dirs("data");
  write_file("data/corpus.db", "old");
  check_quiet(!run_check(message, sizeof(message)), "an unimported data/corpus.db is refused");
  check_quiet(has(message, "data/corpus.db") && has(message, "collections/.imported-from-corpus-db") && has(message, "pre-overhaul") &&
                  has(message, "then start this one"),
              "the refusal names the database and the import marker, and says what to run first");
  make_dirs("collections");
  write_file("collections/.imported-from-corpus-db", "imported\n");
  check_quiet(run_check(message, sizeof(message)), "data/corpus.db passes once the import marker exists");

  reset_case();
  const char *major[] = {"C major"};
  make_item("mot-a", "motif", major, 1);
  check_quiet(!run_check(message, sizeof(message)), "a motif without store markers is refused");
  check_quiet(has(message, "tones-migrated") && has(message, "segments-migrated") && has(message, "themes-cleared"), "the refusal names all three markers");
  make_store_markers(2);
  check_quiet(!run_check(message, sizeof(message)) && !has(message, "tones-migrated") && has(message, "themes-cleared"),
              "only the missing marker is named");
  make_store_markers(3);
  check_quiet(run_check(message, sizeof(message)), "a motif with all three markers passes");

  reset_case();
  make_store_markers(3);
  const char *old_d[] = {"D minor"};
  const char *old_eb[] = {"C major", "Eb major"};
  const char *lower[] = {"c major"};
  const char *rewritten[] = {"D minor", "C minor"};
  const char *spaced[] = {"  C major "};
  const char *none[] = {"null"};
  const char *theme[] = {"D minor"};
  make_item("mot-old-d", "motif", old_d, 1);
  make_item("mot-old-eb", "motif", old_eb, 2);
  make_item("mot-lower", "motif", lower, 1);
  make_item("mot-rewritten", "motif", rewritten, 2);
  make_item("mot-spaced", "motif", spaced, 1);
  make_item("mot-none", "motif", none, 1);
  make_item("thm-key", "theme", theme, 1);
  check_quiet(!run_check(message, sizeof(message)), "motifs on another tonic are refused");
  check_quiet(has(message, "mot-old-d") && has(message, "mot-old-eb") && has(message, "mot-lower") && !has(message, "mot-rewritten") &&
                  !has(message, "mot-spaced") && !has(message, "mot-none") && !has(message, "thm-key") && has(message, "Rewrite in C"),
              "the current key decides, and a rewritten, spaced or keyless motif and a theme are not named");

  reset_case();
  make_store_markers(3);
  make_motif_body("mot-material", "{\"material\":\"omt.material.natural-minor\"}");
  make_motif_body("mot-old-minor", "{\"key\":\"C minor\"}");
  make_motif_body("mot-material-and-key", "{\"material\":\"omt.material.major\",\"key\":\"D minor\"}");
  check_quiet(run_check(message, sizeof(message)) && message[0] == '\0', "motifs with a material, or the old key text C major or C minor, pass");
  make_motif_body("mot-old-d", "{\"key\":\"D minor\"}");
  check_quiet(!run_check(message, sizeof(message)) && has(message, "mot-old-d") && !has(message, "mot-material"),
              "an old key on another tonic is still refused");

  reset_case();
  write_file(".pre-overhaul-checked", "checked\n");
  make_dirs("data");
  write_file("data/corpus.db", "old");
  check_quiet(run_check(message, sizeof(message)), "a folder with the marker of the GUI before the overhaul passes at once");
  sh("rm -rf case");
}

int main(void) {
  check_quiet(workshop_knowledge_init(), "the knowledge is built");
  test_dry_run();
  test_real_run();
  test_interrupted();
  test_refusals();
  test_data_check();
  workshop_knowledge_shutdown();
  return test_finish("workshop_migrate");
}
