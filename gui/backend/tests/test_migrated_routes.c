/* The GUI's routes on the migrated version 0 fixture (P63 B7, R-642): the fixture (workshop/tests/fixtures/data-v0: every kind and body
 * generation the GUI ever stored) is copied into the working directory, migrated to layout 1 with a backup, and served. Every project,
 * item and Library piece is listed and opened, the subjects, the collection, the style and the link run are read, and the composed
 * theme is composed again from its stored version. The folder is left as it is for workshop_formats, which validates every file of it. */

#include <civetweb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include <ursatz_workshop/workshop_api.h>

#include "base/knowledge.h"
#include "support/server_support.h"
#include "ursatz_check.h"
#include "test_http.h"

#ifndef URSATZ_WORKSHOP_DATA_V0
#error "URSATZ_WORKSHOP_DATA_V0 must name the version 0 fixture folder"
#endif

#define PORT "18795"

static const char *string_of(const cJSON *object, const char *key) {
  const cJSON *item = object ? cJSON_GetObjectItemCaseSensitive(object, key) : NULL;
  return cJSON_IsString(item) ? item->valuestring : "";
}

static double number_of(const cJSON *object, const char *key) {
  const cJSON *item = object ? cJSON_GetObjectItemCaseSensitive(object, key) : NULL;
  return cJSON_IsNumber(item) ? item->valuedouble : -1;
}

static const cJSON *member(const cJSON *object, const char *key) {
  return object ? cJSON_GetObjectItemCaseSensitive(object, key) : NULL;
}

/* GET answered 200 with ok true; the answer (caller deletes) or NULL. */
static cJSON *get_ok(const char *path) {
  int status = 0;
  cJSON *answer = test_http_json(PORT, "GET", path, NULL, &status);
  if (status != 200 || !cJSON_IsTrue(member(answer, "ok"))) {
    cJSON_Delete(answer);
    return NULL;
  }
  return answer;
}

/* The result of the job an accepted request names, polled until it is done. */
static cJSON *job_result(cJSON *accepted, int *status) {
  const char *id = string_of(member(member(accepted, "job"), "id"), "");
  char path[128];
  snprintf(path, sizeof(path), "/api/jobs/%s", cJSON_IsString(member(member(accepted, "job"), "id")) ? member(member(accepted, "job"), "id")->valuestring : id);
  for (int waited = 0; waited < 300000; waited += 100) {
    int poll = 0;
    cJSON *answer = test_http_json(PORT, "GET", path, NULL, &poll);
    const char *state = string_of(member(answer, "job"), "state");
    if (strcmp(state, "queued") != 0 && strcmp(state, "running") != 0) {
      *status = (int)number_of(member(answer, "job"), "http_status");
      cJSON *result = cJSON_DetachItemFromObjectCaseSensitive((cJSON *)member(answer, "job"), "result");
      cJSON_Delete(answer);
      return result;
    }
    cJSON_Delete(answer);
    test_sleep_ms(100);
  }
  *status = 0;
  return NULL;
}

