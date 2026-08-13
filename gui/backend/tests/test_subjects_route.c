/* Subject routes end to end: subjects and their links to items and Library
 * pieces, relations, the groups view, and the subjects returned with an item
 * and with the Analyzer piece detail. Uses a plain TCP socket for the HTTP
 * requests, same as test_styles_route.c. Runs in its own working directory
 * (pieces/, materials/ and subjects/ are created there). */


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
  addr.sin_port = htons(18771);
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


#include "server.h"

#ifndef URSATZ_GUI_TEST_URSATZ_DIR
#error "URSATZ_GUI_TEST_URSATZ_DIR must name the external/ursatz checkout"
#endif

static const char *kMotifBody =
    "{\"uses\":[],\"motif\":{\"role\":\"melodic\",\"form\":\"full\",\"notes\":\"C4:q Eb4:e G4:e C5:h\","
    "\"meter\":\"4/4\",\"key\":\"C minor\",\"pickup\":\"0\",\"features\":[{\"label\":\"a\",\"first\":0,\"last\":2}],"
    "\"analysis\":null,\"derived\":null}}";

/* Creates a subject and returns its id (caller frees), or NULL. */
static char *create_subject(const char *name, const char *kind) {
  char request[512];
  int status = 0;
  snprintf(request, sizeof(request), "{\"name\":\"%s\",\"kind\":\"%s\",\"note\":\"n\"}", name, kind);
  cJSON *answer = json_exchange("POST", "/api/subjects", request, &status);
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
  cJSON *answer = json_exchange("POST", "/api/subjects", "{\"name\":\"Anna\",\"kind\":\"robot\"}", &status);
  check(status == 400 && !ok_true(answer), "a subject with an unknown kind is 400");
  cJSON_Delete(answer);
  answer = json_exchange("POST", "/api/subjects", "{\"kind\":\"idea\"}", &status);
  check(status == 400, "a subject without a name is 400");
  cJSON_Delete(answer);

  char *anna = create_subject("Anna", "character");
  check(anna && strncmp(anna, "sub-", 4) == 0, "a subject is created");
  if (!anna) {
    return;
  }
  char path[256];
  snprintf(path, sizeof(path), "/api/subjects/%s", anna);

  answer = json_exchange("GET", path, NULL, &status);
  check(status == 200 && strcmp(string_of(cJSON_GetObjectItemCaseSensitive(answer, "subject"), "name"), "Anna") == 0,
        "a subject is read by id");
  cJSON_Delete(answer);
  answer = json_exchange("PATCH", path, "{\"name\":\"Anna K\",\"kind\":\"idea\"}", &status);
  check(status == 200 && strcmp(string_of(cJSON_GetObjectItemCaseSensitive(answer, "subject"), "kind"), "idea") == 0,
        "a subject is renamed and re-kinded");
  cJSON_Delete(answer);
  answer = json_exchange("PATCH", path, "{}", &status);
  check(status == 400, "a patch with nothing to change is 400");
  cJSON_Delete(answer);
  answer = json_exchange("GET", "/api/subjects/sub-0000000000000000", NULL, &status);
  check(status == 404, "an unknown subject is 404");
  cJSON_Delete(answer);
  answer = json_exchange("GET", "/api/subjects", NULL, &status);
  check(status == 200 && cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(answer, "subjects")) == 1,
        "the list holds the subject");
  cJSON_Delete(answer);
  cJSON_Delete(json_exchange("PATCH", path, "{\"kind\":\"character\"}", &status));
  *out_id = anna;
}

