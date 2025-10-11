/* Material and subject routes end to end: projects, items, versions and uses
 * under /api/materials, with the status codes the routes promise (400, 404,
 * 409, 413). Uses a plain TCP socket for the HTTP requests, same as
 * test_styles_route.c. Runs in its own working directory (materials/ and
 * subjects/ are created there). */


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
  addr.sin_port = htons(18770);
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

#include "server.h"

static const char *kMotifBody =
    "{\"uses\":[],\"motif\":{\"role\":\"melodic\",\"form\":\"full\",\"notes\":\"D4:q F4:e A4:e D5:h\","
    "\"meter\":\"4/4\",\"key\":\"D minor\",\"pickup\":\"0\",\"features\":[{\"label\":\"a\",\"first\":0,\"last\":2}],"
    "\"analysis\":null,\"derived\":null}}";
static const char *kBadMotifBody =
    "{\"uses\":[],\"motif\":{\"role\":\"melodic\",\"form\":\"full\",\"notes\":\"zzz\",\"meter\":\"4/4\",\"key\":null,"
    "\"pickup\":\"0\",\"features\":[],\"analysis\":null,\"derived\":null}}";
static const char *kThemeBody = "{\"uses\":[],\"theme\":{\"form\":null,\"slots\":[]}}";

/* Sends {"type","name","project_id","body"} and returns the item id (caller frees), or NULL. */
static char *create_item(const char *type, const char *name, const char *project_id, const char *body_text) {
  char request[2048];
  int status = 0;
  snprintf(request, sizeof(request), "{\"type\":\"%s\",\"name\":\"%s\",\"project_id\":\"%s\",\"body\":%s}", type, name,
           project_id, body_text);
  cJSON *answer = json_exchange("POST", "/api/materials/items", request, &status);
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(answer, "item");
  char *id = status == 200 && item ? strdup(string_of(item, "id")) : NULL;
  cJSON_Delete(answer);
  return id;
}

/* A theme body whose one use names source at version (the copy is replaced by the server). */
static void theme_using(const char *source, int version, char *out, size_t size) {
  snprintf(out, size,
           "{\"uses\":[{\"item_id\":\"%s\",\"version\":%d,\"copy\":{}}],\"theme\":{\"form\":null,\"slots\":[]}}", source,
           version);
}

static void test_projects(void) {
  int status = 0;
  cJSON *answer = json_exchange("GET", "/api/materials/projects", NULL, &status);
  const cJSON *projects = cJSON_GetObjectItemCaseSensitive(answer, "projects");
  check(status == 200 && ok_true(answer) && cJSON_GetArraySize(projects) == 1, "a fresh store lists the General project");
  check(strcmp(string_of(cJSON_GetArrayItem(projects, 0), "name"), "General") == 0, "the first project is General");
  cJSON_Delete(answer);

  answer = json_exchange("POST", "/api/materials/projects", "{\"name\":\"Sonata\",\"description\":\"first\"}", &status);
  const cJSON *project = cJSON_GetObjectItemCaseSensitive(answer, "project");
  check(status == 200 && ok_true(answer) && strncmp(string_of(project, "id"), "prj-", 4) == 0, "a project is created");
  char path[256];
  snprintf(path, sizeof(path), "/api/materials/projects/%s", string_of(project, "id"));
  cJSON_Delete(answer);

  answer = json_exchange("POST", "/api/materials/projects", "{\"description\":\"no name\"}", &status);
  check(status == 400 && !ok_true(answer), "a project without a name is 400");
  cJSON_Delete(answer);
  answer = json_exchange("POST", "/api/materials/projects", "[1]", &status);
  check(status == 400, "a body that is not an object is 400");
  cJSON_Delete(answer);

  answer = json_exchange("GET", path, NULL, &status);
  check(status == 200 && strcmp(string_of(cJSON_GetObjectItemCaseSensitive(answer, "project"), "name"), "Sonata") == 0,
        "a project is read by id");
  cJSON_Delete(answer);
  answer = json_exchange("PATCH", path, "{\"name\":\"Sonatina\"}", &status);
  check(status == 200 && strcmp(string_of(cJSON_GetObjectItemCaseSensitive(answer, "project"), "name"), "Sonatina") == 0,
        "a project is renamed");
  cJSON_Delete(answer);
  answer = json_exchange("PATCH", path, "{}", &status);
  check(status == 400, "a patch with nothing to change is 400");
  cJSON_Delete(answer);
  answer = json_exchange("GET", "/api/materials/projects/prj-0000000000000000", NULL, &status);
  check(status == 404, "an unknown project is 404");
  cJSON_Delete(answer);
  answer = json_exchange("PUT", path, "{}", &status);
  check(status == 405, "an unsupported method is 405");
  cJSON_Delete(answer);

  answer = json_exchange("DELETE", path, NULL, &status);
  check(status == 200 && ok_true(answer), "an empty project is deleted");
  cJSON_Delete(answer);
  answer = json_exchange("DELETE", path, NULL, &status);
  check(status == 404, "deleting it again is 404");
  cJSON_Delete(answer);
}

