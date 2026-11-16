/* Styles end to end: pieces uploaded through the Analyzer route are tagged,
 * then POST /api/styles builds a StyleModel file from a tag, a collection or
 * a list of ids, and the list, read, rebuild and delete routes work on what
 * is stored under styles/<id>/. The model is read back with the Analyzer's
 * style_model_file_read. Uses the shared HTTP client (tests/support/test_http.h).
 * Runs in its own working directory (pieces/, collections/ and styles/ are created there). */

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

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/style_profile.h>

#include "gui_check.h"
#include "server.h"
#include "test_http.h"

#define PORT "18769"
#include "service/style_service.h"

#ifndef URSATZ_GUI_TEST_URSATZ_DIR
#error "URSATZ_GUI_TEST_URSATZ_DIR must name the library folder"
#endif

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

/* PATCHes a piece with a JSON body. */
static void patch_piece(const char *id, const char *body) {
  char path[256];
  int status = 0;
  snprintf(path, sizeof(path), "/api/analyzer/pieces/%s", id);
  cJSON_Delete(test_http_json(PORT, "PATCH", path, body, &status));
}

/* Builds a style with a JSON body; returns the parsed answer (caller deletes). */
static cJSON *build_style(const char *body, int *status) {
  return test_http_json(PORT, "POST", "/api/styles", body, status);
}

static const cJSON *style_of(const cJSON *answer) {
  return cJSON_GetObjectItemCaseSensitive(answer, "style");
}

static int skipped_count(const cJSON *answer) {
  const cJSON *skipped = cJSON_GetObjectItemCaseSensitive(answer, "skipped");
  return cJSON_IsArray(skipped) ? cJSON_GetArraySize(skipped) : -1;
}

/* How many styles GET /api/styles lists. */
static int listed_styles(void) {
  int status = 0;
  cJSON *list = test_http_json(PORT, "GET", "/api/styles", NULL, &status);
  const cJSON *styles = cJSON_GetObjectItemCaseSensitive(list, "styles");
  int count = cJSON_IsArray(styles) ? cJSON_GetArraySize(styles) : -1;
  cJSON_Delete(list);
  return count;
}

/* The Analyzer reads styles/<id>/style-model.json and it holds the piece count. */
static bool model_reads_with_pieces(const char *id, size_t expected_pieces) {
  char path[512];
  snprintf(path, sizeof(path), "styles/%s/style-model.json", id);
  StyleModel *model = NULL;
  size_t pieces = 0;
  if (style_model_file_read(path, &model, &pieces) != STYLE_MODEL_FILE_OK) {
    return false;
  }
  style_model_destroy(model);
  return pieces == expected_pieces;
}