static void test_relations(const char *anna) {
  int status = 0;
  char *tower = create_subject("The tower", "place");
  char path[400];
  snprintf(path, sizeof(path), "/api/subjects/%s/related/%s", anna, tower);

  cJSON *answer = json_exchange("POST", path, NULL, &status);
  check(status == 200 && !cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(answer, "already")) &&
            array_has(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(answer, "subject"), "related"), tower),
        "two subjects are related");
  cJSON_Delete(answer);
  answer = json_exchange("POST", path, NULL, &status);
  check(status == 200 && cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(answer, "already")), "relating twice reports already");
  cJSON_Delete(answer);

  char other_side[400];
  snprintf(other_side, sizeof(other_side), "/api/subjects/%s", tower);
  answer = json_exchange("GET", other_side, NULL, &status);
  check(array_has(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(answer, "subject"), "related"), anna),
        "the relation is stored on both sides");
  cJSON_Delete(answer);

  char self_path[400];
  snprintf(self_path, sizeof(self_path), "/api/subjects/%s/related/%s", anna, anna);
  answer = json_exchange("POST", self_path, NULL, &status);
  check(status == 400, "relating a subject to itself is 400");
  cJSON_Delete(answer);

  answer = json_exchange("DELETE", path, NULL, &status);
  check(status == 200 && ok_true(answer), "a relation is removed");
  cJSON_Delete(answer);
  answer = json_exchange("DELETE", path, NULL, &status);
  check(status == 404, "removing it again is 404");
  cJSON_Delete(answer);

  /* The groups view: four kinds, in order. */
  answer = json_exchange("GET", "/api/subjects/groups", NULL, &status);
  const cJSON *groups = cJSON_GetObjectItemCaseSensitive(answer, "groups");
  check(status == 200 && cJSON_GetArraySize(groups) == 4 &&
            strcmp(string_of(cJSON_GetArrayItem(groups, 0), "kind"), "character") == 0 &&
            strcmp(string_of(cJSON_GetArrayItem(groups, 1), "kind"), "place") == 0,
        "the groups come in kind order");
  check(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(groups, 1), "subjects")) == 1,
        "a place subject is in the place group");
  cJSON_Delete(answer);

  snprintf(path, sizeof(path), "/api/subjects/%s", tower);
  cJSON_Delete(json_exchange("DELETE", path, NULL, &status));
  check(status == 200, "a subject is deleted");
  free(tower);
}

