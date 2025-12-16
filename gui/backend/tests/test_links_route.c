/* Motive links end to end: the link fixtures are uploaded through the Analyzer route (three copies of each, so a
 * first run takes a moment), then POST /api/links/runs starts a run that is polled to done, a second start while it
 * runs is refused, bad, unknown and empty scopes are answered, a second scope reuses the cache, and the list, read,
 * delete and per-piece routes work on what is stored under links/. Uses a plain TCP socket for the HTTP requests, same
 * as test_analyzer_route.c. Runs in its own working directory (pieces/, collections/, materials/, subjects/ and
 * links/ are created there). */

#include <civetweb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef SOCKET socket_t;
#define CLOSESOCK closesocket
#define MKDIR(path) _mkdir(path)
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
typedef int socket_t;
#define CLOSESOCK close
#define INVALID_SOCKET (-1)
#define MKDIR(path) mkdir(path, 0755)
#endif

#include <cJSON.h>

#include "server.h"

#ifndef URSATZ_GUI_TEST_LINK_FIXTURES
#error "URSATZ_GUI_TEST_LINK_FIXTURES must name the link fixtures directory"
#endif

static void sleep_ms(int ms) {
#ifdef _WIN32
  Sleep((DWORD)ms);
#else
  struct timespec ts = {0, (long)ms * 1000 * 1000};
  nanosleep(&ts, NULL);
#endif
}

static int failures = 0;

static void check(bool cond, const char *what) {
  if (!cond) {
    fprintf(stderr, "FAIL: %s\n", what);
    failures++;
  } else {
    printf("PASS: %s\n", what);
  }
}

/* One HTTP exchange. extra_headers is zero or more complete "Name: value\r\n"
 * lines. Returns the whole response (status line, headers, body) as a newly
 * allocated string, or NULL when the connection failed. */
static char *http_exchange(const char *method, const char *path, const char *extra_headers, const void *body,
                           size_t body_len) {
  socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock == INVALID_SOCKET) {
    return NULL;
  }

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(18772);
  inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

  if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
    CLOSESOCK(sock);
    return NULL;
  }

  char header[1024];
  int header_len = snprintf(header, sizeof(header),
                            "%s %s HTTP/1.1\r\nHost: 127.0.0.1\r\n%sContent-Length: %zu\r\nConnection: close\r\n\r\n",
                            method, path, extra_headers ? extra_headers : "", body_len);
  send(sock, header, header_len, 0);
  const char *cursor = body;
  size_t remaining = body_len;
  while (remaining > 0) {
    int n = send(sock, cursor, remaining > 65536 ? 65536 : (int)remaining, 0);
    if (n <= 0) {
      break;
    }
    cursor += n;
    remaining -= (size_t)n;
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
    int n = recv(sock, response + total, (int)(capacity - 1 - total), 0);
    if (n <= 0) {
      break;
    }
    total += (size_t)n;
  }
  CLOSESOCK(sock);
  if (response) {
    response[total] = '\0';
  }
  return response;
}

/* The part of a response after its headers. */
static const char *response_body(const char *response) {
  const char *split = response ? strstr(response, "\r\n\r\n") : NULL;
  return split ? split + 4 : "";
}

static bool response_has_status(const char *response, int status) {
  char needle[32];
  snprintf(needle, sizeof(needle), "HTTP/1.1 %d", status);
  return response && strncmp(response, needle, strlen(needle)) == 0;
}

static char *read_file(const char *path, size_t *out_size) {
  FILE *file = fopen(path, "rb");
  if (!file) {
    return NULL;
  }
  size_t capacity = 65536;
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
  if (buffer) {
    buffer[length] = '\0';
    if (out_size) {
      *out_size = length;
    }
  }
  return buffer;
}

static bool file_exists(const char *path) {
  FILE *file = fopen(path, "rb");
  if (file) {
    fclose(file);
  }
  return file != NULL;
}