int main(void) {
  /* Fresh state: this test's working directory is its own. */
#ifdef _WIN32
  system("rmdir /s /q pieces collections styles data 2> nul");
#else
  system("rm -rf pieces collections styles data");
#endif
  MKDIR("pieces");

  const char *fixtures = URSATZ_GUI_TEST_URSATZ_DIR "/tests/fixtures/framework-v1-corpus";
  char clementi_path[512];
  char mozart_path[512];
  snprintf(clementi_path, sizeof(clementi_path), "%s/Clementi 36.mid", fixtures);
  snprintf(mozart_path, sizeof(mozart_path), "%s/Mozart 545.mid", fixtures);

  struct mg_context *ctx = ursatz_gui_server_start(PORT, ".");
  check(ctx != NULL, "server starts");
  if (!ctx) {
    return 1;
  }
  test_sleep_ms(200);

  char *clementi_id = test_http_upload(PORT, clementi_path, "Clementi%2036.mid", NULL);
  char *mozart_id = test_http_upload(PORT, mozart_path, "Mozart%20545.mid", NULL);
  check(clementi_id && mozart_id, "Clementi 36 and Mozart 545 upload");
  if (!clementi_id || !mozart_id) {
    mg_stop(ctx);
    return 1;
  }
  patch_piece(clementi_id, "{\"add_tags\":[\"sonatina\"]}");
  patch_piece(mozart_id, "{\"add_tags\":[\"sonatina\"],\"composer\":\"Mozart\"}");

  int status = 0;

  /* An empty styles list before anything is built. */
  check(listed_styles() == 0, "no styles are listed before one is built");

  /* From a tag. */
  cJSON *answer = build_style("{\"filter\":{\"tag\":\"sonatina\"}}", &status);
  const cJSON *style = style_of(answer);
  check(status == 200 && ok_true(answer), "building a style from a tag answers 200");
  check(number_of(style, "piece_count") == 2, "a style from the tag holds both pieces");
  check(strcmp(string_of(style, "name"), "Tag: sonatina") == 0, "its default name is \"Tag: sonatina\"");
  check(strcmp(string_of(cJSON_GetObjectItemCaseSensitive(style, "source"), "kind"), "filter") == 0,
        "its source kind is filter");
  check(skipped_count(answer) == 0, "no piece was skipped");
  char tag_style_id[128];
  snprintf(tag_style_id, sizeof(tag_style_id), "%s", string_of(style, "id"));
  cJSON_Delete(answer);
  check(strncmp(tag_style_id, "sty-", 4) == 0, "the id is a sty- id");
  check(model_reads_with_pieces(tag_style_id, 2), "the Analyzer reads the model back with 2 pieces");
  {
    char path[512];
    snprintf(path, sizeof(path), "styles/%s/meta.json", tag_style_id);
    check(test_file_exists(path), "meta.json is stored next to the model");
  }
  check(listed_styles() == 1, "the list holds the new style");

  /* Reading one: the statistics are grouped by dimension and category. */
  {
    char path[256];
    snprintf(path, sizeof(path), "/api/styles/%s", tag_style_id);
    answer = test_http_json(PORT, "GET", path, NULL, &status);
    style = style_of(answer);
    check(status == 200 && strcmp(string_of(style, "id"), tag_style_id) == 0, "GET one style answers it");
    const cJSON *pieces = cJSON_GetObjectItemCaseSensitive(style, "pieces");
    check(cJSON_IsArray(pieces) && cJSON_GetArraySize(pieces) == 2, "it names the 2 pieces used");
    const cJSON *statistics = cJSON_GetObjectItemCaseSensitive(style, "statistics");
    const cJSON *harmony = cJSON_GetObjectItemCaseSensitive(statistics, "harmony");
    const cJSON *numerals = cJSON_GetObjectItemCaseSensitive(harmony, "chord-numeral");
    const cJSON *labels = cJSON_GetObjectItemCaseSensitive(numerals, "labels");
    check(cJSON_IsArray(labels) && cJSON_GetArraySize(labels) > 0 && cJSON_GetArraySize(labels) <= 12,
          "statistics hold up to 12 chord-numeral labels under harmony");
    const cJSON *first = cJSON_IsArray(labels) ? cJSON_GetArrayItem(labels, 0) : NULL;
    const cJSON *second = cJSON_IsArray(labels) ? cJSON_GetArrayItem(labels, 1) : NULL;
    check(first && number_of(first, "count") >= 1 && number_of(first, "total") >= number_of(first, "count"),
          "a label carries its count and its category total");
    check(!second || number_of(first, "count") >= number_of(second, "count"), "labels run by count, highest first");
    check(number_of(numerals, "more") >= 0, "a category reports how many labels were cut");
    cJSON_Delete(answer);
  }

  /* The model path the Composer will use. */
  {
    char path[1024];
    check(ursatz_gui_style_model_path(tag_style_id, path, sizeof(path)) == URSATZ_GUI_STYLE_OK,
          "model_path finds the model");
    check(path[0] == '/' || (path[0] && path[1] == ':'), "the model path is absolute");
    size_t length = strlen(path);
    check(length > 16 && strcmp(path + length - 16, "style-model.json") == 0, "the path ends with style-model.json");
    check(test_file_exists(path), "the path names an existing file");
    check(ursatz_gui_style_model_path("sty-0000000000000000", path, sizeof(path)) == URSATZ_GUI_STYLE_ERR_NOT_FOUND,
          "model_path of an unknown style is not found");
  }

  /* A name with a quote round-trips. */
  answer = build_style("{\"name\":\"  Light  \\\"sonatina\\\" \",\"filter\":{\"tag\":\"sonatina\"}}", &status);
  check(status == 200 && strcmp(string_of(style_of(answer), "name"), "Light \"sonatina\"") == 0,
        "a quote in a name round-trips and the name is normalized");
  {
    char quoted_id[128];
    snprintf(quoted_id, sizeof(quoted_id), "%s", string_of(style_of(answer), "id"));
    cJSON_Delete(answer);
    char path[256];
    snprintf(path, sizeof(path), "/api/styles/%s", quoted_id);
    cJSON_Delete(test_http_json(PORT, "DELETE", path, NULL, &status));
  }

  /* A name longer than 120 bytes is refused. */
  {
    char body[512];
    char name[160];
    memset(name, 'n', sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';
    snprintf(body, sizeof(body), "{\"name\":\"%s\",\"filter\":{\"tag\":\"sonatina\"}}", name);
    answer = build_style(body, &status);
    check(status == 400 && !ok_true(answer), "a name over 120 bytes answers 400");
    cJSON_Delete(answer);
  }

  /* From a collection. */
  answer = test_http_json(PORT, "POST", "/api/corpus", "{\"name\":\"Pair\"}", &status);
  char collection_id[128];
  snprintf(collection_id, sizeof(collection_id), "%s", string_of(answer, "id"));
  cJSON_Delete(answer);
  {
    char path[256];
    char body[512];
    snprintf(path, sizeof(path), "/api/corpus/%s/pieces", collection_id);
    snprintf(body, sizeof(body), "{\"piece_ids\":[\"%s\",\"%s\"]}", clementi_id, mozart_id);
    cJSON_Delete(test_http_json(PORT, "POST", path, body, &status));
    snprintf(body, sizeof(body), "{\"collection_id\":\"%s\"}", collection_id);
    answer = build_style(body, &status);
    check(status == 200 && number_of(style_of(answer), "piece_count") == 2, "a style from a collection holds its pieces");
    check(strcmp(string_of(style_of(answer), "name"), "Collection: Pair") == 0,
          "its default name is \"Collection: Pair\"");
    check(strcmp(string_of(cJSON_GetObjectItemCaseSensitive(style_of(answer), "source"), "kind"), "collection") == 0,
          "its source kind is collection");
    cJSON_Delete(answer);

    answer = build_style("{\"collection_id\":\"col-0000000000000000\"}", &status);
    check(status == 404 && !ok_true(answer), "an unknown collection answers 404");
    cJSON_Delete(answer);
  }

  /* From a brief: the union of a collection, a filter and ids, and of a style's pieces. */
  {
    answer = test_http_json(PORT, "POST", "/api/corpus", "{\"name\":\"Solo\"}", &status);
    char solo_id[128];
    snprintf(solo_id, sizeof(solo_id), "%s", string_of(answer, "id"));
    cJSON_Delete(answer);
    char path[256];
    char body[1024];
    snprintf(path, sizeof(path), "/api/corpus/%s/pieces", solo_id);
    snprintf(body, sizeof(body), "{\"piece_ids\":[\"%s\"]}", clementi_id);
    cJSON_Delete(test_http_json(PORT, "POST", path, body, &status));

    snprintf(body, sizeof(body),
             "{\"brief\":{\"scopes\":[{\"kind\":\"collection\",\"collection_id\":\"%s\"},"
             "{\"kind\":\"filter\",\"filter\":{\"composer\":\"Mozart\"}},"
             "{\"kind\":\"filter\",\"filter\":{\"composer\":\"Nobody\"}}],\"piece_ids\":[\"%s\"]}}",
             solo_id, clementi_id);
    answer = build_style(body, &status);
    style = style_of(answer);
    const cJSON *source = cJSON_GetObjectItemCaseSensitive(style, "source");
    const cJSON *brief = cJSON_GetObjectItemCaseSensitive(source, "brief");
    check(status == 200 && ok_true(answer), "building a style from a brief answers 200");
    check(number_of(style, "piece_count") == 2, "a brief of a collection, a filter and a piece holds the union once");
    check(strcmp(string_of(style, "name"), "Brief") == 0, "its default name is \"Brief\"");
    check(strcmp(string_of(source, "kind"), "brief") == 0, "its source kind is brief");
    check(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(brief, "scopes")) == 3 &&
              cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(brief, "piece_ids")) == 1 &&
              cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(brief, "style_ids")) == 0,
          "the list entry carries the brief");
    check(skipped_count(answer) == 0, "a scope that names no piece adds none and skips none");
    char brief_style_id[128];
    snprintf(brief_style_id, sizeof(brief_style_id), "%s", string_of(style, "id"));
    cJSON_Delete(answer);
    check(model_reads_with_pieces(brief_style_id, 2), "the Analyzer reads the brief's model back with 2 pieces");

    /* The pieces are in id order. */
    snprintf(path, sizeof(path), "/api/styles/%s", brief_style_id);
    answer = test_http_json(PORT, "GET", path, NULL, &status);
    {
      const cJSON *pieces = cJSON_GetObjectItemCaseSensitive(style_of(answer), "pieces");
      check(cJSON_GetArraySize(pieces) == 2 &&
                strcmp(cJSON_GetArrayItem(pieces, 0)->valuestring, cJSON_GetArrayItem(pieces, 1)->valuestring) < 0,
            "the brief's pieces run in id order");
    }
    cJSON_Delete(answer);

    /* A style id adds its pieces. */
    snprintf(body, sizeof(body), "{\"name\":\"Clementi only\",\"piece_ids\":[\"%s\"]}", clementi_id);
    answer = build_style(body, &status);
    char one_id[128];
    snprintf(one_id, sizeof(one_id), "%s", string_of(style_of(answer), "id"));
    cJSON_Delete(answer);
    snprintf(body, sizeof(body),
             "{\"name\":\"Mozart and a style\",\"brief\":{\"scopes\":[{\"kind\":\"filter\",\"filter\":{\"composer\":"
             "\"Mozart\"}}],\"style_ids\":[\"%s\"]}}",
             one_id);
    answer = build_style(body, &status);
    check(status == 200 && number_of(style_of(answer), "piece_count") == 2, "a style id adds the pieces of its last build");
    check(strcmp(string_of(style_of(answer), "name"), "Mozart and a style") == 0, "a brief takes the name it is given");
    char with_style_id[128];
    snprintf(with_style_id, sizeof(with_style_id), "%s", string_of(style_of(answer), "id"));
    cJSON_Delete(answer);

    /* Unknown sources are 404, a bad brief is 400, a brief of nothing is 422. */
    answer = build_style("{\"brief\":{\"scopes\":[{\"kind\":\"subject\",\"subject_id\":\"sub-0000000000000000\"}]}}",
                         &status);
    check(status == 404 && !ok_true(answer), "an unknown subject answers 404");
    cJSON_Delete(answer);
    answer = build_style("{\"brief\":{\"style_ids\":[\"sty-0000000000000000\"]}}", &status);
    check(status == 404 && !ok_true(answer), "an unknown style answers 404");
    cJSON_Delete(answer);
    answer = build_style("{\"brief\":{\"scopes\":[{\"kind\":\"collection\",\"collection_id\":\"col-0000000000000000\"}]}}",
                         &status);
    check(status == 404 && !ok_true(answer), "an unknown collection in a brief answers 404");
    cJSON_Delete(answer);
    answer = build_style("{\"brief\":{}}", &status);
    check(status == 400 && !ok_true(answer), "an empty brief answers 400");
    cJSON_Delete(answer);
    answer = build_style("{\"brief\":{\"scopes\":[{\"kind\":\"galaxy\"}]}}", &status);
    check(status == 400 && !ok_true(answer), "a scope of an unknown kind answers 400");
    cJSON_Delete(answer);
    snprintf(body, sizeof(body), "{\"brief\":{\"piece_ids\":[\"%s\"]},\"filter\":{\"tag\":\"sonatina\"}}", clementi_id);
    answer = build_style(body, &status);
    check(status == 400 && !ok_true(answer), "a brief beside another source answers 400");
    cJSON_Delete(answer);
    answer = build_style("{\"brief\":{\"scopes\":[{\"kind\":\"filter\",\"filter\":{\"composer\":\"Nobody\"}}]}}",
                         &status);
    check(status == 422 && !ok_true(answer), "a brief that names no piece answers 422");
    cJSON_Delete(answer);

    /* Rebuild resolves the brief again: the style it names changed. */
    snprintf(path, sizeof(path), "/api/styles/%s/rebuild", one_id);
    answer = test_http_json(PORT, "POST", path, NULL, &status);
    check(status == 200, "rebuilding the style a brief names answers 200");
    cJSON_Delete(answer);
    snprintf(path, sizeof(path), "/api/styles/%s/rebuild", with_style_id);
    answer = test_http_json(PORT, "POST", path, NULL, &status);
    check(status == 200 && number_of(style_of(answer), "piece_count") == 2 &&
              strcmp(string_of(cJSON_GetObjectItemCaseSensitive(style_of(answer), "source"), "kind"), "brief") == 0,
          "rebuilding a brief resolves it again and keeps its kind");
    cJSON_Delete(answer);
    check(model_reads_with_pieces(with_style_id, 2), "the rebuilt brief's model holds 2 pieces");

    /* A deleted style makes the brief's rebuild 422. */
    snprintf(path, sizeof(path), "/api/styles/%s", one_id);
    cJSON_Delete(test_http_json(PORT, "DELETE", path, NULL, &status));
    snprintf(path, sizeof(path), "/api/styles/%s/rebuild", with_style_id);
    answer = test_http_json(PORT, "POST", path, NULL, &status);
    check(status == 422 && !ok_true(answer), "rebuilding a brief whose style is gone answers 422");
    cJSON_Delete(answer);
    check(model_reads_with_pieces(with_style_id, 2), "a refused brief rebuild leaves the model as it was");

    snprintf(path, sizeof(path), "/api/styles/%s", with_style_id);
    cJSON_Delete(test_http_json(PORT, "DELETE", path, NULL, &status));
    snprintf(path, sizeof(path), "/api/styles/%s", brief_style_id);
    cJSON_Delete(test_http_json(PORT, "DELETE", path, NULL, &status));
  }

  /* From ids, one unknown. */
  {
    char body[512];
    snprintf(body, sizeof(body), "{\"piece_ids\":[\"%s\",\"missing-piece\"]}", mozart_id);
    answer = build_style(body, &status);
    check(status == 200 && number_of(style_of(answer), "piece_count") == 1, "a style from ids uses the known piece");
    check(strcmp(string_of(style_of(answer), "name"), "Selected pieces") == 0,
          "its default name is \"Selected pieces\"");
    const cJSON *skipped = cJSON_GetObjectItemCaseSensitive(answer, "skipped");
    const cJSON *entry = cJSON_IsArray(skipped) ? cJSON_GetArrayItem(skipped, 0) : NULL;
    check(skipped_count(answer) == 1 && strcmp(string_of(entry, "id"), "missing-piece") == 0 &&
              strcmp(string_of(entry, "reason"), "not found") == 0,
          "the unknown id is skipped as \"not found\"");
    cJSON_Delete(answer);
  }

  /* Nothing to analyze: 422, and nothing is written. */
  {
    int before = listed_styles();
    answer = build_style("{\"filter\":{\"tag\":\"no-such-tag\"}}", &status);
    check(status == 422 && !ok_true(answer) && strcmp(string_of(answer, "message"), "no analyzable pieces") == 0,
          "a tag no piece has answers 422 \"no analyzable pieces\"");
    cJSON_Delete(answer);
    check(listed_styles() == before, "a refused build writes no style");
  }

  /* The request must name exactly one source. */
  answer = build_style("{\"filter\":{\"tag\":\"sonatina\"},\"collection_id\":\"x\"}", &status);
  check(status == 400 && !ok_true(answer), "two sources answer 400");
  cJSON_Delete(answer);
  answer = build_style("{\"name\":\"nothing\"}", &status);
  check(status == 400 && !ok_true(answer), "no source answers 400");
  cJSON_Delete(answer);
  answer = build_style("{\"filter\":{}}", &status);
  check(status == 400 && !ok_true(answer), "a filter with no value answers 400");
  cJSON_Delete(answer);
  answer = build_style("not json", &status);
  check(status == 400 && !ok_true(answer), "a body that is not JSON answers 400");
  cJSON_Delete(answer);

  /* A piece analyzed under an older schema is analyzed again and used. */
  {
    char meta_path[512];
    char analysis_path[512];
    snprintf(meta_path, sizeof(meta_path), "pieces/%s/meta.json", mozart_id);
    snprintf(analysis_path, sizeof(analysis_path), "pieces/%s/analysis.json", mozart_id);
    char *meta_text = test_read_file(meta_path, NULL);
    cJSON *meta = meta_text ? cJSON_Parse(meta_text) : NULL;
    free(meta_text);
    bool seeded = false;
    if (meta) {
      cJSON_ReplaceItemInObjectCaseSensitive(meta, "document_schema_version", cJSON_CreateNumber(1));
      char *text = cJSON_Print(meta);
      seeded = text && test_write_file(meta_path, text, strlen(text)) && test_write_file(analysis_path, "{\"schema_version\":1}", strlen("{\"schema_version\":1}"));
      free(text);
    }
    cJSON_Delete(meta);
    check(seeded, "a piece is set back to schema version 1 with an unusable analysis");

    char body[256];
    snprintf(body, sizeof(body), "{\"piece_ids\":[\"%s\"]}", mozart_id);
    answer = build_style(body, &status);
    check(status == 200 && number_of(style_of(answer), "piece_count") == 1 && skipped_count(answer) == 0,
          "the old piece is re-analyzed and used");
    cJSON_Delete(answer);

    meta_text = test_read_file(meta_path, NULL);
    meta = meta_text ? cJSON_Parse(meta_text) : NULL;
    free(meta_text);
    check(number_of(meta, "document_schema_version") == URSATZ_ANALYZER_DOCUMENT_SCHEMA_VERSION,
          "its meta.json records the current schema version");
    cJSON_Delete(meta);
  }

  /* Rebuild after untagging one piece. */
  {
    patch_piece(clementi_id, "{\"remove_tags\":[\"sonatina\"]}");
    char path[256];
    snprintf(path, sizeof(path), "/api/styles/%s/rebuild", tag_style_id);
    char old_updated[64];
    {
      char get_path[256];
      snprintf(get_path, sizeof(get_path), "/api/styles/%s", tag_style_id);
      answer = test_http_json(PORT, "GET", get_path, NULL, &status);
      snprintf(old_updated, sizeof(old_updated), "%s", string_of(style_of(answer), "updated_at"));
      cJSON_Delete(answer);
    }
    answer = test_http_json(PORT, "POST", path, NULL, &status);
    check(status == 200 && number_of(style_of(answer), "piece_count") == 1, "rebuilding after untagging one gives 1 piece");
    check(strcmp(string_of(style_of(answer), "id"), tag_style_id) == 0, "the rebuilt style keeps its id");
    check(strcmp(string_of(style_of(answer), "name"), "Tag: sonatina") == 0, "the rebuilt style keeps its name");
    check(strcmp(string_of(style_of(answer), "updated_at"), old_updated) >= 0, "updated_at does not go back");
    cJSON_Delete(answer);
    check(model_reads_with_pieces(tag_style_id, 1), "the model file was replaced in place with 1 piece");

    /* No piece left: 422, and the style stays as it was. */
    patch_piece(mozart_id, "{\"remove_tags\":[\"sonatina\"]}");
    answer = test_http_json(PORT, "POST", path, NULL, &status);
    check(status == 422 && !ok_true(answer), "rebuilding with no piece left answers 422");
    cJSON_Delete(answer);
    check(model_reads_with_pieces(tag_style_id, 1), "a refused rebuild leaves the model as it was");
  }

  /* Delete, then 404. */
  {
    char path[256];
    snprintf(path, sizeof(path), "/api/styles/%s", tag_style_id);
    int before = listed_styles();
    cJSON *deleted = test_http_json(PORT, "DELETE", path, NULL, &status);
    check(status == 200 && ok_true(deleted), "DELETE a style answers ok");
    cJSON_Delete(deleted);
    check(listed_styles() == before - 1, "the list no longer holds it");
    char model[512];
    snprintf(model, sizeof(model), "styles/%s/style-model.json", tag_style_id);
    check(!test_file_exists(model), "its files are gone");
    answer = test_http_json(PORT, "GET", path, NULL, &status);
    check(status == 404 && !ok_true(answer), "GET a deleted style answers 404");
    cJSON_Delete(answer);
    answer = test_http_json(PORT, "DELETE", path, NULL, &status);
    check(status == 404, "DELETE a deleted style answers 404");
    cJSON_Delete(answer);
    char rebuild[256];
    snprintf(rebuild, sizeof(rebuild), "/api/styles/%s/rebuild", tag_style_id);
    answer = test_http_json(PORT, "POST", rebuild, NULL, &status);
    check(status == 404, "rebuilding a deleted style answers 404");
    cJSON_Delete(answer);
    answer = test_http_json(PORT, "GET", "/api/styles/sty-0000000000000000", NULL, &status);
    check(status == 404, "an unknown style id answers 404");
    cJSON_Delete(answer);
  }

  free(clementi_id);
  free(mozart_id);
  mg_stop(ctx);

  return test_finish("styles_route");
}