static void test_links(const char *anna, const char *piece) {
  int status = 0;
  cJSON *answer = json_exchange("GET", "/api/materials/projects", NULL, &status);
  char project[64];
  snprintf(project, sizeof(project), "%s",
           string_of(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(answer, "projects"), 0), "id"));
  cJSON_Delete(answer);

  char request[1024];
  snprintf(request, sizeof(request), "{\"type\":\"motif\",\"name\":\"Opening\",\"project_id\":\"%s\",\"body\":%s}", project,
           kMotifBody);
  answer = json_exchange("POST", "/api/materials/items", request, &status);
  char item[64];
  snprintf(item, sizeof(item), "%s", string_of(cJSON_GetObjectItemCaseSensitive(answer, "item"), "id"));
  cJSON_Delete(answer);

  char path[400];
  snprintf(path, sizeof(path), "/api/subjects/%s/items/%s", anna, item);
  answer = json_exchange("POST", path, NULL, &status);
  check(status == 200 && !cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(answer, "already")) &&
            array_has(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(answer, "subject"), "items"), item),
        "an item is linked to a subject");
  cJSON_Delete(answer);
  answer = json_exchange("POST", path, NULL, &status);
  check(status == 200 && cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(answer, "already")), "linking an item twice reports already");
  cJSON_Delete(answer);
  char missing[400];
  snprintf(missing, sizeof(missing), "/api/subjects/%s/items/mot-0000000000000000", anna);
  answer = json_exchange("POST", missing, NULL, &status);
  check(status == 404, "linking an unknown item is 404");
  cJSON_Delete(answer);

  snprintf(path, sizeof(path), "/api/materials/items/%s", item);
  answer = json_exchange("GET", path, NULL, &status);
  const cJSON *subjects = cJSON_GetObjectItemCaseSensitive(answer, "subjects");
  check(cJSON_GetArraySize(subjects) == 1 && strcmp(string_of(cJSON_GetArrayItem(subjects, 0), "id"), anna) == 0 &&
            strcmp(string_of(cJSON_GetArrayItem(subjects, 0), "name"), "Anna K") == 0,
        "the item answer lists the subject that links it");
  cJSON_Delete(answer);

  char piece_link[400];
  snprintf(piece_link, sizeof(piece_link), "/api/subjects/%s/pieces/%s", anna, piece);
  answer = json_exchange("POST", piece_link, NULL, &status);
  check(status == 200 && array_has(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(answer, "subject"), "pieces"), piece),
        "a Library piece is linked to a subject");
  cJSON_Delete(answer);
  snprintf(missing, sizeof(missing), "/api/subjects/%s/pieces/nope", anna);
  answer = json_exchange("POST", missing, NULL, &status);
  check(status == 404 || status == 400, "linking an unknown piece is refused");
  cJSON_Delete(answer);

  char piece_path[300];
  snprintf(piece_path, sizeof(piece_path), "/api/analyzer/pieces/%s", piece);
  answer = json_exchange("GET", piece_path, NULL, &status);
  subjects = cJSON_GetObjectItemCaseSensitive(answer, "subjects");
  check(ok_true(answer) && cJSON_GetArraySize(subjects) == 1 && strcmp(string_of(cJSON_GetArrayItem(subjects, 0), "id"), anna) == 0,
        "the piece detail lists the subject that links it");
  cJSON_Delete(answer);

  answer = json_exchange("DELETE", piece_link, NULL, &status);
  check(status == 200 && !array_has(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(answer, "subject"), "pieces"), piece),
        "a piece link is removed");
  cJSON_Delete(answer);
  answer = json_exchange("DELETE", piece_link, NULL, &status);
  check(status == 404, "removing a link that is not there is 404");
  cJSON_Delete(answer);
  answer = json_exchange("GET", piece_path, NULL, &status);
  check(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(answer, "subjects")) == 0, "the piece detail has no subjects after unlinking");
  cJSON_Delete(answer);

  /* Deleting an item takes it out of the subject's links. */
  cJSON_Delete(json_exchange("DELETE", path, NULL, &status));
  char subject_path[300];
  snprintf(subject_path, sizeof(subject_path), "/api/subjects/%s", anna);
  answer = json_exchange("GET", subject_path, NULL, &status);
  check(!array_has(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(answer, "subject"), "items"), item),
        "a deleted item leaves the subject's links");
  cJSON_Delete(answer);
}

int main(void) {
#ifdef _WIN32
  WSADATA wsa_data;
  WSAStartup(MAKEWORD(2, 2), &wsa_data);
#endif

  /* Fresh state: this test's working directory is its own. */
#ifdef _WIN32
  system("rmdir /s /q pieces collections materials subjects data 2> nul");
#else
  system("rm -rf pieces collections materials subjects data");
#endif
  MKDIR("pieces");

  char clementi_path[512];
  snprintf(clementi_path, sizeof(clementi_path), "%s/Clementi 36.mid",
           URSATZ_GUI_TEST_URSATZ_DIR "/tests/fixtures/framework-v1-corpus");

  struct mg_context *ctx = ursatz_gui_server_start("18771", ".");
  check(ctx != NULL, "server starts");
  if (!ctx) {
    return 1;
  }
  sleep_ms(200);

  char *piece = upload_file(clementi_path, "Clementi%2036.mid");
  check(piece != NULL, "Clementi 36 uploads");
  char *anna = NULL;
  test_subject_crud(&anna);
  if (anna && piece) {
    test_relations(anna);
    test_links(anna, piece);
  }

  free(anna);
  free(piece);
  ursatz_gui_server_stop(ctx);

  if (failures > 0) {
    fprintf(stderr, "%d check(s) failed\n", failures);
    return 1;
  }
  return 0;
}