/* An exchange with a JSON body; *status gets the HTTP status and the result is the parsed body (caller deletes). */
static cJSON *json_exchange(const char *method, const char *path, const char *body, int *status) {
  char *response = http_exchange(method, path, "Content-Type: application/json\r\n", body ? body : "",
                                 body ? strlen(body) : 0);
  *status = 0;
  if (response && strncmp(response, "HTTP/1.1 ", 9) == 0) {
    *status = atoi(response + 9);
  }
  cJSON *json = response ? cJSON_Parse(response_body(response)) : NULL;
  free(response);
  return json;
}

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

/* POSTs the file at path with the given upload name; returns the new piece's id (caller frees), or NULL. */
static char *upload_file(const char *path, const char *encoded_filename) {
  size_t size = 0;
  char *data = read_file(path, &size);
  if (!data) {
    return NULL;
  }
  char headers[512];
  snprintf(headers, sizeof(headers), "X-Filename: %s\r\n", encoded_filename);
  char *response = http_exchange("POST", "/api/analyzer/pieces", headers, data, size);
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

/* PATCHes a piece with a JSON body. */
static void patch_piece(const char *id, const char *body) {
  char path[256];
  int status = 0;
  snprintf(path, sizeof(path), "/api/analyzer/pieces/%s", id);
  cJSON_Delete(json_exchange("PATCH", path, body, &status));
}


/* The start request for a scope given as JSON text. */
static cJSON *start_run(const char *scope, int *status) {
  char body[512];
  snprintf(body, sizeof(body), "{\"scope\":%s}", scope);
  return json_exchange("POST", "/api/links/runs", body, status);
}

static const cJSON *run_of(const cJSON *answer) {
  return cJSON_GetObjectItemCaseSensitive(answer, "run");
}

/* Polls the run until it is no longer running; returns its answer (caller deletes). */
static cJSON *wait_for_run(const char *id) {
  char path[256];
  snprintf(path, sizeof(path), "/api/links/runs/%s", id);
  cJSON *answer = NULL;
  for (int waited = 0; waited < 600000; waited += 100) {
    int status = 0;
    cJSON_Delete(answer);
    answer = json_exchange("GET", path, NULL, &status);
    if (status != 200 || strcmp(string_of(run_of(answer), "status"), "running") != 0) {
      break;
    }
    sleep_ms(100);
  }
  return answer;
}

static double result_number(const cJSON *run, const char *key) {
  return number_of(cJSON_GetObjectItemCaseSensitive(run, "result"), key);
}

/* The first link of the run's result. */
static const cJSON *first_link(const cJSON *run) {
  return cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(run, "result"), "links"), 0);
}