static void test_items(void) {
  int status = 0;
  cJSON *answer = json_exchange("GET", "/api/materials/projects", NULL, &status);
  char project[64];
  snprintf(project, sizeof(project), "%s",
           string_of(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(answer, "projects"), 0), "id"));
  cJSON_Delete(answer);

  char *motif = create_item("motif", "Opening", project, kMotifBody);
  check(motif && strncmp(motif, "mot-", 4) == 0, "a motif item is created");
  if (!motif) {
    return;
  }
  char path[256];
  snprintf(path, sizeof(path), "/api/materials/items/%s", motif);

  char request[1024];
  snprintf(request, sizeof(request), "{\"type\":\"chord\",\"name\":\"x\",\"project_id\":\"%s\",\"body\":%s}", project,
           kMotifBody);
  answer = json_exchange("POST", "/api/materials/items", request, &status);
  check(status == 400, "an unknown item type is 400");
  cJSON_Delete(answer);
  snprintf(request, sizeof(request), "{\"type\":\"motif\",\"name\":\"x\",\"project_id\":\"%s\",\"body\":{\"uses\":[]}}",
           project);
  answer = json_exchange("POST", "/api/materials/items", request, &status);
  check(status == 400, "an invalid body is 400");
  cJSON_Delete(answer);
  snprintf(request, sizeof(request),
           "{\"type\":\"motif\",\"name\":\"x\",\"project_id\":\"prj-0000000000000000\",\"body\":%s}", kMotifBody);
  answer = json_exchange("POST", "/api/materials/items", request, &status);
  check(status == 404, "an item in an unknown project is 404");
  cJSON_Delete(answer);

  answer = json_exchange("GET", path, NULL, &status);
  check(status == 200 && cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(answer, "body")) &&
            cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(answer, "uses_status")) &&
            cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(answer, "used_by")) &&
            cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(answer, "subjects")),
        "an item answers with meta, body, uses_status, used_by and subjects");
  check(number_of(cJSON_GetObjectItemCaseSensitive(answer, "item"), "current_version") == 1, "a new item is at version 1");
  cJSON_Delete(answer);

  answer = json_exchange("PATCH", path, "{\"name\":\"Opening bars\"}", &status);
  check(status == 200 && strcmp(string_of(cJSON_GetObjectItemCaseSensitive(answer, "item"), "name"), "Opening bars") == 0,
        "an item is renamed");
  cJSON_Delete(answer);
  answer = json_exchange("PATCH", path, "{\"project_id\":\"prj-0000000000000000\"}", &status);
  check(status == 404, "moving an item to an unknown project is 404");
  cJSON_Delete(answer);

  char list_path[256];
  snprintf(list_path, sizeof(list_path), "/api/materials/items?project=%s&type=motif", project);
  answer = json_exchange("GET", list_path, NULL, &status);
  check(status == 200 && cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(answer, "items")) == 1,
        "items are listed by project and type");
  cJSON_Delete(answer);
  snprintf(list_path, sizeof(list_path), "/api/materials/items?project=%s&type=theme", project);
  answer = json_exchange("GET", list_path, NULL, &status);
  check(status == 200 && cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(answer, "items")) == 0,
        "the type filter narrows the list");
  cJSON_Delete(answer);

  /* Versions. */
  char versions_path[320];
  snprintf(versions_path, sizeof(versions_path), "%s/versions", path);
  snprintf(request, sizeof(request), "{\"note\":\"longer\",\"body\":%s}", kMotifBody);
  answer = json_exchange("POST", versions_path, request, &status);
  check(status == 200 && number_of(cJSON_GetObjectItemCaseSensitive(answer, "item"), "current_version") == 2,
        "a new version becomes current");
  cJSON_Delete(answer);
  answer = json_exchange("POST", versions_path, "{\"body\":{\"uses\":[]}}", &status);
  check(status == 400, "an invalid version body is 400");
  cJSON_Delete(answer);
  snprintf(request, sizeof(request), "%s/1", versions_path);
  answer = json_exchange("GET", request, NULL, &status);
  check(status == 200 && number_of(answer, "number") == 1 && cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(answer, "body")),
        "version 1 is still readable");
  cJSON_Delete(answer);
  snprintf(request, sizeof(request), "%s/9", versions_path);
  answer = json_exchange("GET", request, NULL, &status);
  check(status == 404, "a version past the current one is 404");
  cJSON_Delete(answer);
  snprintf(request, sizeof(request), "%s/abc", versions_path);
  answer = json_exchange("GET", request, NULL, &status);
  check(status == 404, "a version that is not a number is 404");
  cJSON_Delete(answer);

  /* A theme that uses the motif at version 2. */
  char theme_body[1024];
  theme_using(motif, 2, theme_body, sizeof(theme_body));
  char *theme = create_item("theme", "Theme A", project, theme_body);
  check(theme != NULL, "a theme that uses the motif is created");

  answer = json_exchange("DELETE", path, NULL, &status);
  const cJSON *used_by = cJSON_GetObjectItemCaseSensitive(answer, "used_by");
  check(status == 409 && !ok_true(answer) && cJSON_GetArraySize(used_by) == 1 &&
            strcmp(string_of(cJSON_GetArrayItem(used_by, 0), "id"), theme ? theme : "") == 0,
        "deleting a used item is 409 with used_by");
  cJSON_Delete(answer);

  /* A third motif version makes the theme's use out of date. */
  snprintf(request, sizeof(request), "{\"note\":\"again\",\"body\":%s}", kMotifBody);
  cJSON_Delete(json_exchange("POST", versions_path, request, &status));
  char theme_path[256];
  snprintf(theme_path, sizeof(theme_path), "/api/materials/items/%s", theme);
  answer = json_exchange("GET", theme_path, NULL, &status);
  const cJSON *uses_status = cJSON_GetObjectItemCaseSensitive(answer, "uses_status");
  check(cJSON_GetArraySize(uses_status) == 1 &&
            cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(uses_status, 0), "updated")),
        "the theme's use reports updated");
  cJSON_Delete(answer);

  char update_path[400];
  snprintf(update_path, sizeof(update_path), "%s/uses/%s/update", theme_path, motif);
  answer = json_exchange("POST", update_path, NULL, &status);
  check(status == 200 && number_of(cJSON_GetObjectItemCaseSensitive(answer, "item"), "current_version") == 2,
        "updating a use writes a new version of the theme");
  cJSON_Delete(answer);
  snprintf(update_path, sizeof(update_path), "%s/uses/mot-0000000000000000/update", theme_path);
  answer = json_exchange("POST", update_path, NULL, &status);
  check(status == 404, "updating a use that is not there is 404");
  cJSON_Delete(answer);

  char force_path[300];
  snprintf(force_path, sizeof(force_path), "%s?force=1", path);
  answer = json_exchange("DELETE", force_path, NULL, &status);
  check(status == 200 && ok_true(answer), "a forced delete removes a used item");
  cJSON_Delete(answer);
  answer = json_exchange("GET", path, NULL, &status);
  check(status == 404, "the deleted item is 404");
  cJSON_Delete(answer);
  answer = json_exchange("GET", theme_path, NULL, &status);
  check(status == 200 && cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(
                             cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(answer, "uses_status"), 0), "missing")),
        "the theme's use reports missing after the force delete");
  cJSON_Delete(answer);

  /* A project that still holds an item cannot be deleted. */
  char project_path[256];
  snprintf(project_path, sizeof(project_path), "/api/materials/projects/%s", project);
  answer = json_exchange("DELETE", project_path, NULL, &status);
  check(status == 409, "deleting a project that holds items is 409");
  cJSON_Delete(answer);

  free(theme);
  free(motif);
}

