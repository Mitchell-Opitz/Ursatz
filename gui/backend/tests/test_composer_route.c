/* The Composer routes write versions: grow and plan make version 1 of a
 * composition by running the real Composer command line, and regenerate, lock,
 * variations and choose each add a version without changing the earlier ones.
 * A fake Composer (URSATZ_GUI_COMPOSER) records the arguments of every run, so
 * a request that must not start the Composer can be shown not to, and one that
 * sleeps past URSATZ_GUI_COMPOSER_TIMEOUT gives 504. Uses a plain TCP socket
 * for the HTTP requests, same as test_analyzer_route.c. */

#include <civetweb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <cJSON.h>

#include "server.h"

#ifndef URSATZ_GUI_TEST_COMPOSER_DIR
#error "URSATZ_GUI_TEST_COMPOSER_DIR must name the external/ursatz-composer checkout"
#endif

static int failures = 0;

static void check(bool cond, const char *what) {
  if (!cond) {
    fprintf(stderr, "FAIL: %s\n", what);
    failures++;
  } else {
    printf("PASS: %s\n", what);
  }
}

static void sleep_ms(int ms) {
  struct timespec ts = {0, (long)ms * 1000 * 1000};
  nanosleep(&ts, NULL);
}

/* One HTTP exchange. Returns the whole response (status line, headers, body)
 * as a newly allocated string, or NULL when the connection failed. */
static char *http_exchange_with(const char *method, const char *path, const char *extra_headers, const char *body,
                                size_t body_len) {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    return NULL;
  }

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(18768);
  inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
  if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
    close(sock);
    return NULL;
  }

  char header[1024];
  int header_len = snprintf(header, sizeof(header),
                            "%s %s HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Type: application/json\r\n%s"
                            "Content-Length: %zu\r\nConnection: close\r\n\r\n",
                            method, path, extra_headers ? extra_headers : "", body_len);
  send(sock, header, (size_t)header_len, 0);
  if (body_len > 0) {
    send(sock, body, body_len, 0);
  }

  size_t capacity = 65536;
  size_t total = 0;
  char *response = malloc(capacity);
  while (response) {
    if (total + 1 >= capacity) {
      capacity *= 2;
      char *grown = realloc(response, capacity);
      if (!grown) {
        free(response);
        response = NULL;
        break;
      }
      response = grown;
    }
    ssize_t n = recv(sock, response + total, capacity - 1 - total, 0);
    if (n <= 0) {
      break;
    }
    total += (size_t)n;
  }
  close(sock);
  if (response) {
    response[total] = '\0';
  }
  return response;
}

static char *http_exchange(const char *method, const char *path, const char *body) {
  return http_exchange_with(method, path, NULL, body, body ? strlen(body) : 0);
}

static const char *response_body(const char *response) {
  const char *split = response ? strstr(response, "\r\n\r\n") : NULL;
  return split ? split + 4 : "";
}

static int response_status(const char *response) {
  int status = 0;
  if (response && sscanf(response, "HTTP/1.1 %d", &status) == 1) {
    return status;
  }
  return 0;
}

/* The body parsed as JSON; the status goes to *status. Caller frees the JSON. */
static cJSON *exchange_json(const char *method, const char *path, const char *body, int *status) {
  char *response = http_exchange(method, path, body);
  *status = response_status(response);
  cJSON *json = cJSON_Parse(response_body(response));
  free(response);
  return json;
}

static char *read_file(const char *path, size_t *out_size) {
  FILE *file = fopen(path, "rb");
  if (!file) {
    return NULL;
  }
  size_t capacity = 4096;
  size_t length = 0;
  char *buffer = malloc(capacity);
  while (buffer) {
    size_t n = fread(buffer + length, 1, capacity - length - 1, file);
    length += n;
    if (n == 0) {
      break;
    }
    if (length + 1 >= capacity) {
      capacity *= 2;
      char *grown = realloc(buffer, capacity);
      if (!grown) {
        free(buffer);
        return NULL;
      }
      buffer = grown;
    }
  }
  fclose(file);
  if (!buffer) {
    return NULL;
  }
  buffer[length] = '\0';
  if (out_size) {
    *out_size = length;
  }
  return buffer;
}

