/* The resolver of a stored Composer request (P63 B15, R-650): each reference of a theme or piece request is replaced by what the
 * Composer reads (a motive with the motif's degrees, pickup, harmony, features and tone overrides; the saved forms of a motif; the stored
 * plan of a theme), a reference the body does not use or a motif without degrees is NOT_FOUND at its path, and the style of the brief
 * is the request's style, which a request without a brief does not have. The store is the one of the working directory. */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include <common/files.h>
#include <ursatz_workshop/workshop_api.h>

#include "composition/request_resolve.h"
#include "items/store.h"
#include "test_knowledge.h"
#include "ursatz_check.h"

static const char *text(const cJSON *object, const char *name) {
  const cJSON *item = object ? cJSON_GetObjectItemCaseSensitive(object, name) : NULL;
  return cJSON_IsString(item) ? item->valuestring : "";
}

/* A body with the uses of the motif versions mot-a v1 (degrees, harmony of its own, a tone override) and mot-b v1 (no degrees). */
static cJSON *body_with(const char *request_json) {
  char json[4096];
  snprintf(json, sizeof(json),
           "{\"uses\":[{\"item_id\":\"mot-a\",\"version\":1,\"copy\":{\"motif\":{\"degrees\":\"1:q 3:q\",\"pickup\":\"0\",\"harmony\":\"I\","
           "\"features\":[{\"label\":\"a\",\"first\":0,\"last\":1}],\"tone_overrides\":{\"2\":\"chord\"}}}},"
           "{\"item_id\":\"mot-b\",\"version\":1,\"copy\":{\"motif\":{\"notes\":\"C4:q\"}}}],\"request\":%s,"
           "\"brief\":null}",
           request_json);
  return cJSON_Parse(json);
}