/* The Composer's analysis is stored with a saved motif (needs the Composer built with the backend). */
static void test_motif_analysis(void) {
  int status = 0;
  char project[64] = "";
  cJSON *answer = json_exchange("GET", "/api/materials/projects", NULL, &status);
  snprintf(project, sizeof(project), "%s",
           string_of(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(answer, "projects"), 0), "id"));
  cJSON_Delete(answer);

  char *motif = create_item("motif", "Analyzed", project, kMotifBody);
  check(motif != NULL, "a full motif is saved");
  if (!motif) {
    return;
  }
  char path[256];
  snprintf(path, sizeof(path), "/api/materials/items/%s", motif);
  answer = json_exchange("GET", path, NULL, &status);
  const cJSON *body = cJSON_GetObjectItemCaseSensitive(answer, "body");
  const cJSON *analysis = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(body, "motif"), "analysis");
  const cJSON *inner = cJSON_GetObjectItemCaseSensitive(analysis, "analysis");
  check(cJSON_IsObject(analysis), "the saved motif carries the Composer's analysis");
  check(cJSON_GetObjectItemCaseSensitive(inner, "scale_degrees") != NULL &&
            cJSON_GetObjectItemCaseSensitive(inner, "implied_harmony") != NULL,
        "the analysis has scale degrees and implied harmony");
  cJSON_Delete(answer);

  char request[2048];
  snprintf(request, sizeof(request), "{\"note\":\"again\",\"body\":%s}", kMotifBody);
  char versions[300];
  snprintf(versions, sizeof(versions), "%s/versions", path);
  answer = json_exchange("POST", versions, request, &status);
  check(status == 200 && ok_true(answer) && !cJSON_GetObjectItemCaseSensitive(answer, "analysis_error"),
        "a new motif version is saved with its analysis");
  cJSON_Delete(answer);
  free(motif);
}

