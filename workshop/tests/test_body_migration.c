/* The migration of the theme and piece bodies to stored Composer requests (P63 B15, R-650). Every theme and piece body the route tests and
 * the migrated version 0 folder composed (fixtures/requests, recorded before the change by fixtures/record-requests.sh) is migrated and
 * resolved, and the request is the one the old translation built, member for member: the composed files do not move. A layout 1 folder
 * is refused by workshop_open and migrates in place to layout 2 after a backup; the run is a no-op the second time and a run that stopped
 * is finished by the next. */

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

#include "items/body_requests.h"
#include "composition/request_resolve.h"
#include "items/store.h"
#include "test_knowledge.h"
#include "ursatz_check.h"
#include "command_context_support.h"

#ifndef URSATZ_WORKSHOP_DATA_V0
#error "URSATZ_WORKSHOP_DATA_V0 must name the version 0 fixture folder"
#endif
#ifndef URSATZ_WORKSHOP_REQUESTS
#error "URSATZ_WORKSHOP_REQUESTS must name the folder of the recorded requests"
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

static const char *text(const cJSON *object, const char *name) {
  const cJSON *item = object ? cJSON_GetObjectItemCaseSensitive(object, name) : NULL;
  return cJSON_IsString(item) ? item->valuestring : "";
}

static double number(const cJSON *object, const char *name) {
  const cJSON *item = object ? cJSON_GetObjectItemCaseSensitive(object, name) : NULL;
  return cJSON_IsNumber(item) ? item->valuedouble : -1;
}

/* The request with what a store decides left out: the saved forms of a motif and the path of a theme's plan (the fixtures were composed
   in folders of their own). */
static void normalize(cJSON *request) {
  cJSON *section = NULL;
  cJSON_ArrayForEach(section, cJSON_GetObjectItemCaseSensitive(request, "sections")) {
    cJSON *fill = cJSON_GetObjectItemCaseSensitive(section, "fill");
    if (cJSON_GetObjectItemCaseSensitive(fill, "forms")) {
      cJSON_ReplaceItemInObjectCaseSensitive(fill, "forms", cJSON_CreateArray());
    }
    if (cJSON_GetObjectItemCaseSensitive(fill, "plan")) {
      cJSON_ReplaceItemInObjectCaseSensitive(fill, "plan", cJSON_CreateString("plan"));
    }
  }
}

/* The plan of a theme fill names a theme version that has a folder in the store: the test's own theme stands in for it. */
static void point_plans_at(cJSON *request, const char *theme_id) {
  cJSON *section = NULL;
  cJSON_ArrayForEach(section, cJSON_GetObjectItemCaseSensitive(request, "sections")) {
    cJSON *plan = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(section, "fill"), "plan");
    cJSON *reference = cJSON_GetObjectItemCaseSensitive(plan, "$item");
    if (reference) {
      cJSON_ReplaceItemInObjectCaseSensitive(reference, "item_id", cJSON_CreateString(theme_id));
      cJSON_ReplaceItemInObjectCaseSensitive(reference, "version", cJSON_CreateNumber(1));
    }
  }
}