int main(void) {
#ifdef _WIN32
  WSADATA wsa_data;
  WSAStartup(MAKEWORD(2, 2), &wsa_data);
#endif

  /* Fresh state: this test's working directory is its own. */
#ifdef _WIN32
  system("rmdir /s /q pieces collections materials subjects links data 2> nul");
#else
  system("rm -rf pieces collections materials subjects links data");
#endif
  MKDIR("pieces");

  struct mg_context *ctx = ursatz_gui_server_start("18772", ".");
  check(ctx != NULL, "server starts");
  if (!ctx) {
    return 1;
  }
  sleep_ms(200);

  /* Three copies of each fixture: a and b (composer Anna), c, and d (composer Zed). */
  const char *names[] = {"a.musicxml", "b.musicxml", "c.musicxml", "d.musicxml"};
  char *ids[12] = {0};
  bool uploaded = true;
  for (int copy = 0; copy < 3; copy++) {
    for (int i = 0; i < 4; i++) {
      char path[512];
      snprintf(path, sizeof(path), "%s/%s", URSATZ_GUI_TEST_LINK_FIXTURES, names[i]);
      ids[copy * 4 + i] = upload_file(path, names[i]);
      uploaded = uploaded && ids[copy * 4 + i] != NULL;
      if (ids[copy * 4 + i]) {
        patch_piece(ids[copy * 4 + i], i < 2 ? "{\"composer\":\"Anna\"}" : "{\"composer\":\"Zed\"}");
      }
    }
  }
  check(uploaded, "the fixtures upload");
  if (!uploaded) {
    ursatz_gui_server_stop(ctx);
    return 1;
  }

  int status = 0;
  cJSON *answer = json_exchange("GET", "/api/links/runs", NULL, &status);
  check(status == 200 && ok_true(answer) && cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(answer, "runs")) == 0,
        "no runs are listed before one starts");
  cJSON_Delete(answer);

  /* Bad, unknown and empty scopes. */
  answer = json_exchange("POST", "/api/links/runs", "not json", &status);
  check(status == 400 && !ok_true(answer), "a body that is not JSON answers 400");
  cJSON_Delete(answer);
  answer = json_exchange("POST", "/api/links/runs", "{}", &status);
  check(status == 400, "a request without a scope answers 400");
  cJSON_Delete(answer);
  answer = start_run("{\"kind\":\"everything\"}", &status);
  check(status == 400, "an unknown kind answers 400");
  cJSON_Delete(answer);
  answer = start_run("{\"kind\":\"collection\"}", &status);
  check(status == 400, "a collection scope without an id answers 400");
  cJSON_Delete(answer);
  answer = start_run("{\"kind\":\"filter\",\"filter\":{}}", &status);
  check(status == 400, "a filter without a value answers 400");
  cJSON_Delete(answer);
  answer = start_run("{\"kind\":\"filter\",\"filter\":{\"tag\":7}}", &status);
  check(status == 400, "a filter value that is not text answers 400");
  cJSON_Delete(answer);
  answer = start_run("{\"kind\":\"collection\",\"collection_id\":\"col-0000000000000000\"}", &status);
  check(status == 404, "an unknown collection answers 404");
  cJSON_Delete(answer);
  answer = start_run("{\"kind\":\"subject\",\"subject_id\":\"sub-0000000000000000\"}", &status);
  check(status == 404, "an unknown subject answers 404");
  cJSON_Delete(answer);
  answer = start_run("{\"kind\":\"project\",\"project_id\":\"prj-0000000000000000\"}", &status);
  check(status == 404, "an unknown project answers 404");
  cJSON_Delete(answer);
  answer = start_run("{\"kind\":\"filter\",\"filter\":{\"composer\":\"Nobody\"}}", &status);
  check(status == 422 && !ok_true(answer), "a scope without pieces answers 422");
  cJSON_Delete(answer);

  /* A run over every piece: accepted, refused while it runs, polled to done. */
  answer = start_run("{\"kind\":\"all\"}", &status);
  check(status == 202 && ok_true(answer), "starting a run answers 202");
  const cJSON *run = run_of(answer);
  check(strncmp(string_of(run, "id"), "lnk-", 4) == 0, "the run id starts with lnk-");
  check(strcmp(string_of(cJSON_GetObjectItemCaseSensitive(run, "scope"), "kind"), "all") == 0, "the run keeps its scope");
  char first_id[64];
  snprintf(first_id, sizeof(first_id), "%s", string_of(run, "id"));
  cJSON_Delete(answer);

  answer = start_run("{\"kind\":\"all\"}", &status);
  check(status == 409 && !ok_true(answer), "a second start while a run runs answers 409");
  cJSON_Delete(answer);

  char first_path[128];
  snprintf(first_path, sizeof(first_path), "/api/links/runs/%s", first_id);
  answer = json_exchange("DELETE", first_path, NULL, &status);
  check(status == 409, "deleting a run that runs answers 409");
  cJSON_Delete(answer);

  answer = wait_for_run(first_id);
  run = run_of(answer);
  check(ok_true(answer) && strcmp(string_of(run, "status"), "done") == 0, "the run reaches done");
  check(result_number(run, "pieces") == 12, "the run linked all twelve pieces");
  /* The copies of a fixture have one sketch, so the cache has a pair once for all the copies. */
  check(result_number(run, "pairs") > 0 && result_number(run, "compared") > 0 &&
            result_number(run, "compared") + result_number(run, "cached") == result_number(run, "pairs"),
        "the first run compares pairs");
  check(number_of(run, "done") == result_number(run, "pairs") && number_of(run, "total") == result_number(run, "pairs"),
        "done and total are the pairs");
  check(cJSON_IsString(cJSON_GetObjectItemCaseSensitive(run, "finished_at")), "a finished run has an end time");
  const cJSON *link = first_link(run);
  check(link != NULL, "the run found links");
  check(strcmp(string_of(cJSON_GetObjectItemCaseSensitive(link, "a"), "title"), "") != 0 &&
            strcmp(string_of(cJSON_GetObjectItemCaseSensitive(link, "b"), "title"), "") != 0,
        "each end of a link has a title");
  bool titled_group = false;
  const cJSON *groups = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(run, "result"), "groups");
  if (cJSON_GetArraySize(groups) > 0) {
    const cJSON *member = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(groups, 0), "members"), 0);
    titled_group = strcmp(string_of(member, "title"), "") != 0;
  }
  check(titled_group, "a group member has a title");
  int pairs = (int)result_number(run, "pairs");
  cJSON_Delete(answer);

  answer = json_exchange("GET", "/api/links/runs/lnk-0000000000000000", NULL, &status);
  check(status == 404 && !ok_true(answer), "an unknown run answers 404");
  cJSON_Delete(answer);

  /* A second scope over pieces already compared: the shared cache compares nothing. */
  answer = start_run("{\"kind\":\"filter\",\"filter\":{\"composer\":\"Anna\"}}", &status);
  check(status == 202, "a run over a composer starts");
  char second_id[64];
  snprintf(second_id, sizeof(second_id), "%s", string_of(run_of(answer), "id"));
  cJSON_Delete(answer);
  answer = wait_for_run(second_id);
  run = run_of(answer);
  check(strcmp(string_of(run, "status"), "done") == 0 && result_number(run, "pieces") == 6, "the composer run is done");
  check(result_number(run, "compared") == 0 && result_number(run, "cached") == result_number(run, "pairs"),
        "a second scope reuses the cache");
  check(pairs > result_number(run, "pairs"), "the composer's scope has fewer pairs");
  cJSON_Delete(answer);

  /* A second run over the same scope: nothing to compare. */
  answer = start_run("{\"kind\":\"all\"}", &status);
  char third_id[64];
  snprintf(third_id, sizeof(third_id), "%s", string_of(run_of(answer), "id"));
  cJSON_Delete(answer);
  answer = wait_for_run(third_id);
  run = run_of(answer);
  check(strcmp(string_of(run, "status"), "done") == 0 && result_number(run, "compared") == 0 &&
            result_number(run, "cached") == pairs,
        "a second run over the same scope compares 0");
  cJSON_Delete(answer);

  /* The list: newest first, without results. */
  answer = json_exchange("GET", "/api/links/runs", NULL, &status);
  const cJSON *runs = cJSON_GetObjectItemCaseSensitive(answer, "runs");
  check(status == 200 && cJSON_GetArraySize(runs) == 3, "three runs are listed");
  check(strcmp(string_of(cJSON_GetArrayItem(runs, 0), "id"), third_id) == 0 &&
            strcmp(string_of(cJSON_GetArrayItem(runs, 2), "id"), first_id) == 0,
        "runs are listed newest first");
  check(strcmp(string_of(cJSON_GetArrayItem(runs, 0), "status"), "done") == 0 &&
            cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(runs, 0), "result") == NULL,
        "a listed run has a status and no result");
  cJSON_Delete(answer);

  /* A piece's links: from the newest done run of each scope. */
  char piece_path[128];
  snprintf(piece_path, sizeof(piece_path), "/api/links/pieces/%s", ids[0]);
  answer = json_exchange("GET", piece_path, NULL, &status);
  const cJSON *links = cJSON_GetObjectItemCaseSensitive(answer, "links");
  check(status == 200 && ok_true(answer) && cJSON_GetArraySize(links) > 0, "a's links are listed");
  bool from_third = false;
  bool from_second = false;
  bool from_first = false;
  bool titled = true;
  bool mine = true;
  const cJSON *entry = NULL;
  cJSON_ArrayForEach(entry, links) {
    from_third = from_third || strcmp(string_of(entry, "run"), third_id) == 0;
    from_second = from_second || strcmp(string_of(entry, "run"), second_id) == 0;
    from_first = from_first || strcmp(string_of(entry, "run"), first_id) == 0;
    const cJSON *entry_link = cJSON_GetObjectItemCaseSensitive(entry, "link");
    const cJSON *a = cJSON_GetObjectItemCaseSensitive(entry_link, "a");
    const cJSON *b = cJSON_GetObjectItemCaseSensitive(entry_link, "b");
    titled = titled && strcmp(string_of(a, "title"), "") != 0 && strcmp(string_of(b, "title"), "") != 0;
    mine = mine && (strcmp(string_of(a, "piece"), ids[0]) == 0 || strcmp(string_of(b, "piece"), ids[0]) == 0);
  }
  check(from_third && from_second && !from_first, "links come from the newest done run of each scope");
  check(titled && mine, "each link names the piece and carries titles");
  cJSON_Delete(answer);
  snprintf(piece_path, sizeof(piece_path), "/api/links/pieces/%s", ids[2]);
  answer = json_exchange("GET", piece_path, NULL, &status);
  bool only_copies = status == 200;
  cJSON_ArrayForEach(entry, cJSON_GetObjectItemCaseSensitive(answer, "links")) {
    const cJSON *entry_link = cJSON_GetObjectItemCaseSensitive(entry, "link");
    const char *a = string_of(cJSON_GetObjectItemCaseSensitive(entry_link, "a"), "piece");
    const char *b = string_of(cJSON_GetObjectItemCaseSensitive(entry_link, "b"), "piece");
    const char *other = strcmp(a, ids[2]) == 0 ? b : a;
    only_copies = only_copies && (strcmp(other, ids[6]) == 0 || strcmp(other, ids[10]) == 0);
  }
  check(only_copies, "c's motive is linked to nothing but its own copies");
  cJSON_Delete(answer);
  answer = json_exchange("GET", "/api/links/pieces/..", NULL, &status);
  check(status == 404, "a bad piece id answers 404");
  cJSON_Delete(answer);

  /* Deleting. */
  answer = json_exchange("DELETE", first_path, NULL, &status);
  check(status == 200 && ok_true(answer), "deleting a finished run answers 200");
  cJSON_Delete(answer);
  answer = json_exchange("GET", first_path, NULL, &status);
  check(status == 404, "a deleted run answers 404");
  cJSON_Delete(answer);
  answer = json_exchange("DELETE", first_path, NULL, &status);
  check(status == 404, "deleting it again answers 404");
  cJSON_Delete(answer);
  answer = json_exchange("GET", "/api/links/runs", NULL, &status);
  check(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(answer, "runs")) == 2, "two runs are left");
  cJSON_Delete(answer);
  check(file_exists("links/motive-link-cache.json"), "the cache is kept under links/");
  char deleted_file[160];
  snprintf(deleted_file, sizeof(deleted_file), "links/runs/%s/run.json", first_id);
  check(!file_exists(deleted_file), "the deleted run's file is gone");

  for (int i = 0; i < 12; i++) {
    free(ids[i]);
  }
  ursatz_gui_server_stop(ctx);
#ifdef _WIN32
  system("rmdir /s /q pieces collections materials subjects links data 2> nul");
#else
  system("rm -rf pieces collections materials subjects links data");
#endif

  if (failures > 0) {
    fprintf(stderr, "%d check(s) failed\n", failures);
    return 1;
  }
  return 0;
}