static bool file_exists(const char *path) {
  struct stat st;
  return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static bool dir_exists(const char *path) {
  struct stat st;
  return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static bool write_text(const char *path, const char *text, mode_t mode) {
  FILE *file = fopen(path, "wb");
  if (!file) {
    return false;
  }
  bool ok = fwrite(text, 1, strlen(text), file) == strlen(text);
  ok = (fclose(file) == 0) && ok;
  return ok && chmod(path, mode) == 0;
}

/* True when two files have the same bytes. */
static bool same_bytes(const char *left, const char *right) {
  size_t left_size = 0, right_size = 0;
  char *a = read_file(left, &left_size);
  char *b = read_file(right, &right_size);
  bool same = a && b && left_size == right_size && memcmp(a, b, left_size) == 0;
  free(a);
  free(b);
  return same;
}

static bool version_files_exist(const char *id, int version) {
  char path[512];
  static const char *const names[] = {"plan.json", "piece.mid", "piece.musicxml"};
  for (size_t i = 0; i < 3; i++) {
    snprintf(path, sizeof(path), "compositions/%s/v%d/%s", id, version, names[i]);
    if (!file_exists(path)) {
      return false;
    }
  }
  return true;
}

static int version_count(const char *id) {
  char path[256];
  snprintf(path, sizeof(path), "/api/composer/compositions/%s", id);
  int status = 0;
  cJSON *detail = exchange_json("GET", path, NULL, &status);
  const cJSON *versions = detail ? cJSON_GetObjectItemCaseSensitive(detail, "versions") : NULL;
  int count = cJSON_IsArray(versions) ? cJSON_GetArraySize(versions) : -1;
  cJSON_Delete(detail);
  return count;
}

static int composition_count(void) {
  int status = 0;
  cJSON *list = exchange_json("GET", "/api/composer/compositions", NULL, &status);
  const cJSON *items = list ? cJSON_GetObjectItemCaseSensitive(list, "compositions") : NULL;
  int count = cJSON_IsArray(items) ? cJSON_GetArraySize(items) : -1;
  cJSON_Delete(list);
  return count;
}

/* The id string of a response, copied into out. */
static bool take_id(const cJSON *json, char *out, size_t size) {
  const cJSON *id = json ? cJSON_GetObjectItemCaseSensitive(json, "id") : NULL;
  if (!cJSON_IsString(id) || !id->valuestring || strlen(id->valuestring) >= size) {
    return false;
  }
  snprintf(out, size, "%s", id->valuestring);
  return true;
}

static double number_at(const cJSON *json, const char *key) {
  const cJSON *item = json ? cJSON_GetObjectItemCaseSensitive(json, key) : NULL;
  return cJSON_IsNumber(item) ? item->valuedouble : -1;
}

/* A request to the composition `id`, with the body built from a format. */
static cJSON *post_to(const char *id, const char *what, const char *body, int *status) {
  char path[256];
  snprintf(path, sizeof(path), "/api/composer/compositions/%s/%s", id, what);
  return exchange_json("POST", path, body, status);
}

/* Uploads a file to the Library; returns the new piece's id (caller frees), or NULL. */
static char *upload_piece(const char *file_path, const char *encoded_filename) {
  size_t size = 0;
  char *data = read_file(file_path, &size);
  if (!data) {
    return NULL;
  }
  char headers[512];
  snprintf(headers, sizeof(headers), "X-Filename: %s\r\n", encoded_filename);
  char *response = http_exchange_with("POST", "/api/analyzer/pieces", headers, data, size);
  free(data);
  char *id = NULL;
  cJSON *json = cJSON_Parse(response_body(response));
  const cJSON *id_item = json ? cJSON_GetObjectItemCaseSensitive(json, "id") : NULL;
  if (cJSON_IsString(id_item)) {
    id = strdup(id_item->valuestring);
  }
  cJSON_Delete(json);
  free(response);
  return id;
}

int main(void) {
  system("rm -rf compositions fake pieces styles collections");
  mkdir("fake", 0755);

  char cwd[512];
  if (!getcwd(cwd, sizeof(cwd))) {
    return 1;
  }
  char fake_log[600];
  char fake_record[600];
  char fake_sleep[600];
  snprintf(fake_log, sizeof(fake_log), "%s/fake/argv.log", cwd);
  snprintf(fake_record, sizeof(fake_record), "%s/fake/record.sh", cwd);
  snprintf(fake_sleep, sizeof(fake_sleep), "%s/fake/sleep.sh", cwd);
  char script[1024];
  snprintf(script, sizeof(script), "#!/bin/sh\necho \"$@\" >> \"%s\"\nexit 0\n", fake_log);
  check(write_text(fake_record, script, 0755), "the recording fake Composer is written");
  check(write_text(fake_sleep, "#!/bin/sh\nexec sleep 5\n", 0755), "the sleeping fake Composer is written");

  struct mg_context *ctx = ursatz_gui_server_start("18768", ".");
  check(ctx != NULL, "server starts");
  if (!ctx) {
    return 1;
  }
  sleep_ms(200);

  int status = 0;
  cJSON *json = NULL;

  /* status. */
  json = exchange_json("GET", "/api/composer/status", NULL, &status);
  const cJSON *version_text = json ? cJSON_GetObjectItemCaseSensitive(json, "version") : NULL;
  check(status == 200 && cJSON_IsString(version_text) && strncmp(version_text->valuestring, "ursatz-composer", 15) == 0,
        "status reports the Composer version");
  cJSON_Delete(json);

  /* grow: motive_d_minor, sentence, D minor 4/4, seed 7. */
  size_t motive_size = 0;
  char *motive = read_file(URSATZ_GUI_TEST_COMPOSER_DIR "/tests/fixtures/motive_d_minor.txt", &motive_size);
  check(motive != NULL, "the motive fixture is read");
  if (!motive) {
    ursatz_gui_server_stop(ctx);
    return 1;
  }
  while (motive_size > 0 && (motive[motive_size - 1] == '\n' || motive[motive_size - 1] == '\r')) {
    motive[--motive_size] = '\0';
  }
  char grow_body[2048];
  snprintf(grow_body, sizeof(grow_body),
           "{\"name\":\"Theme\",\"motive\":\"%s\",\"into\":\"sentence\",\"key\":\"D minor\",\"meter\":\"4/4\","
           "\"seed\":7}",
           motive);
  free(motive);
  json = exchange_json("POST", "/api/composer/grow", grow_body, &status);
  char theme_id[64] = "";
  check(status == 200 && take_id(json, theme_id, sizeof(theme_id)), "grow answers 200 with an id");
  check(number_at(json, "version") == 1, "grow makes version 1");
  const cJSON *summary = json ? cJSON_GetObjectItemCaseSensitive(json, "summary") : NULL;
  const cJSON *themes = summary ? cJSON_GetObjectItemCaseSensitive(summary, "themes") : NULL;
  check(number_at(cJSON_GetArrayItem(themes, 0), "seed") == 7, "the grow summary has seed 7");
  cJSON_Delete(json);
  check(version_files_exist(theme_id, 1), "v1 has plan.json, piece.mid and piece.musicxml");

  json = exchange_json("GET", "/api/composer/compositions", NULL, &status);
  char *listed = json ? cJSON_PrintUnformatted(json) : NULL;
  check(status == 200 && listed && strstr(listed, "\"name\":\"Theme\"") != NULL,
        "the list shows the new composition");
  free(listed);
  cJSON_Delete(json);

  char path[512];
  snprintf(path, sizeof(path), "/api/composer/compositions/%s", theme_id);
  json = exchange_json("GET", path, NULL, &status);
  const cJSON *versions = json ? cJSON_GetObjectItemCaseSensitive(json, "versions") : NULL;
  const cJSON *first = cJSON_GetArrayItem(versions, 0);
  check(status == 200 && number_at(first, "seed") == 7 && number_at(first, "n") == 1, "the detail lists v1 with seed 7");
  const cJSON *action = first ? cJSON_GetObjectItemCaseSensitive(first, "action") : NULL;
  check(cJSON_IsString(action) && strcmp(action->valuestring, "grow") == 0, "v1 records the grow action");
  cJSON_Delete(json);


  /* Composing in a style built from a tagged reference piece. */
  {
    char piece_path[600];
    snprintf(piece_path, sizeof(piece_path),
             URSATZ_GUI_TEST_URSATZ_DIR "/tests/fixtures/framework-v1-corpus/Clementi 36.mid");
    char *piece_id = upload_piece(piece_path, "Clementi%2036.mid");
    check(piece_id != NULL, "the reference piece uploads");
    if (piece_id) {
      char patch_path[256];
      snprintf(patch_path, sizeof(patch_path), "/api/analyzer/pieces/%s", piece_id);
      cJSON_Delete(exchange_json("PATCH", patch_path, "{\"add_tags\":[\"sonatina\"]}", &status));
      free(piece_id);
    }
    json = exchange_json("POST", "/api/styles", "{\"filter\":{\"tag\":\"sonatina\"}}", &status);
    const cJSON *built = json ? cJSON_GetObjectItemCaseSensitive(json, "style") : NULL;
    const cJSON *built_id = built ? cJSON_GetObjectItemCaseSensitive(built, "id") : NULL;
    char style_id[96] = "";
    if (status == 200 && cJSON_IsString(built_id)) {
      snprintf(style_id, sizeof(style_id), "%s", built_id->valuestring);
    }
    check(style_id[0] != '\0', "a style is built from the tag");
    cJSON_Delete(json);

    char styled_body[2048];
    snprintf(styled_body, sizeof(styled_body),
             "{\"name\":\"Styled\",\"motive\":\"D4:q F4:e A4:e D5:h\",\"into\":\"sentence\",\"key\":\"D minor\","
             "\"meter\":\"4/4\",\"seed\":7,\"style\":\"%s\",\"style_weight\":\"1/4\"}",
             style_id);
    json = exchange_json("POST", "/api/composer/grow", styled_body, &status);
    char styled_id[64] = "";
    check(status == 200 && take_id(json, styled_id, sizeof(styled_id)), "grow with a style answers 200");
    cJSON_Delete(json);
    /* grow composes in the style; only plan records it as a constraint in plan.json. */
    snprintf(styled_body, sizeof(styled_body),
             "{\"name\":\"Styled piece\",\"form\":\"binary\",\"key\":\"C\",\"meter\":\"4/4\",\"seed\":3,"
             "\"style\":\"%s\",\"style_weight\":\"1/4\"}",
             style_id);
    json = exchange_json("POST", "/api/composer/plan", styled_body, &status);
    char planned_id[64] = "";
    check(status == 200 && take_id(json, planned_id, sizeof(planned_id)), "plan with a style answers 200");
    cJSON_Delete(json);
    char styled_plan[600];
    snprintf(styled_plan, sizeof(styled_plan), "compositions/%s/v1/plan.json", planned_id);
    char *plan_text = read_file(styled_plan, NULL);
    check(plan_text && strstr(plan_text, "style-model.json") != NULL,
          "the plan has a style constraint naming the style file");
    free(plan_text);

    json = exchange_json("POST", "/api/composer/grow",
                         "{\"name\":\"x\",\"motive\":\"D4:q F4:e\",\"into\":\"sentence\",\"key\":\"D minor\","
                         "\"meter\":\"4/4\",\"style\":\"sty-0000000000000000\"}",
                         &status);
    const cJSON *style_error = json ? cJSON_GetObjectItemCaseSensitive(json, "error") : NULL;
    check(status == 400 && cJSON_IsString(style_error) && strcmp(style_error->valuestring, "unknown style") == 0,
          "an unknown style gives 400 unknown style");
    cJSON_Delete(json);
    json = exchange_json("POST", "/api/composer/grow",
                         "{\"name\":\"x\",\"motive\":\"D4:q F4:e\",\"into\":\"sentence\",\"key\":\"D minor\","
                         "\"meter\":\"4/4\",\"style_weight\":\"1/2\"}",
                         &status);
    check(status == 400, "a style weight without a style gives 400");
    cJSON_Delete(json);
    json = exchange_json("POST", "/api/composer/plan",
                         "{\"name\":\"x\",\"form\":\"binary\",\"key\":\"C\",\"meter\":\"4/4\",\"style_weight\":\"1/2\"}",
                         &status);
    check(status == 400, "plan with a style weight and no style gives 400");
    cJSON_Delete(json);
    snprintf(styled_body, sizeof(styled_body),
             "{\"name\":\"x\",\"form\":\"binary\",\"key\":\"C\",\"meter\":\"4/4\",\"style\":\"%s\","
             "\"style_weight\":\"1/0\"}",
             style_id);
    json = exchange_json("POST", "/api/composer/plan", styled_body, &status);
    check(status == 400, "a style weight with a zero denominator gives 400");
    cJSON_Delete(json);
  }

  /* regenerate the theme's section (node 2 of a sentence plan) with seed 5. */
  char v1_before[600];
  char v1_copy[600];
  snprintf(v1_before, sizeof(v1_before), "compositions/%s/v1/plan.json", theme_id);
  snprintf(v1_copy, sizeof(v1_copy), "fake/v1-plan-before.json");
  {
    size_t size = 0;
    char *bytes = read_file(v1_before, &size);
    FILE *out = bytes ? fopen(v1_copy, "wb") : NULL;
    if (out) {
      fwrite(bytes, 1, size, out);
      fclose(out);
    }
    free(bytes);
  }
  json = post_to(theme_id, "regenerate", "{\"version\":1,\"node\":2,\"seed\":5}", &status);
  check(status == 200 && number_at(json, "version") == 2, "regenerate makes version 2");
  check(number_at(cJSON_GetObjectItemCaseSensitive(json, "summary"), "seed") == 5, "the regenerate summary has seed 5");
  cJSON_Delete(json);
  check(version_files_exist(theme_id, 2), "v2 has all three files");
  check(same_bytes(v1_before, v1_copy), "regenerate leaves v1 unchanged");

  /* lock makes v3 and copies the piece files of v2. */
  json = post_to(theme_id, "lock", "{\"version\":2,\"node\":2}", &status);
  check(status == 200 && number_at(json, "version") == 3, "lock makes version 3");
  cJSON_Delete(json);
  check(version_files_exist(theme_id, 3), "v3 has all three files");
  char v2_mid[600];
  char v3_mid[600];
  snprintf(v2_mid, sizeof(v2_mid), "compositions/%s/v2/piece.mid", theme_id);
  snprintf(v3_mid, sizeof(v3_mid), "compositions/%s/v3/piece.mid", theme_id);
  check(same_bytes(v2_mid, v3_mid), "a locked version copies the piece files of the one it locks");
  check(version_count(theme_id) == 3, "meta.json lists three versions");

  /* A command that cannot proceed (the section is locked) adds no version. */
  json = post_to(theme_id, "regenerate", "{\"version\":3,\"node\":2,\"seed\":1}", &status);
  const cJSON *error = json ? cJSON_GetObjectItemCaseSensitive(json, "error") : NULL;
  check(status == 422 && cJSON_IsString(error) && strcmp(error->valuestring, "stuck") == 0 &&
            cJSON_GetObjectItemCaseSensitive(json, "stderr") != NULL,
        "regenerating a locked section gives 422 stuck with the Composer's stderr");
  cJSON_Delete(json);
  char v4_dir[600];
  snprintf(v4_dir, sizeof(v4_dir), "compositions/%s/v4", theme_id);
  check(!dir_exists(v4_dir) && version_count(theme_id) == 3, "a failed command leaves no version");

  /* variations of the open v1, then choose. */
  json = post_to(theme_id, "variations", "{\"version\":1,\"node\":2,\"lane\":\"harmony\",\"count\":2,\"seed\":1}", &status);
  const cJSON *list = json ? cJSON_GetObjectItemCaseSensitive(json, "variations") : NULL;
  check(status == 200 && cJSON_IsArray(list) && cJSON_GetArraySize(list) >= 1, "variations lists at least one variation");
  double set = number_at(json, "set");
  check(set >= 1 && number_at(json, "kept") == cJSON_GetArraySize(list), "the set number and kept count are returned");
  int kept = cJSON_GetArraySize(list);
  cJSON_Delete(json);
  bool files_ok = true;
  for (int i = 1; i <= kept; i++) {
    static const char *const extensions[] = {"json", "mid", "musicxml"};
    for (size_t e = 0; e < 3; e++) {
      char file[600];
      snprintf(file, sizeof(file), "compositions/%s/v1/variations-%d/variation-%d.%s", theme_id, (int)set, i,
               extensions[e]);
      files_ok = files_ok && file_exists(file);
    }
  }
  check(files_ok, "every kept variation has its plan, MIDI and MusicXML");

  snprintf(path, sizeof(path), "/api/composer/compositions/%s/variations/%d/1/musicxml", theme_id, (int)set);
  char *xml = http_exchange("GET", path, NULL);
  check(response_status(xml) == 200 && strstr(response_body(xml), "<score-partwise") != NULL,
        "a variation's MusicXML is served");
  free(xml);

  snprintf(path, sizeof(path), "/api/composer/compositions/%s/variations/%d/1/choose", theme_id, (int)set);
  json = exchange_json("POST", path, NULL, &status);
  check(status == 200 && number_at(json, "version") == 4, "choose makes the variation version 4");
  cJSON_Delete(json);
  check(version_files_exist(theme_id, 4), "v4 has all three files");
  snprintf(path, sizeof(path), "compositions/%s/v1/variations-%d/variation-1.json", theme_id, (int)set);
  char v4_plan[600];
  snprintf(v4_plan, sizeof(v4_plan), "compositions/%s/v4/plan.json", theme_id);
  check(same_bytes(path, v4_plan), "version 4 is the chosen variation's plan");
  snprintf(path, sizeof(path), "/api/composer/compositions/%s/variations/%d/%d/choose", theme_id, (int)set, kept + 1);
  json = exchange_json("POST", path, NULL, &status);
  check(status == 404, "choosing a variation that was not kept gives 404");
  cJSON_Delete(json);

  /* The downloads. */
  snprintf(path, sizeof(path), "/api/composer/compositions/%s/versions/1/midi", theme_id);
  char *midi = http_exchange("GET", path, NULL);
  check(response_status(midi) == 200 && strstr(midi, "Content-Type: audio/midi") != NULL &&
            strstr(midi, "attachment") != NULL && strncmp(response_body(midi), "MThd", 4) == 0,
        "the MIDI download is audio/midi, an attachment, and a MIDI file");
  free(midi);
  snprintf(path, sizeof(path), "/api/composer/compositions/%s/versions/1/plan", theme_id);
  json = exchange_json("GET", path, NULL, &status);
  check(status == 200 && cJSON_IsObject(json), "the plan of a version is JSON");
  cJSON_Delete(json);
  snprintf(path, sizeof(path), "/api/composer/compositions/%s/versions/9/plan", theme_id);
  json = exchange_json("GET", path, NULL, &status);
  check(status == 404, "an unknown version gives 404");
  cJSON_Delete(json);

  /* plan: rounded binary, C major 4/4, seed 3. */
  json = exchange_json("POST", "/api/composer/plan",
                       "{\"name\":\"Piece\",\"form\":\"rounded-binary\",\"key\":\"C\",\"meter\":\"4/4\",\"seed\":3}",
                       &status);
  char piece_id[64] = "";
  check(status == 200 && take_id(json, piece_id, sizeof(piece_id)), "plan answers 200 with an id");
  summary = json ? cJSON_GetObjectItemCaseSensitive(json, "summary") : NULL;
  check(number_at(cJSON_GetObjectItemCaseSensitive(summary, "fill"), "seed") == 3 &&
            number_at(cJSON_GetObjectItemCaseSensitive(summary, "plan"), "seed") == 3,
        "the summary holds the plan and the fill, both with seed 3");
  cJSON_Delete(json);
  check(version_files_exist(piece_id, 1), "the planned piece is filled into v1");
  snprintf(path, sizeof(path), "/api/composer/compositions/%s", piece_id);
  json = exchange_json("GET", path, NULL, &status);
  versions = json ? cJSON_GetObjectItemCaseSensitive(json, "versions") : NULL;
  check(number_at(cJSON_GetArrayItem(versions, 0), "seed") == 3, "the plan version's seed is the fill's");
  cJSON_Delete(json);

  /* The harmonize rule: a section without a given melody is refused. */
  json = post_to(piece_id, "variations", "{\"version\":1,\"section\":\"B\",\"count\":2,\"seed\":3,\"harmonize\":true}",
                 &status);
  check(status == 422, "harmonize on a section without a given melody is refused");
  cJSON_Delete(json);
  json = post_to(piece_id, "variations", "{\"version\":1,\"section\":\"B\",\"count\":2,\"seed\":3}", &status);
  check(status == 200 && number_at(json, "kept") >= 1, "variations of a section of a planned piece are kept");
  cJSON_Delete(json);

  /* rename and delete. */
  snprintf(path, sizeof(path), "/api/composer/compositions/%s", piece_id);
  json = exchange_json("PATCH", path, "{\"name\":\"Renamed\"}", &status);
  check(status == 200, "PATCH renames a composition");
  cJSON_Delete(json);
  json = exchange_json("GET", path, NULL, &status);
  const cJSON *name = json ? cJSON_GetObjectItemCaseSensitive(json, "name") : NULL;
  check(cJSON_IsString(name) && strcmp(name->valuestring, "Renamed") == 0 && version_count(piece_id) == 1,
        "the new name is kept with the versions");
  cJSON_Delete(json);
  json = exchange_json("PATCH", path, "{\"name\":\"\"}", &status);
  check(status == 400, "an empty name gives 400");
  cJSON_Delete(json);
  json = exchange_json("DELETE", path, NULL, &status);
  check(status == 200, "DELETE removes a composition");
  cJSON_Delete(json);
  snprintf(path, sizeof(path), "compositions/%s", piece_id);
  check(!dir_exists(path), "deleting a composition removes its directory");
  snprintf(path, sizeof(path), "/api/composer/compositions/%s", piece_id);
  json = exchange_json("GET", path, NULL, &status);
  check(status == 404, "a deleted composition is not found");
  cJSON_Delete(json);

  /* Bad requests answer 400 or 404 and start nothing: the fake records every run. */
  setenv("URSATZ_GUI_COMPOSER", fake_record, 1);
  remove(fake_log);
  int compositions_before = composition_count();
  json = exchange_json("POST", "/api/composer/grow",
                       "{\"name\":\"x\",\"motive\":\"D4:q F4:e\",\"into\":\"sentence\",\"key\":\"H minor\","
                       "\"meter\":\"4/4\"}",
                       &status);
  check(status == 400, "a bad key gives 400");
  cJSON_Delete(json);
  json = exchange_json("POST", "/api/composer/grow",
                       "{\"name\":\"x\",\"motive\":\"D4:q F4:e\",\"into\":\"sentence\",\"key\":\"D minor\","
                       "\"meter\":\"4/4\",\"seed\":-1}",
                       &status);
  check(status == 400, "a negative seed gives 400");
  cJSON_Delete(json);
  json = exchange_json("POST", "/api/composer/grow",
                       "{\"name\":\"x\",\"motive\":\"D4:q; rm -rf /\",\"into\":\"sentence\",\"key\":\"D minor\","
                       "\"meter\":\"4/4\"}",
                       &status);
  check(status == 400, "a motive with shell characters gives 400");
  cJSON_Delete(json);
  json = exchange_json("POST", "/api/composer/plan",
                       "{\"name\":\"x\",\"form\":\"binary\",\"key\":\"C\",\"meter\":\"4/4\",\"tempo\":900}", &status);
  check(status == 400, "a tempo out of range gives 400");
  cJSON_Delete(json);
  json = post_to(theme_id, "regenerate", "{\"version\":1,\"section\":\"A B\"}", &status);
  check(status == 400, "a section letter with a space gives 400");
  cJSON_Delete(json);
  json = post_to(theme_id, "regenerate", "{\"version\":1,\"section\":\"A\",\"node\":2}", &status);
  check(status == 400, "a section and a node together give 400");
  cJSON_Delete(json);
  json = post_to(theme_id, "variations", "{\"version\":1,\"section\":\"A\",\"count\":17}", &status);
  check(status == 400, "a count of 17 gives 400");
  cJSON_Delete(json);
  json = post_to(theme_id, "lock", "{\"version\":1,\"section\":\"A\",\"lane\":\"harmony; ls\"}", &status);
  check(status == 400, "a lane that is not a lane gives 400");
  cJSON_Delete(json);
  json = post_to("comp-0000000000000000", "lock", "{\"version\":1,\"section\":\"A\"}", &status);
  check(status == 404, "an unknown composition gives 404");
  cJSON_Delete(json);
  json = post_to("..", "lock", "{\"version\":1,\"section\":\"A\"}", &status);
  check(status == 404, "an id that is not an id gives 404");
  cJSON_Delete(json);
  json = post_to(theme_id, "lock", "{\"version\":99,\"section\":\"A\"}", &status);
  check(status == 404, "an unknown version gives 404");
  cJSON_Delete(json);
  check(!file_exists(fake_log), "none of those requests started the Composer");
  check(composition_count() == compositions_before, "none of those requests left a composition");

  /* The fake sees the arguments of a valid request, with absolute paths and no shell. */
  json = exchange_json("POST", "/api/composer/plan",
                       "{\"name\":\"y\",\"form\":\"binary\",\"key\":\"C\",\"meter\":\"4/4\",\"seed\":2}", &status);
  cJSON_Delete(json);
  char *log = read_file(fake_log, NULL);
  check(log && strstr(log, "plan --form binary --key C --meter 4/4 --seed 2 --out ") != NULL &&
            strstr(log, cwd) != NULL,
        "the Composer is started with the validated arguments and absolute paths");
  free(log);

  /* A Composer that runs too long is stopped: 504. */
  setenv("URSATZ_GUI_COMPOSER", fake_sleep, 1);
  setenv("URSATZ_GUI_COMPOSER_TIMEOUT", "1", 1);
  compositions_before = composition_count();
  json = exchange_json("POST", "/api/composer/plan",
                       "{\"name\":\"z\",\"form\":\"binary\",\"key\":\"C\",\"meter\":\"4/4\"}", &status);
  check(status == 504, "a Composer that sleeps past the timeout gives 504");
  cJSON_Delete(json);
  check(composition_count() == compositions_before, "a timed-out run leaves no composition");

  /* No Composer at all: 503. */
  setenv("URSATZ_GUI_COMPOSER", "/nonexistent/ursatz-composer", 1);
  json = exchange_json("GET", "/api/composer/status", NULL, &status);
  check(status == 503, "status gives 503 when the binary is missing");
  cJSON_Delete(json);
  json = exchange_json("POST", "/api/composer/plan",
                       "{\"name\":\"z\",\"form\":\"binary\",\"key\":\"C\",\"meter\":\"4/4\"}", &status);
  check(status == 503, "plan gives 503 when the binary is missing");
  cJSON_Delete(json);

  ursatz_gui_server_stop(ctx);
  return failures == 0 ? 0 : 1;
}
