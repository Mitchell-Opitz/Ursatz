/* Subject routes end to end: subjects and their links to items and Library
 * pieces, relations, the groups view, and the subjects returned with an item
 * and with the Analyzer piece detail. Uses the shared HTTP client
 * (tests/support/test_http.h). Runs in its own working directory
 * (pieces/, materials/ and subjects/ are created there). */


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


static const char *string_of(const cJSON *object, const char *key) {
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
  return cJSON_IsString(item) ? item->valuestring : "";
}

static double number_of(const cJSON *object, const char *key) {
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
  return cJSON_IsNumber(item) ? item->valuedouble : -1;
}

static bool ok_true(const cJSON *json) {
  return cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(json, "ok"));
}


#include "support/server_support.h"
#include "gui_check.h"
#include "server.h"
#include "test_http.h"

#define PORT "18771"

static const char *kMotifBody =
    "{\"uses\":[],\"motif\":{\"role\":\"melodic\",\"form\":\"full\",\"notes\":\"C4:q Eb4:e G4:e C5:h\","
    "\"meter\":\"4/4\",\"key\":\"C minor\",\"pickup\":\"0\",\"features\":[{\"label\":\"a\",\"first\":0,\"last\":2}],"
    "\"analysis\":null,\"derived\":null}}";

/* Creates a subject and returns its id (caller frees), or NULL. */
static char *create_subject(const char *name, const char *kind) {
  char request[512];
  int status = 0;
  snprintf(request, sizeof(request), "{\"name\":\"%s\",\"kind\":\"%s\",\"note\":\"n\"}", name, kind);
  cJSON *answer = test_http_json(PORT, "POST", "/api/subjects", request, &status);
  char *id = status == 200 ? strdup(string_of(cJSON_GetObjectItemCaseSensitive(answer, "subject"), "id")) : NULL;
  cJSON_Delete(answer);
  return id;
}

static bool array_has(const cJSON *array, const char *text) {
  const cJSON *item = NULL;
  cJSON_ArrayForEach(item, array) {
    if (cJSON_IsString(item) && strcmp(item->valuestring, text) == 0) {
      return true;
    }
  }
  return false;
}

static void test_subject_crud(char **out_id) {
  int status = 0;
  cJSON *answer = test_http_json(PORT, "POST", "/api/subjects", "{\"name\":\"Anna\",\"kind\":\"robot\"}", &status);
  check(status == 400 && !ok_true(answer), "a subject with an unknown kind is 400");
  cJSON_Delete(answer);
  answer = test_http_json(PORT, "POST", "/api/subjects", "{\"kind\":\"idea\"}", &status);
  check(status == 400, "a subject without a name is 400");
  cJSON_Delete(answer);

  char *anna = create_subject("Anna", "character");
  check(anna && strncmp(anna, "sub-", 4) == 0, "a subject is created");
  if (!anna) {
    return;
  }
  char path[256];
  snprintf(path, sizeof(path), "/api/subjects/%s", anna);

  answer = test_http_json(PORT, "GET", path, NULL, &status);
  check(status == 200 && strcmp(string_of(cJSON_GetObjectItemCaseSensitive(answer, "subject"), "name"), "Anna") == 0,
        "a subject is read by id");
  cJSON_Delete(answer);
  answer = test_http_json(PORT, "PATCH", path, "{\"name\":\"Anna K\",\"kind\":\"idea\"}", &status);
  check(status == 200 && strcmp(string_of(cJSON_GetObjectItemCaseSensitive(answer, "subject"), "kind"), "idea") == 0,
        "a subject is renamed and re-kinded");
  cJSON_Delete(answer);
  answer = test_http_json(PORT, "PATCH", path, "{}", &status);
  check(status == 400, "a patch with nothing to change is 400");
  cJSON_Delete(answer);
  answer = test_http_json(PORT, "GET", "/api/subjects/sub-0000000000000000", NULL, &status);
  check(status == 404, "an unknown subject is 404");
  cJSON_Delete(answer);
  answer = test_http_json(PORT, "GET", "/api/subjects", NULL, &status);
  check(status == 200 && cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(answer, "subjects")) == 1,
        "the list holds the subject");
  cJSON_Delete(answer);
  cJSON_Delete(test_http_json(PORT, "PATCH", path, "{\"kind\":\"character\"}", &status));
  *out_id = anna;
}