/* The id of the first project (the default one). */
static void first_project(char *out, size_t out_size) {
  int status = 0;
  cJSON *answer = json_exchange("GET", "/api/materials/projects", NULL, &status);
  snprintf(out, out_size, "%s",
           string_of(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(answer, "projects"), 0), "id"));
  cJSON_Delete(answer);
}

/* Saves a motif of the given form; *status gets the HTTP status and the answer is the parsed body (caller deletes). */
static cJSON *save_motif_form(const char *project, const char *form, const char *notes, const char *key_json,
                              const char *pickup, int *status) {
  char request[2048];
  snprintf(request, sizeof(request),
           "{\"type\":\"motif\",\"name\":\"Form\",\"project_id\":\"%s\",\"body\":{\"uses\":[],\"motif\":{\"role\":\"melodic\","
           "\"form\":\"%s\",\"notes\":\"%s\",\"meter\":\"4/4\",\"key\":%s,\"pickup\":\"%s\",\"features\":[],"
           "\"analysis\":null,\"derived\":null}}}",
           project, form, notes, key_json, pickup);
  return json_exchange("POST", "/api/materials/items", request, status);
}

/* The saved motif object of an item answer, read back. */
static cJSON *read_motif(const cJSON *saved) {
  char path[256];
  int status = 0;
  snprintf(path, sizeof(path), "/api/materials/items/%s", string_of(cJSON_GetObjectItemCaseSensitive(saved, "item"), "id"));
  cJSON *answer = json_exchange("GET", path, NULL, &status);
  cJSON *motif = cJSON_DetachItemFromObjectCaseSensitive(cJSON_GetObjectItemCaseSensitive(answer, "body"), "motif");
  cJSON_Delete(answer);
  return motif;
}

