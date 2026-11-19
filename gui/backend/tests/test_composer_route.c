/* The Composer route is the status check only: it reports the versions the
 * linked Composer was built with (composer_api_versions), and the old composition and
 * grow/plan routes are gone (404). Uses the shared HTTP client
 * (tests/support/test_http.h). */

#include <civetweb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include "server.h"
#include "gui_check.h"
#include "test_http.h"
#include "ursatz_composer/composer_api.h"

#define PORT "18768"

int main(void) {
  struct mg_context *ctx = ursatz_gui_server_start(PORT, ".");
  check(ctx != NULL, "server starts");
  if (!ctx) {
    return 1;
  }
  test_sleep_ms(200);

  int status = 0;
  char where[256] = "";
  cJSON *json = test_http_json(PORT, "GET", "/api/composer/status", NULL, &status);
  const cJSON *version_text = json ? cJSON_GetObjectItemCaseSensitive(json, "version") : NULL;
  check(status == 200 && cJSON_IsString(version_text) && strncmp(version_text->valuestring, "ursatz-composer", 15) == 0,
        "status reports the Composer version");
  const cJSON *api = cJSON_GetObjectItemCaseSensitive(json, "api_version");
  const cJSON *analyzer = cJSON_GetObjectItemCaseSensitive(json, "analyzer_version");
  const cJSON *commit = cJSON_GetObjectItemCaseSensitive(json, "producer_key");
  check(cJSON_IsString(api) && strcmp(api->valuestring, composer_api_version()) == 0,
        "status reports the API version the backend is linked with");
  check(cJSON_IsString(analyzer) && analyzer->valuestring[0] != '\0', "status reports the Analyzer version");
  check(cJSON_IsString(commit) && commit->valuestring[0] != '\0', "status reports the Ursatz commit");
  cJSON_Delete(json);

  json = test_http_json(PORT, "POST", "/api/composer/status", "{}", &status);
  check(status == 405, "status accepts only GET");
  cJSON_Delete(json);

  /* The vocabulary: the Composer's lists, kept for the process, equal to the file the build generates for the frontend
   * tests. */
  json = test_http_json(PORT, "GET", "/api/composer/vocabulary", NULL, &status);
  check(status == 200 && cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(json, "meters")) &&
            cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(json, "pitch_class_names")) &&
            cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(json, "limits")),
        "the vocabulary route gives the Composer's lists");
  cJSON *again = test_http_json(PORT, "GET", "/api/composer/vocabulary", NULL, &status);
  check(status == 200 && cJSON_Compare(json, again, 1), "the vocabulary is the same on every call");
  cJSON_Delete(again);
  char *fixture_text = test_read_file(URSATZ_GUI_TEST_VOCABULARY_FIXTURE, NULL);
  cJSON *fixture = fixture_text ? cJSON_Parse(fixture_text) : NULL;
  free(fixture_text);
  check(fixture && test_json_equal(fixture, json, where, sizeof where),
        "the generated vocabulary fixture is what the route gives");
  cJSON_Delete(fixture);
  cJSON_Delete(json);
  json = test_http_json(PORT, "POST", "/api/composer/vocabulary", "{}", &status);
  check(status == 405, "the vocabulary accepts only GET");
  cJSON_Delete(json);

  /* The forms: the Composer's built-in forms, kept for the process (R-430). */
  json = test_http_json(PORT, "GET", "/api/composer/forms", NULL, &status);
  bool has_game_loop = false;
  bool has_rounded_binary = false;
  const cJSON *form = NULL;
  cJSON_ArrayForEach(form, cJSON_GetObjectItemCaseSensitive(json, "forms")) {
    const cJSON *form_name = cJSON_GetObjectItemCaseSensitive(form, "name");
    has_game_loop = has_game_loop || (cJSON_IsString(form_name) && strcmp(form_name->valuestring, "game-loop") == 0);
    has_rounded_binary = has_rounded_binary || (cJSON_IsString(form_name) && strcmp(form_name->valuestring, "rounded-binary") == 0);
  }
  check(status == 200 && has_game_loop && has_rounded_binary, "the forms route lists the game loop and the rounded binary");
  again = test_http_json(PORT, "GET", "/api/composer/forms", NULL, &status);
  check(status == 200 && cJSON_Compare(json, again, 1), "the forms are the same on every call");
  cJSON_Delete(again);
  /* The generated forms are 300 KB, so they are read whole, not into a fixed buffer. */
  fixture_text = test_read_file(URSATZ_GUI_TEST_FORMS_FIXTURE, NULL);
  fixture = fixture_text ? cJSON_Parse(fixture_text) : NULL;
  free(fixture_text);
  check(fixture && test_json_equal(fixture, json, where, sizeof where), "the generated forms fixture is what the route gives");
  cJSON_Delete(fixture);
  cJSON_Delete(json);
  json = test_http_json(PORT, "POST", "/api/composer/forms", "{}", &status);
  check(status == 405, "the forms accept only GET");
  cJSON_Delete(json);

  json = test_http_json(PORT, "GET", "/api/composer/compositions", NULL, &status);
  check(status == 404, "the compositions route is gone");
  cJSON_Delete(json);
  json = test_http_json(PORT, "POST", "/api/composer/grow", "{}", &status);
  check(status == 404, "the grow route is gone");
  cJSON_Delete(json);
  json = test_http_json(PORT, "POST", "/api/composer/plan", "{}", &status);
  check(status == 404, "the plan route is gone");
  cJSON_Delete(json);

  ursatz_gui_server_stop(ctx);
  return test_finish("composer_route");
}