static void test_relations(const char *anna) {
  int status = 0;
  char *tower = create_subject("The tower", "place");
  char path[400];
  snprintf(path, sizeof(path), "/api/subjects/%s/related/%s", anna, tower);

  cJSON *answer = test_http_json(PORT, "POST", path, NULL, &status);
  check(status == 200 && !cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(answer, "already")) &&
            array_has(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(answer, "subject"), "related"), tower),
        "two subjects are related");
  cJSON_Delete(answer);
  answer = test_http_json(PORT, "POST", path, NULL, &status);
  check(status == 200 && cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(answer, "already")), "relating twice reports already");
  cJSON_Delete(answer);

  char other_side[400];
  snprintf(other_side, sizeof(other_side), "/api/subjects/%s", tower);
  answer = test_http_json(PORT, "GET", other_side, NULL, &status);
  check(array_has(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(answer, "subject"), "related"), anna),
        "the relation is stored on both sides");
  cJSON_Delete(answer);

  char self_path[400];
  snprintf(self_path, sizeof(self_path), "/api/subjects/%s/related/%s", anna, anna);
  answer = test_http_json(PORT, "POST", self_path, NULL, &status);
  check(status == 400, "relating a subject to itself is 400");
  cJSON_Delete(answer);

  answer = test_http_json(PORT, "DELETE", path, NULL, &status);
  check(status == 200 && ok_true(answer), "a relation is removed");
  cJSON_Delete(answer);
  answer = test_http_json(PORT, "DELETE", path, NULL, &status);
  check(status == 404, "removing it again is 404");
  cJSON_Delete(answer);

  /* The groups view: four kinds, in order. */
  answer = test_http_json(PORT, "GET", "/api/subjects/groups", NULL, &status);
  const cJSON *groups = cJSON_GetObjectItemCaseSensitive(answer, "groups");
  check(status == 200 && cJSON_GetArraySize(groups) == 4 &&
            strcmp(string_of(cJSON_GetArrayItem(groups, 0), "kind"), "character") == 0 &&
            strcmp(string_of(cJSON_GetArrayItem(groups, 1), "kind"), "place") == 0,
        "the groups come in kind order");
  check(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(groups, 1), "subjects")) == 1,
        "a place subject is in the place group");
  cJSON_Delete(answer);

  snprintf(path, sizeof(path), "/api/subjects/%s", tower);
  cJSON_Delete(test_http_json(PORT, "DELETE", path, NULL, &status));
  check(status == 200, "a subject is deleted");
  free(tower);
}