static void test_recorded_requests(const char *theme_id) {
  CommonNames names = {0};
  check_quiet(common_dir_list(URSATZ_WORKSHOP_REQUESTS, &names, NULL) == URSATZ_OK && names.count >= 30, "the recorded requests are there");
  int equal = 0;
  int pieces = 0;
  int themes = 0;
  for (size_t i = 0; i < names.count; i++) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", URSATZ_WORKSHOP_REQUESTS, names.items[i]);
    cJSON *record = test_read_json(path);
    const char *kind = text(record, "kind");
    cJSON *body = cJSON_DetachItemFromObjectCaseSensitive(record, "body");
    cJSON *recorded = cJSON_DetachItemFromObjectCaseSensitive(record, "request");
    cJSON *resolved = NULL;
    UrsatzError error = {{0}, {0}};
    bool migrated = body && recorded && workshop_body_requests_migrate(body) && cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(body, "request")) &&
                    !cJSON_GetObjectItemCaseSensitive(body, kind);
    if (migrated) {
      point_plans_at(cJSON_GetObjectItemCaseSensitive(body, "request"), theme_id);
      const cJSON *style = cJSON_GetObjectItemCaseSensitive(recorded, "style");
      migrated = workshop_request_resolve(kind, body, NULL, cJSON_IsString(style) ? style->valuestring : NULL, &resolved, &error) == URSATZ_OK;
    }
    if (migrated) {
      normalize(recorded);
      normalize(resolved);
      if (cJSON_Compare(recorded, resolved, 1)) {
        equal++;
      } else {
        char *want = cJSON_PrintUnformatted(recorded);
        char *got = cJSON_PrintUnformatted(resolved);
        fprintf(stderr, "%s differs\nrecorded: %.1500s\nresolved: %.1500s\n", names.items[i], want, got);
        free(want);
        free(got);
      }
    } else {
      fprintf(stderr, "%s: %s (%s)\n", names.items[i], error.message, error.path);
    }
    pieces += strcmp(kind, "piece") == 0;
    themes += strcmp(kind, "theme") == 0;
    cJSON_Delete(record);
    cJSON_Delete(body);
    cJSON_Delete(recorded);
    cJSON_Delete(resolved);
  }
  check_quiet(themes > 10 && pieces > 10, "the recordings hold themes and pieces");
  check_quiet(equal == (int)names.count, "every recorded body migrates and resolves to the request it composed with, member for member");
  common_names_free(&names);
}

/* The theme item of the v0 fixture, and its project. */
static void find_theme(const char *old_root, char *id, size_t id_size, char *project, size_t project_size) {
  CommonNames names = {0};
  char folder[1024];
  snprintf(folder, sizeof(folder), "%s/materials/items", old_root);
  id[0] = '\0';
  if (common_dir_list(folder, &names, NULL) == URSATZ_OK) {
    for (size_t i = 0; i < names.count && id[0] == '\0'; i++) {
      if (strncmp(names.items[i], "thm-", 4) == 0) {
        snprintf(id, id_size, "%s", names.items[i]);
        char path[1100];
        snprintf(path, sizeof(path), "%s/%s/item.json", folder, names.items[i]);
        cJSON *item = test_read_json(path);
        snprintf(project, project_size, "%s", text(item, "project_id"));
        cJSON_Delete(item);
      }
    }
  }
  common_names_free(&names);
}