static void test_theme(void) {
  UrsatzError error = {{0}, {0}};
  cJSON *resolved = NULL;
  cJSON *body = body_with("{\"schema\":\"ursatz-theme-request\",\"schema_version\":2,\"key\":\"C\",\"into\":\"sentence\","
                          "\"motives\":[{\"id\":\"M1\",\"$item\":{\"item_id\":\"mot-a\",\"version\":1}}],"
                          "\"slots\":{\"basic\":\"M1\"},\"accompaniment\":{\"pattern\":\"motive\",\"motive\":{\"$item\":{\"item_id\":\"mot-a\",\"version\":1}}},"
                          "\"style_weight\":\"1\",\"without\":[\"motive-rhythm\"],\"tempo\":null}");
  check_quiet(workshop_request_resolve("theme", body, NULL, NULL, &resolved, &error) == URSATZ_OK && resolved, "a theme request resolves");
  const cJSON *motive = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(resolved, "motives"), 0);
  check_quiet(strcmp(text(motive, "id"), "M1") == 0 && strcmp(text(motive, "degrees"), "1:q 3:q") == 0 && strcmp(text(motive, "harmony"), "I") == 0 &&
                  strcmp(text(motive, "role"), "melodic") == 0 && cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(motive, "tone_overrides")) &&
                  !cJSON_GetObjectItemCaseSensitive(motive, "$item"),
              "a motive is the motif's degrees, harmony and tone overrides under its id");
  const cJSON *accompaniment = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(resolved, "accompaniment"), "motive");
  check_quiet(strcmp(text(accompaniment, "role"), "accompaniment") == 0 && !cJSON_GetObjectItemCaseSensitive(accompaniment, "id"),
              "an accompaniment motive has the role accompaniment and no id");
  check_quiet(!cJSON_GetObjectItemCaseSensitive(resolved, "style") && !cJSON_GetObjectItemCaseSensitive(resolved, "style_weight") &&
                  !cJSON_GetObjectItemCaseSensitive(resolved, "without"),
              "without a brief the request has no style, weight or exclusions");
  cJSON_Delete(resolved);
  check_quiet(workshop_request_resolve("theme", body, NULL, "/tmp/brief.style.json", &resolved, &error) == URSATZ_OK &&
                  strcmp(text(resolved, "style"), "/tmp/brief.style.json") == 0 && strcmp(text(resolved, "style_weight"), "1") == 0 &&
                  cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(resolved, "without")) == 1,
              "with a brief the request names its style and keeps the weight and exclusions");
  cJSON_Delete(resolved);
  check_quiet(cJSON_GetObjectItemCaseSensitive(body, "request") && cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(
                  cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(body, "request"), "motives")->child, "$item"), "item_id"),
              "the body is not changed");
  cJSON_Delete(body);

  resolved = NULL;
  body = body_with("{\"motives\":[{\"id\":\"M1\",\"$item\":{\"item_id\":\"mot-a\",\"version\":1}},{\"id\":\"M2\",\"$item\":{\"item_id\":\"mot-a\",\"version\":2}}]}");
  check_quiet(workshop_request_resolve("theme", body, NULL, NULL, &resolved, &error) == URSATZ_ERR_NOT_FOUND && resolved == NULL &&
                  strcmp(error.path, "/motives/1") == 0,
              "a version the body does not use is NOT_FOUND at its motive");
  cJSON_Delete(body);
  body = body_with("{\"motives\":[{\"id\":\"M1\",\"$item\":{\"item_id\":\"mot-b\",\"version\":1}}]}");
  check_quiet(workshop_request_resolve("theme", body, NULL, NULL, &resolved, &error) == URSATZ_ERR_NOT_FOUND && strcmp(error.path, "/motives/0") == 0,
              "a motif without degrees is NOT_FOUND at its motive");
  cJSON_Delete(body);
  body = body_with("{\"accompaniment\":{\"pattern\":\"motive\",\"motive\":{\"$item\":{\"item_id\":\"mot-x\",\"version\":1}}}}");
  check_quiet(workshop_request_resolve("theme", body, NULL, NULL, &resolved, &error) == URSATZ_ERR_NOT_FOUND && strcmp(error.path, "/accompaniment/motive") == 0,
              "an unknown accompaniment motif is NOT_FOUND at the accompaniment");
  cJSON_Delete(body);
  body = cJSON_Parse("{\"uses\":[]}");
  check_quiet(workshop_request_resolve("theme", body, NULL, NULL, &resolved, &error) == URSATZ_ERR_REQUEST && strcmp(error.path, "/request") == 0,
              "a body with no request is refused at /request");
  cJSON_Delete(body);
}