static void test_links(const char *anna, const char *piece) {
  int status = 0;
  cJSON *answer = test_http_json(PORT, "GET", "/api/materials/projects", NULL, &status);
  char project[64];
  snprintf(project, sizeof(project), "%s",
           string_of(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(answer, "projects"), 0), "id"));
  cJSON_Delete(answer);

  char request[1024];
  snprintf(request, sizeof(request), "{\"type\":\"motif\",\"name\":\"Opening\",\"project_id\":\"%s\",\"body\":%s}", project,
           kMotifBody);
  answer = test_http_json(PORT, "POST", "/api/materials/items", request, &status);
  char item[64];
  snprintf(item, sizeof(item), "%s", string_of(cJSON_GetObjectItemCaseSensitive(answer, "item"), "id"));
  cJSON_Delete(answer);

  char path[400];
  snprintf(path, sizeof(path), "/api/subjects/%s/items/%s", anna, item);
  answer = test_http_json(PORT, "POST", path, NULL, &status);
  check(status == 200 && !cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(answer, "already")) &&
            array_has(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(answer, "subject"), "items"), item),
        "an item is linked to a subject");
  cJSON_Delete(answer);
  answer = test_http_json(PORT, "POST", path, NULL, &status);
  check(status == 200 && cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(answer, "already")), "linking an item twice reports already");
  cJSON_Delete(answer);
  char missing[400];
  snprintf(missing, sizeof(missing), "/api/subjects/%s/items/mot-0000000000000000", anna);
  answer = test_http_json(PORT, "POST", missing, NULL, &status);
  check(status == 404, "linking an unknown item is 404");
  cJSON_Delete(answer);

  /* A variant belongs to its motif and has no subjects (R-187). */
  char variant_request[1600];
  snprintf(variant_request, sizeof(variant_request),
           "{\"type\":\"motif\",\"name\":\"Opening v1\",\"project_id\":\"%s\",\"body\":"
           "{\"uses\":[],\"motif\":{\"role\":\"melodic\",\"form\":\"full\",\"notes\":\"C4:q Eb4:e G4:e C5:h\","
           "\"meter\":\"4/4\",\"key\":\"C minor\",\"pickup\":\"0\",\"features\":[],\"analysis\":null,"
           "\"derived\":{\"source_item\":\"%s\",\"source_version\":1,\"operations\":[],\"seed\":1,\"kind\":\"variant\","
           "\"kept\":[],\"changed\":[],\"harmony\":null,\"functions\":[],\"flag\":\"unflagged\"}}}}",
           project, item);
  answer = test_http_json(PORT, "POST", "/api/materials/items", variant_request, &status);
  char variant[64];
  snprintf(variant, sizeof(variant), "%s", string_of(cJSON_GetObjectItemCaseSensitive(answer, "item"), "id"));
  cJSON_Delete(answer);
  check(status == 200 && variant[0] != '\0', "a variant is created");
  char variant_path[400];
  snprintf(variant_path, sizeof(variant_path), "/api/subjects/%s/items/%s", anna, variant);
  answer = test_http_json(PORT, "POST", variant_path, NULL, &status);
  check(status == 400 && strcmp(string_of(answer, "error"), "variants have no subjects; save it as a new motif first") == 0,
        "linking a variant to a subject is refused with 400");
  cJSON_Delete(answer);
  snprintf(path, sizeof(path), "/api/materials/items/%s", variant);
  cJSON_Delete(test_http_json(PORT, "DELETE", path, NULL, &status));
  check(status == 200, "the variant is deleted again");

  snprintf(path, sizeof(path), "/api/materials/items/%s", item);
  answer = test_http_json(PORT, "GET", path, NULL, &status);
  const cJSON *subjects = cJSON_GetObjectItemCaseSensitive(answer, "subjects");
  check(cJSON_GetArraySize(subjects) == 1 && strcmp(string_of(cJSON_GetArrayItem(subjects, 0), "id"), anna) == 0 &&
            strcmp(string_of(cJSON_GetArrayItem(subjects, 0), "name"), "Anna K") == 0,
        "the item answer lists the subject that links it");
  cJSON_Delete(answer);

  char piece_link[400];
  snprintf(piece_link, sizeof(piece_link), "/api/subjects/%s/pieces/%s", anna, piece);
  answer = test_http_json(PORT, "POST", piece_link, NULL, &status);
  check(status == 200 && array_has(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(answer, "subject"), "pieces"), piece),
        "a Library piece is linked to a subject");
  cJSON_Delete(answer);
  snprintf(missing, sizeof(missing), "/api/subjects/%s/pieces/nope", anna);
  answer = test_http_json(PORT, "POST", missing, NULL, &status);
  check(status == 404 || status == 400, "linking an unknown piece is refused");
  cJSON_Delete(answer);

  char piece_path[300];
  snprintf(piece_path, sizeof(piece_path), "/api/analyzer/pieces/%s", piece);
  answer = test_http_json(PORT, "GET", piece_path, NULL, &status);
  subjects = cJSON_GetObjectItemCaseSensitive(answer, "subjects");
  check(ok_true(answer) && cJSON_GetArraySize(subjects) == 1 && strcmp(string_of(cJSON_GetArrayItem(subjects, 0), "id"), anna) == 0,
        "the piece detail lists the subject that links it");
  cJSON_Delete(answer);

  answer = test_http_json(PORT, "DELETE", piece_link, NULL, &status);
  check(status == 200 && !array_has(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(answer, "subject"), "pieces"), piece),
        "a piece link is removed");
  cJSON_Delete(answer);
  answer = test_http_json(PORT, "DELETE", piece_link, NULL, &status);
  check(status == 404, "removing a link that is not there is 404");
  cJSON_Delete(answer);
  answer = test_http_json(PORT, "GET", piece_path, NULL, &status);
  check(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(answer, "subjects")) == 0, "the piece detail has no subjects after unlinking");
  cJSON_Delete(answer);

  /* Deleting an item takes it out of the subject's links. */
  cJSON_Delete(test_http_json(PORT, "DELETE", path, NULL, &status));
  char subject_path[300];
  snprintf(subject_path, sizeof(subject_path), "/api/subjects/%s", anna);
  answer = test_http_json(PORT, "GET", subject_path, NULL, &status);
  check(!array_has(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(answer, "subject"), "items"), item),
        "a deleted item leaves the subject's links");
  cJSON_Delete(answer);
}

int main(void) {
  /* Fresh state: this test's working directory is its own. */
#ifdef _WIN32
  system("rmdir /s /q pieces collections materials subjects data 2> nul");
#else
  system("rm -rf pieces collections materials subjects data");
#endif
  MKDIR("pieces");

  char clementi_path[512];
  snprintf(clementi_path, sizeof(clementi_path), "%s/Clementi 36.mid",
           URSATZ_FIXTURES_DIR "/framework-v1-corpus");

  struct mg_context *ctx = test_gui_server_start(PORT);
  check(ctx != NULL, "server starts");
  if (!ctx) {
    return 1;
  }
  test_sleep_ms(200);

  char *piece = test_http_upload(PORT, clementi_path, "Clementi%2036.mid", NULL);
  check(piece != NULL, "Clementi 36 uploads");
  char *anna = NULL;
  test_subject_crud(&anna);
  if (anna && piece) {
    test_relations(anna);
    test_links(anna, piece);
  }

  free(anna);
  free(piece);
  test_gui_server_stop(ctx);

  return test_finish("subjects_route");
}