static void test_layout_one(void) {
  /* the v0 fixture migrates to layout 2 in one run (test_migrate); this folder is then made a layout 1 folder again: the marker of version 1
     and one theme body written as the services wrote it */
  sh("rm -rf one one-backup-* && cp -r '%s' one", URSATZ_WORKSHOP_DATA_V0);
  UrsatzError error = {{0}, {0}};
  cJSON *report = NULL;
  WorkshopMigrateOptions options = {false, NULL};
  check_quiet(workshop_migrate("one", &options, &report, &error) == URSATZ_OK && number(report, "to_layout") == 2, "the version 0 folder migrates to layout 2");
  cJSON_Delete(report);
  sh("rm -rf one-backup-*");

  char theme[64];
  char project[64];
  find_theme(URSATZ_WORKSHOP_DATA_V0, theme, sizeof(theme), project, sizeof(project));
  char old_path[1200];
  char new_path[1200];
  snprintf(old_path, sizeof(old_path), "%s/materials/items/%s/v1.json", URSATZ_WORKSHOP_DATA_V0, theme);
  snprintf(new_path, sizeof(new_path), "one/projects/%s/items/%s/v1/body.json", project, theme);
  cJSON *migrated = test_read_json(new_path);
  cJSON *old_body = test_read_json(old_path);
  check_quiet(migrated && old_body && cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(migrated, "request")) && number(migrated, "format_version") == 2,
              "the theme body is a stored request after the migration");
  /* the body as the services wrote it in layout 1: the old members under format 1 */
  cJSON_AddStringToObject(old_body, "format", "ursatz-theme-body");
  cJSON_AddNumberToObject(old_body, "format_version", 1);
  workshop_material_body_migrate(old_body);
  char *old_text = cJSON_Print(old_body);
  check_quiet(old_text && common_file_write_atomic(new_path, old_text, strlen(old_text), NULL) == URSATZ_OK, "the theme body is written as format 1");
  free(old_text);
  check_quiet(common_file_write_atomic("one/data-folder.json", "{\"format\":\"ursatz-data-folder\",\"format_version\":1,\"created_at\":\"2026-01-01T00:00:00Z\"}\n", 91, NULL) == URSATZ_OK,
              "the marker names layout 1");

  WorkshopConfig config = {"one", NULL, 9203, 2, 2};
  WorkshopConfig resolved;
  char storage[512];
  Workshop *workshop = NULL;
  check_quiet(workshop_config_resolve(&config, &resolved, storage, sizeof(storage), &error) == URSATZ_OK, "settings resolve");
  check_quiet(workshop_open(&resolved, &workshop, &error) == URSATZ_ERR_FORMAT && workshop == NULL && strstr(error.message, "layout 1") != NULL &&
                  strstr(error.message, "migrate --dry-run") != NULL,
              "a layout 1 folder is refused by open, saying how to back it up and migrate it");

  options.dry_run = true;
  check_quiet(workshop_migrate("one", &options, &report, &error) == URSATZ_OK && number(report, "from_layout") == 1 && number(report, "to_layout") == 2 &&
                  number(cJSON_GetObjectItemCaseSensitive(report, "counts"), "bodies_rewritten") == 1,
              "a dry run counts the one body it would rewrite");
  cJSON_Delete(report);
  cJSON *still = test_read_json(new_path);
  check_quiet(number(still, "format_version") == 1 && !cJSON_GetObjectItemCaseSensitive(still, "request"), "and writes nothing");
  cJSON_Delete(still);

  options.dry_run = false;
  report = NULL;
  check_quiet(workshop_migrate("one", &options, &report, &error) == URSATZ_OK && number(report, "from_layout") == 1 &&
                  cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(report, "changed")) && cJSON_IsString(cJSON_GetObjectItemCaseSensitive(report, "backup")),
              "the run reports layout 1 to 2 and its backup");
  cJSON_Delete(report);
  cJSON *after = test_read_json(new_path);
  check_quiet(number(after, "format_version") == 2 && cJSON_Compare(cJSON_GetObjectItemCaseSensitive(after, "request"), cJSON_GetObjectItemCaseSensitive(migrated, "request"), 1) &&
                  !cJSON_GetObjectItemCaseSensitive(after, "theme"),
              "the theme body is the request the migration of layout 0 made");
  cJSON_Delete(after);
  cJSON *marker = test_read_json("one/data-folder.json");
  check_quiet(number(marker, "format_version") == 2 && number(marker, "migrated_from") == 1, "the marker names layout 2 and where it came from");
  cJSON_Delete(marker);
  check_quiet(workshop_open(&resolved, &workshop, &error) == URSATZ_OK && workshop, "the folder opens");
  workshop_close(workshop);
  report = NULL;
  check_quiet(workshop_migrate("one", &options, &report, &error) == URSATZ_OK && !cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(report, "changed")),
              "a second run is a no-op");
  cJSON_Delete(report);
  cJSON_Delete(migrated);
  cJSON_Delete(old_body);
  sh("rm -rf one-backup-*");
}

int main(void) {
    command_context_support_begin(); /* the services are called directly: a write needs a command (command_context_support.h) */
  sh("rm -rf stage projects");
  test_knowledge();
  check_quiet(workshop_material_service_init(), "the store starts");
  cJSON *projects = NULL;
  check_quiet(workshop_material_project_list(&projects) == URSATZ_OK && cJSON_GetArraySize(projects) > 0, "a project exists");
  cJSON *placeholder = cJSON_Parse("{\"uses\":[],\"request\":{}}");
  cJSON *item = NULL;
  check_quiet(workshop_material_item_create_noted("theme", "Theme", text(cJSON_GetArrayItem(projects, 0), "id"), placeholder, "Created", &item) == URSATZ_OK,
              "a theme with a version folder is made");
  char theme_id[64];
  snprintf(theme_id, sizeof(theme_id), "%s", text(item, "id"));
  cJSON_Delete(item);
  cJSON_Delete(placeholder);
  cJSON_Delete(projects);
  test_recorded_requests(theme_id);
  workshop_material_service_shutdown();
  test_layout_one();
  return test_finish("workshop_body_migration");
}