static void test_piece(const char *theme_id) {
  UrsatzError error = {{0}, {0}};
  cJSON *resolved = NULL;
  char request[2048];
  snprintf(request, sizeof(request),
           "{\"schema\":\"ursatz-piece-request\",\"schema_version\":2,\"form\":\"game-loop\",\"motive\":{\"$item\":{\"item_id\":\"mot-a\",\"version\":1}},"
           "\"sections\":[{\"id\":\"s0\",\"part\":\"intro\",\"fill\":{\"kind\":\"empty\"}},"
           "{\"id\":\"s1\",\"part\":\"body\",\"fill\":{\"kind\":\"motif\",\"motive\":{\"$item\":{\"item_id\":\"mot-a\",\"version\":1}},"
           "\"forms\":{\"$pool\":{\"item_id\":\"mot-a\",\"pool\":\"approved\"}},\"into\":\"sentence\"}},"
           "{\"id\":\"s2\",\"part\":\"turnaround\",\"fill\":{\"kind\":\"theme\",\"plan\":{\"$item\":{\"item_id\":\"%s\",\"version\":1,\"file\":\"plan.json\"}}}}]}",
           theme_id);
  cJSON *body = body_with(request);
  char dir[512];
  snprintf(dir, sizeof(dir), "stage");
  check_quiet(common_dir_make(dir, NULL) == URSATZ_OK, "the stage folder is made");
  char version_dir[512];
  char plan[640];
  check_quiet(workshop_material_version_dir(theme_id, 1, version_dir, sizeof(version_dir)), "the theme has a version folder");
  snprintf(plan, sizeof(plan), "%s/plan.json", version_dir);
  check_quiet(common_file_write_atomic(plan, "{\"plan\":true}\n", 14, NULL) == URSATZ_OK, "its plan is written");
  check_quiet(workshop_request_resolve("piece", body, dir, NULL, &resolved, &error) == URSATZ_OK && resolved, "a piece request resolves");
  const cJSON *sections = cJSON_GetObjectItemCaseSensitive(resolved, "sections");
  const cJSON *motif_fill = cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(sections, 1), "fill");
  check_quiet(strcmp(text(cJSON_GetObjectItemCaseSensitive(resolved, "motive"), "degrees"), "1:q 3:q") == 0 &&
                  !cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(resolved, "motive"), "id") &&
                  strcmp(text(cJSON_GetObjectItemCaseSensitive(motif_fill, "motive"), "degrees"), "1:q 3:q") == 0,
              "the piece motive and a motif fill's motive are motives with no id");
  check_quiet(cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(motif_fill, "forms")) && cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(motif_fill, "forms")) == 0 &&
                  strcmp(text(motif_fill, "into"), "sentence") == 0,
              "the saved forms of a motif are the pool (none saved here) and the fill keeps its other members");
  const char *copied = text(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(sections, 2), "fill"), "plan");
  check_quiet(strcmp(copied, "stage/theme-s2.plan.json") == 0 && common_path_exists(copied), "the plan of a theme fill is copied into the stage as theme-<section>.plan.json");
  cJSON_Delete(resolved);
  check_quiet(workshop_request_resolve("piece", body, NULL, NULL, &resolved, &error) == URSATZ_OK &&
                  strcmp(text(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(resolved, "sections"), 2), "fill"), "plan"), plan) == 0,
              "without a stage the request names the plan of the theme version itself");
  cJSON_Delete(resolved);
  cJSON_Delete(body);
  snprintf(request, sizeof(request),
           "{\"sections\":[{\"id\":\"s0\",\"fill\":{\"kind\":\"theme\",\"plan\":{\"$item\":{\"item_id\":\"%s\",\"version\":9,\"file\":\"plan.json\"}}}}]}", theme_id);
  body = body_with(request);
  check_quiet(workshop_request_resolve("piece", body, dir, NULL, &resolved, &error) == URSATZ_ERR_NOT_FOUND && strcmp(error.path, "/sections/0/fill/plan") == 0,
              "a theme version with no plan file is NOT_FOUND at its plan");
  cJSON_Delete(body);
  body = body_with("{\"sections\":[{\"id\":\"s0\",\"fill\":{\"kind\":\"motif\",\"motive\":{\"$item\":{\"item_id\":\"mot-a\",\"version\":3}}}}]}");
  check_quiet(workshop_request_resolve("piece", body, NULL, NULL, &resolved, &error) == URSATZ_ERR_NOT_FOUND && strcmp(error.path, "/sections/0/fill/motive") == 0,
              "a motif version the body does not use is NOT_FOUND at its fill's motive");
  cJSON_Delete(body);
}

int main(void) {
  if (system("rm -rf stage projects") != 0) {
    return 1;
  }
  test_knowledge();
  check_quiet(workshop_material_service_init(), "the store starts");
  cJSON *projects = NULL;
  check_quiet(workshop_material_project_list(&projects) == URSATZ_OK && cJSON_GetArraySize(projects) > 0, "a project exists");
  cJSON *placeholder = cJSON_Parse("{\"uses\":[],\"request\":{}}");
  cJSON *item = NULL;
  check_quiet(workshop_material_item_create_noted("theme", "Theme", text(cJSON_GetArrayItem(projects, 0), "id"), placeholder, "Created", &item) == URSATZ_OK,
              "a theme is made");
  char theme_id[64];
  snprintf(theme_id, sizeof(theme_id), "%s", text(item, "id"));
  cJSON_Delete(item);
  cJSON_Delete(placeholder);
  cJSON_Delete(projects);
  test_theme();
  test_piece(theme_id);
  return test_finish("workshop_request_resolve");
}