/* Rhythm and contour motifs are analyzed too, and every form stores its list as motif.degrees. */
static void test_motif_forms(void) {
  char project[64] = "";
  first_project(project, sizeof(project));
  int status = 0;

  cJSON *saved = save_motif_form(project, "rhythm", "x:q x:e x:e x:h", "null", "0", &status);
  check(status == 200 && ok_true(saved), "a rhythm-only motif without a key is saved");
  cJSON *motif = read_motif(saved);
  check(strcmp(string_of(motif, "degrees"), "x:q x:e x:e x:h") == 0, "a rhythm motif stores its rhythm list as degrees");
  check(cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(motif, "analysis")), "a rhythm motif carries the analysis");
  cJSON_Delete(motif);
  cJSON_Delete(saved);

  saved = save_motif_form(project, "contour", "0 2 1 3", "null", "0", &status);
  check(status == 200 && ok_true(saved), "a contour-only motif is saved");
  motif = read_motif(saved);
  check(strcmp(string_of(motif, "degrees"), "0 2 1 3") == 0, "a contour motif stores its contour list as degrees");
  cJSON_Delete(motif);
  cJSON_Delete(saved);

  saved = save_motif_form(project, "full", "D4:q F4:e A4:e D5:h", "\"D minor\"", "0", &status);
  check(status == 200 && ok_true(saved), "a full motif with a key is saved");
  motif = read_motif(saved);
  check(strcmp(string_of(motif, "degrees"), "1:q 3:e 5:e 1':h") == 0, "a full motif stores its degree list");
  cJSON_Delete(motif);
  cJSON_Delete(saved);

  saved = save_motif_form(project, "rhythm", "x:q x:q x:h", "null", "1", &status);
  check(status == 200 && ok_true(saved), "a motif with a pickup is saved");
  motif = read_motif(saved);
  const cJSON *inner = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(motif, "analysis"), "analysis");
  check(strcmp(string_of(inner, "pickup"), "1") == 0, "the pickup reaches the Composer");
  cJSON_Delete(motif);
  cJSON_Delete(saved);

  saved = save_motif_form(project, "full", "D4:q F4:e A4:e D5:h", "null", "0", &status);
  check(status == 400 && strstr(string_of(saved, "error"), "give a key") != NULL, "a full motif without a key is 400");
  cJSON_Delete(saved);

  saved = save_motif_form(project, "rhythm", "x:q x:q x:h", "null", "1;rm", &status);
  check(status == 400, "a pickup that is not beats is 400");
  cJSON_Delete(saved);
}

/* A format 0 MIDI file of one track holding the given events (the track length is filled in). */
static size_t midi_file(unsigned char *out, const unsigned char *events, size_t event_count) {
  static const unsigned char header[] = {'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, 0x01, 0xE0, 'M', 'T', 'r', 'k'};
  memcpy(out, header, sizeof(header));
  size_t at = sizeof(header);
  out[at++] = 0;
  out[at++] = 0;
  out[at++] = 0;
  out[at++] = (unsigned char)event_count;
  memcpy(out + at, events, event_count);
  return at + event_count;
}

static cJSON *import_exchange(const unsigned char *file, size_t file_size, const char *query, int *status) {
  char path[128];
  snprintf(path, sizeof(path), "/api/materials/motifs/import%s", query);
  char *response = http_exchange("POST", path, "Content-Type: audio/midi\r\n", file, file_size);
  *status = response && strncmp(response, "HTTP/1.1 ", 9) == 0 ? atoi(response + 9) : 0;
  cJSON *json = response ? cJSON_Parse(response_body(response)) : NULL;
  free(response);
  return json;
}

/* A MIDI file is read into a motif without saving anything. */
static void test_motif_import(void) {
  /* D4 then F4, a quarter note each. */
  static const unsigned char one_voice[] = {0x00, 0x90, 0x3E, 0x64, 0x83, 0x60, 0x80, 0x3E, 0x00, 0x00, 0x90,
                                            0x41, 0x64, 0x83, 0x60, 0x80, 0x41, 0x00, 0x00, 0xFF, 0x2F, 0x00};
  /* D4 and F4 sounding together. */
  static const unsigned char two_voices[] = {0x00, 0x90, 0x3E, 0x64, 0x00, 0x90, 0x41, 0x64, 0x83, 0x60, 0x80,
                                             0x3E, 0x00, 0x00, 0x80, 0x41, 0x00, 0x00, 0xFF, 0x2F, 0x00};
  unsigned char file[128];
  int status = 0;

  size_t size = midi_file(file, one_voice, sizeof(one_voice));
  cJSON *answer = import_exchange(file, size, "?key=D%20minor", &status);
  check(status == 200 && ok_true(answer), "a one-voice MIDI file is imported");
  check(strcmp(string_of(answer, "notes_text"), "D4:q F4:q") == 0, "the import answers the notes as a note list");
  check(strcmp(string_of(answer, "meter"), "4/4") == 0, "the import answers the meter");
  check(strcmp(string_of(answer, "key"), "D minor") == 0, "the import answers the given key");
  check(strcmp(string_of(answer, "pickup"), "0") == 0, "the import answers the pickup");
  check(cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(answer, "analysis")), "the import answers the analysis");
  cJSON_Delete(answer);

  answer = import_exchange(file, size, "", &status);
  check(status == 200 && cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(answer, "key")), "a file without a key answers null");
  cJSON_Delete(answer);

  size = midi_file(file, two_voices, sizeof(two_voices));
  answer = import_exchange(file, size, "", &status);
  check(status == 400 && strlen(string_of(answer, "error")) > 0, "a polyphonic MIDI file is 400 with the Composer's message");
  cJSON_Delete(answer);

  answer = import_exchange(file, 0, "", &status);
  check(status == 400, "an empty body is 400");
  cJSON_Delete(answer);

  answer = import_exchange(file, size, "?meter=nope", &status);
  check(status == 400, "a bad meter is 400");
  cJSON_Delete(answer);

  answer = json_exchange("GET", "/api/materials/motifs/import", NULL, &status);
  check(status == 405, "importing is POST only");
  cJSON_Delete(answer);
}