int main(void) {
  /* a fresh working directory holding the migrated fixture */
  char command[2048];
  system("rm -rf ./* ./.[!.]* 2> /dev/null");
  snprintf(command, sizeof(command), "cp -r '%s'/. .", URSATZ_WORKSHOP_DATA_V0);
  check(system(command) == 0, "the fixture is copied");
  check(workshop_knowledge_init(), "the knowledge is built");
  cJSON *report = NULL;
  UrsatzError error = {{0}};
  WorkshopMigrateOptions options = {false, NULL};
  check(workshop_migrate(".", &options, &report, &error) == URSATZ_OK && report, "the fixture is migrated");
  snprintf(command, sizeof(command), "rm -rf '%s'", string_of(report, "backup"));
  cJSON_Delete(report);
  if (system(command) != 0) {
    fprintf(stderr, "the backup could not be removed\n");
  }
  workshop_knowledge_shutdown();

  struct mg_context *ctx = test_gui_server_start(PORT);
  check(ctx != NULL, "the server starts on the migrated folder");
  if (!ctx) {
    return 1;
  }
  test_sleep_ms(200);

  /* every project and item */
  cJSON *projects = get_ok("/api/materials/projects");
  check(cJSON_GetArraySize(member(projects, "projects")) == 2, "both projects are listed");
  int items_total = 0;
  int opened = 0;
  char theme_id[96] = "";
  const cJSON *project = NULL;
  cJSON_ArrayForEach(project, member(projects, "projects")) {
    char path[256];
    snprintf(path, sizeof(path), "/api/materials/items?project=%s", string_of(project, "id"));
    cJSON *list = get_ok(path);
    items_total += cJSON_GetArraySize(member(list, "items"));
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, member(list, "items")) {
      snprintf(path, sizeof(path), "/api/materials/items/%s", string_of(item, "id"));
      cJSON *opened_item = get_ok(path);
      const cJSON *meta = member(opened_item, "item");
      int current = (int)number_of(meta, "current_version");
      bool every_version = current >= 1;
      for (int version = 1; version <= current; version++) {
        snprintf(path, sizeof(path), "/api/materials/items/%s/versions/%d", string_of(item, "id"), version);
        cJSON *body = get_ok(path);
        every_version = every_version && body && cJSON_IsObject(member(body, "body"));
        cJSON_Delete(body);
      }
      opened += opened_item && cJSON_IsObject(member(opened_item, "body")) && strcmp(string_of(meta, "project_id"), string_of(project, "id")) == 0 &&
                every_version && cJSON_GetArraySize(member(meta, "versions")) == current;
      if (strcmp(string_of(item, "name"), "Sentence theme") == 0) {
        snprintf(theme_id, sizeof(theme_id), "%s", string_of(item, "id"));
      }
      cJSON_Delete(opened_item);
    }
    cJSON_Delete(list);
  }
  check(items_total == 11, "the eleven items are listed by their projects");
  check(opened == 11, "every item opens with its body, its project and every one of its versions");
  cJSON_Delete(projects);

  /* the Library pieces */
  cJSON *pieces = get_ok("/api/analyzer/pieces");
  check(cJSON_GetArraySize(member(pieces, "pieces")) == 2, "both Library pieces are listed");
  int piece_ok = 0;
  char piece_id[96] = "";
  const cJSON *piece = NULL;
  cJSON_ArrayForEach(piece, member(pieces, "pieces")) {
    char path[256];
    snprintf(path, sizeof(path), "/api/analyzer/pieces/%s", string_of(piece, "id"));
    cJSON *detail = get_ok(path);
    piece_ok += detail && cJSON_IsObject(member(detail, "document"));
    snprintf(piece_id, sizeof(piece_id), "%s", string_of(piece, "id"));
    cJSON_Delete(detail);
  }
  check(piece_ok == 2, "every Library piece opens with its document");
  cJSON_Delete(pieces);

  /* subjects, the collection, the style and the link run */
  cJSON *subjects = get_ok("/api/subjects");
  check(cJSON_GetArraySize(member(subjects, "subjects")) == 2, "both subjects are listed");
  cJSON_Delete(subjects);
  cJSON *collections = get_ok("/api/corpus");
  check(cJSON_GetArraySize(member(collections, "corpora")) == 1, "the collection is listed");
  cJSON_Delete(collections);
  cJSON *styles = get_ok("/api/styles");
  check(cJSON_GetArraySize(member(styles, "styles")) == 1, "the style is listed");
  char path[256];
  snprintf(path, sizeof(path), "/api/styles/%s", string_of(cJSON_GetArrayItem(member(styles, "styles"), 0), "id"));
  cJSON *style = get_ok(path);
  check(style && number_of(member(style, "style"), "piece_count") == 2, "the style opens with its two pieces");
  cJSON_Delete(style);
  cJSON_Delete(styles);
  cJSON *runs = get_ok("/api/links/runs");
  check(cJSON_GetArraySize(member(runs, "runs")) == 1 && strcmp(string_of(cJSON_GetArrayItem(member(runs, "runs"), 0), "status"), "done") == 0,
        "the finished link run is listed");
  snprintf(path, sizeof(path), "/api/links/runs/%s", string_of(cJSON_GetArrayItem(member(runs, "runs"), 0), "id"));
  cJSON *run = get_ok(path);
  check(run && cJSON_IsObject(member(member(run, "run"), "result")), "the run opens with its result");
  cJSON_Delete(run);
  cJSON_Delete(runs);
  snprintf(path, sizeof(path), "/api/links/pieces/%s", piece_id);
  cJSON *links = get_ok(path);
  check(links && cJSON_IsArray(member(links, "links")), "the links of a piece are read from the run");
  cJSON_Delete(links);

  /* the composed theme is composed again from its stored version */
  check(theme_id[0] != '\0', "the composed theme is found");
  if (theme_id[0]) {
    int status = 0;
    cJSON *accepted = test_gui_send(PORT, "item.compose", theme_id, "{\"seed\":2}", &status);
    cJSON *result = status == 202 ? job_result(accepted, &status) : NULL;
    cJSON_Delete(accepted);
    check(status == 200 && result && cJSON_IsTrue(member(result, "ok")) && number_of(member(result, "item"), "current_version") == 3,
          "the theme composes again from its stored version (version 3)");
    cJSON_Delete(result);
    snprintf(path, sizeof(path), "/api/materials/items/%s/versions/3/files/theme.mid", theme_id);
    char *file = test_http_exchange(PORT, "GET", path, NULL, NULL, 0);
    check(test_http_status(file) == 200 && strstr(file, "audio/midi") != NULL && strlen(test_http_body(file)) > 0 && strncmp(test_http_body(file), "MThd", 4) == 0,
          "the new version has its MIDI file, served as audio/midi");
    free(file);
  }

  /* what the routes write beside the migrated files: a project, a motif and a new version of it, a mark and a subject */
  {
    int status = 0;
    cJSON *made = test_gui_send(PORT, "project.create", "", "{\"name\":\"Written after the migration\"}", &status);
    char project_id[96];
    snprintf(project_id, sizeof(project_id), "%s", string_of(member(made, "project"), "id"));
    cJSON_Delete(made);
    char body[1200];
    snprintf(body, sizeof(body),
             "{\"type\":\"motif\",\"name\":\"Fresh\",\"project_id\":\"%s\",\"body\":{\"uses\":[],\"motif\":{\"role\":\"melodic\","
             "\"form\":\"full\",\"notes\":\"C4:q Eb4:q G4:h\",\"meter\":\"4/4\",\"material\":\"omt.material.natural-minor\",\"pickup\":\"0\","
             "\"features\":[],\"analysis\":null,\"derived\":null}}}",
             project_id);
    made = test_gui_send(PORT, "item.create", "", body, &status);
    check(status == 200 && cJSON_IsTrue(member(made, "ok")), "a motif is saved in a project written after the migration");
    char item_id[96];
    snprintf(item_id, sizeof(item_id), "%s", string_of(member(made, "item"), "id"));
    cJSON_Delete(made);
    snprintf(body, sizeof(body),
             "{\"note\":\"Second\",\"body\":{\"uses\":[],\"motif\":{\"role\":\"melodic\",\"form\":\"full\",\"notes\":\"C4:q D4:q G4:h\","
             "\"meter\":\"4/4\",\"material\":\"omt.material.natural-minor\",\"pickup\":\"0\",\"features\":[],\"analysis\":null,\"derived\":null}}}");
    made = test_gui_send(PORT, "item.save", item_id, body, &status);
    check(status == 200 && number_of(member(made, "item"), "current_version") == 2, "and gets a second version");
    cJSON_Delete(made);
    made = test_gui_send(PORT, "item.set-op-checks", item_id,
                          "{\"checks\":{\"transpose\":{\"verdict\":\"works\",\"comment\":\"fine\",\"seed\":1,\"notes_text\":\"C4\",\"date\":\"2026-11-01\"}}}",
                          &status);
    check(status == 200 && cJSON_IsTrue(member(made, "ok")), "its operation marks are saved");
    cJSON_Delete(made);
    char marks_path[256];
    snprintf(marks_path, sizeof(marks_path), "/api/materials/items/%s/op-checks", item_id);
    cJSON *marks = get_ok(marks_path);
    const cJSON *transpose = member(member(marks, "checks"), "transpose");
    check(marks && strcmp(string_of(transpose, "verdict"), "works") == 0 && strcmp(string_of(transpose, "comment"), "fine") == 0,
          "the marks read back as they were saved");
    cJSON_Delete(marks);
    made = test_gui_send(PORT, "subject.create", "", "{\"name\":\"After\",\"kind\":\"idea\"}", &status);
    check(status == 200 && cJSON_IsTrue(member(made, "ok")) && strcmp(string_of(member(made, "subject"), "name"), "After") == 0 &&
              strcmp(string_of(member(made, "subject"), "kind"), "idea") == 0,
          "a subject is written, with its name and kind");
    cJSON_Delete(made);
  }

  test_gui_server_stop(ctx);
  return test_finish("migrated_routes");
}
