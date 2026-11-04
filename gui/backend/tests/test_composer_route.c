/* The Composer route is the status check only: it reports the versions the
 * linked Composer was built with (composer_api_versions), and the old composition and
 * grow/plan routes are gone (404). Uses a plain TCP socket for the HTTP
 * requests, same as test_analyzer_route.c. */

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
#include "ursatz_composer/composer_api.h"

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

int main(void) {
  struct mg_context *ctx = ursatz_gui_server_start("18768", ".");
  check(ctx != NULL, "server starts");
  if (!ctx) {
    return 1;
  }
  sleep_ms(200);

  int status = 0;
  cJSON *json = exchange_json("GET", "/api/composer/status", NULL, &status);
  const cJSON *version_text = json ? cJSON_GetObjectItemCaseSensitive(json, "version") : NULL;
  check(status == 200 && cJSON_IsString(version_text) && strncmp(version_text->valuestring, "ursatz-composer", 15) == 0,
        "status reports the Composer version");
  const cJSON *api = cJSON_GetObjectItemCaseSensitive(json, "api_version");
  const cJSON *analyzer = cJSON_GetObjectItemCaseSensitive(json, "analyzer_version");
  const cJSON *commit = cJSON_GetObjectItemCaseSensitive(json, "ursatz_commit");
  check(cJSON_IsString(api) && strcmp(api->valuestring, composer_api_version()) == 0,
        "status reports the API version the backend is linked with");
  check(cJSON_IsString(analyzer) && analyzer->valuestring[0] != '\0', "status reports the Analyzer version");
  check(cJSON_IsString(commit) && commit->valuestring[0] != '\0', "status reports the Ursatz commit");
  cJSON_Delete(json);

  json = exchange_json("POST", "/api/composer/status", "{}", &status);
  check(status == 405, "status accepts only GET");
  cJSON_Delete(json);

  /* The vocabulary: the Composer's lists, kept for the process, equal to the fixture the frontend tests load. */
  json = exchange_json("GET", "/api/composer/vocabulary", NULL, &status);
  check(status == 200 && cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(json, "meters")) &&
            cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(json, "pitch_class_names")) &&
            cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(json, "limits")),
        "the vocabulary route gives the Composer's lists");
  cJSON *again = exchange_json("GET", "/api/composer/vocabulary", NULL, &status);
  check(status == 200 && cJSON_Compare(json, again, 1), "the vocabulary is the same on every call");
  cJSON_Delete(again);
  FILE *fixture_file = fopen(URSATZ_GUI_TEST_VOCABULARY_FIXTURE, "rb");
  char fixture_text[65536];
  size_t fixture_size = fixture_file ? fread(fixture_text, 1, sizeof(fixture_text) - 1, fixture_file) : 0;
  if (fixture_file) {
    fclose(fixture_file);
  }
  fixture_text[fixture_size] = '\0';
  cJSON *fixture = cJSON_Parse(fixture_text);
  check(fixture && cJSON_Compare(json, fixture, 1), "the frontend vocabulary fixture is what the route gives");
  cJSON_Delete(fixture);
  cJSON_Delete(json);
  json = exchange_json("POST", "/api/composer/vocabulary", "{}", &status);
  check(status == 405, "the vocabulary accepts only GET");
  cJSON_Delete(json);

  json = exchange_json("GET", "/api/composer/compositions", NULL, &status);
  check(status == 404, "the compositions route is gone");
  cJSON_Delete(json);
  json = exchange_json("POST", "/api/composer/grow", "{}", &status);
  check(status == 404, "the grow route is gone");
  cJSON_Delete(json);
  json = exchange_json("POST", "/api/composer/plan", "{}", &status);
  check(status == 404, "the plan route is gone");
  cJSON_Delete(json);

  ursatz_gui_server_stop(ctx);
  return failures == 0 ? 0 : 1;
}