/* A note list the Composer cannot read is refused and no version is written. */
static void test_motif_refused(void) {
  int status = 0;
  char project[64] = "";
  cJSON *answer = json_exchange("GET", "/api/materials/projects", NULL, &status);
  snprintf(project, sizeof(project), "%s",
           string_of(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(answer, "projects"), 0), "id"));
  cJSON_Delete(answer);

  char *motif = create_item("motif", "Refused", project, kMotifBody);
  check(motif != NULL, "a motif is saved before a bad version");
  if (!motif) {
    return;
  }
  char path[256];
  char versions[300];
  snprintf(path, sizeof(path), "/api/materials/items/%s", motif);
  snprintf(versions, sizeof(versions), "%s/versions", path);

  char bad_notes[2048];
  snprintf(bad_notes, sizeof(bad_notes), "{\"body\":%s}", kBadMotifBody);
  answer = json_exchange("POST", versions, bad_notes, &status);
  check(status == 400 && !ok_true(answer) && strlen(string_of(answer, "error")) > 0, "a bad note list is 400 with the message");
  cJSON_Delete(answer);

  answer = json_exchange("GET", path, NULL, &status);
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(answer, "item");
  check(status == 200 && cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(item, "versions")) == 1 &&
            number_of(item, "current_version") == 1,
        "the version count is unchanged");
  cJSON_Delete(answer);

  char request[2048];
  snprintf(request, sizeof(request), "{\"type\":\"motif\",\"name\":\"Bad\",\"project_id\":\"%s\",\"body\":%s}", project,
           kBadMotifBody);
  answer = json_exchange("POST", "/api/materials/items", request, &status);
  check(status == 400, "a new motif with a bad note list is 400");
  cJSON_Delete(answer);
  free(motif);
}

/* The first note of a playback answer: its pitch, and its position as num/den. */
static bool first_note(const cJSON *playback, int *pitch, int *on_num, int *on_den) {
  const cJSON *note = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(playback, "notes"), 0);
  const cJSON *on = cJSON_GetObjectItemCaseSensitive(note, "on");
  if (!note || !on) {
    return false;
  }
  *pitch = cJSON_GetObjectItemCaseSensitive(note, "pitch")->valueint;
  *on_num = cJSON_GetArrayItem(on, 0)->valueint;
  *on_den = cJSON_GetArrayItem(on, 1)->valueint;
  return true;
}

static cJSON *render_exchange(const char *key, const char *render_key, const char *pickup, int *status) {
  char request[512];
  snprintf(request, sizeof(request),
           "{\"form\":\"full\",\"degrees\":\"1:q 3:e 5:e 1':h\",\"meter\":\"4/4\",\"key\":\"%s\",\"pickup\":\"%s\","
           "\"render_key\":\"%s\",\"role\":\"melodic\"}",
           key, pickup, render_key);
  return json_exchange("POST", "/api/materials/render", request, status);
}

/* A motif renders to MusicXML and playback data, in any key, from a request or from a stored version. */
static void test_motif_render(void) {
  int status = 0;
  int pitch = 0;
  int on_num = -1;
  int on_den = 0;

  cJSON *answer = render_exchange("D minor", "D minor", "0", &status);
  check(status == 200 && ok_true(answer), "a degree list renders");
  check(strstr(string_of(answer, "musicxml"), "<score-partwise") != NULL, "the render answers MusicXML");
  const cJSON *playback = cJSON_GetObjectItemCaseSensitive(answer, "playback");
  check(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(playback, "notes")) == 4, "the playback holds the four notes");
  check(strcmp(string_of(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(playback, "parts"), 0), "name"), "melodic") == 0,
        "the playback has one part named after the role");
  check(first_note(playback, &pitch, &on_num, &on_den) && pitch == 62, "the first note sounds the tonic");
  cJSON_Delete(answer);

  answer = render_exchange("D minor", "D minor", "1", &status);
  playback = cJSON_GetObjectItemCaseSensitive(answer, "playback");
  check(status == 200 && first_note(playback, &pitch, &on_num, &on_den) && on_num == 0 && on_den == 1,
        "the first note is at the pickup start");
  cJSON_Delete(answer);

  answer = render_exchange("D minor", "G minor", "0", &status);
  playback = cJSON_GetObjectItemCaseSensitive(answer, "playback");
  check(status == 200 && first_note(playback, &pitch, &on_num, &on_den) && pitch == 67,
        "another render key moves the pitches");
  cJSON_Delete(answer);

  answer = render_exchange("D minor", "H major", "0", &status);
  check(status == 400, "a render key that is not a key is 400");
  cJSON_Delete(answer);

  char project[64] = "";
  first_project(project, sizeof(project));
  cJSON *saved = save_motif_form(project, "full", "D4:q F4:e A4:e D5:h", "\"D minor\"", "0", &status);
  const char *id = string_of(cJSON_GetObjectItemCaseSensitive(saved, "item"), "id");
  char path[256];
  snprintf(path, sizeof(path), "/api/materials/items/%s/versions/1/playback", id);
  answer = json_exchange("GET", path, NULL, &status);
  check(status == 200 && first_note(answer, &pitch, &on_num, &on_den) && pitch == 62,
        "a version sounds in its own key by default");
  cJSON_Delete(answer);
  snprintf(path, sizeof(path), "/api/materials/items/%s/versions/1/playback?key=G%%20minor", id);
  answer = json_exchange("GET", path, NULL, &status);
  check(status == 200 && first_note(answer, &pitch, &on_num, &on_den) && pitch == 67,
        "a version sounds in the chosen key");
  cJSON_Delete(answer);
  snprintf(path, sizeof(path), "/api/materials/items/%s/versions/1/score?key=G%%20minor", id);
  char *response = http_exchange("GET", path, NULL, NULL, 0);
  check(response && strncmp(response, "HTTP/1.1 200", 12) == 0 && strstr(response, "<score-partwise") != NULL,
        "a version's score is MusicXML");
  free(response);
  snprintf(path, sizeof(path), "/api/materials/items/%s/versions/1/score?key=nope", id);
  response = http_exchange("GET", path, NULL, NULL, 0);
  check(response && strncmp(response, "HTTP/1.1 400", 12) == 0, "a bad key on a version's score is 400");
  free(response);
  snprintf(path, sizeof(path), "/api/materials/items/%s/versions/9/score", id);
  response = http_exchange("GET", path, NULL, NULL, 0);
  check(response && strncmp(response, "HTTP/1.1 404", 12) == 0, "a missing version's score is 404");
  free(response);
  cJSON_Delete(saved);
}

static void test_body_limit(void) {
  size_t size = 300 * 1024;
  char *big = malloc(size + 64);
  int status = 0;
  if (!big) {
    check(false, "allocate the oversized body");
    return;
  }
  size_t length = (size_t)snprintf(big, 64, "{\"name\":\"");
  memset(big + length, 'a', size);
  length += size;
  length += (size_t)snprintf(big + length, 16, "\"}");
  cJSON *answer = json_exchange("POST", "/api/materials/projects", big, &status);
  check(status == 413, "a body over 256 KB is 413");
  cJSON_Delete(answer);
  free(big);
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

  struct mg_context *ctx = ursatz_gui_server_start("18770", ".");
  check(ctx != NULL, "server starts");
  if (!ctx) {
    return 1;
  }
  sleep_ms(200);

  test_projects();
  test_items();
  test_motif_analysis();
  test_motif_refused();
  test_motif_forms();
  test_motif_import();
  test_motif_render();
  test_body_limit();

  ursatz_gui_server_stop(ctx);

  if (failures > 0) {
    fprintf(stderr, "%d check(s) failed\n", failures);
    return 1;
  }
  return 0;
}
