/* Material and subject routes end to end: projects, items, versions and uses
 * under /api/materials, with the status codes the routes promise (400, 404,
 * 409, 413). Uses a plain TCP socket for the HTTP requests, same as
 * test_styles_route.c. Runs in its own working directory (materials/ and
 * subjects/ are created there). */


#include <civetweb.h>
#include <math.h>
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
    "{\"uses\":[],\"motif\":{\"role\":\"melodic\",\"form\":\"full\",\"notes\":\"C4:q Eb4:e G4:e C5:h\","
    "\"meter\":\"4/4\",\"key\":\"C minor\",\"pickup\":\"0\",\"features\":[{\"label\":\"a\",\"first\":0,\"last\":2}],"
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

/* The note of version number (1-based) of an item, "" when it has none. */
static void version_note(const char *item_id, int number, char *out, size_t out_size) {
  char path[256];
  int status = 0;
  snprintf(path, sizeof(path), "/api/materials/items/%s", item_id);
  cJSON *answer = json_exchange("GET", path, NULL, &status);
  const cJSON *versions = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(answer, "item"), "versions");
  snprintf(out, out_size, "%s", string_of(cJSON_GetArrayItem(versions, number - 1), "note"));
  cJSON_Delete(answer);
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
#ifndef _WIN32
static bool write_stand_in(const char *name, const char *text, char *path, size_t size) {
  char dir[512];
  if (!getcwd(dir, sizeof(dir))) {
    return false;
  }
  snprintf(path, size, "%s/%s", dir, name);
  FILE *file = fopen(path, "wb");
  if (!file) {
    return false;
  }
  bool ok = fwrite(text, 1, strlen(text), file) == strlen(text);
  ok = (fclose(file) == 0) && ok;
  return ok && chmod(path, 0755) == 0;
}

static const cJSON *saved_motif(const char *item_id, int version) {
  static cJSON *held = NULL;
  cJSON_Delete(held);
  char path[300];
  int status = 0;
  snprintf(path, sizeof(path), "/api/materials/items/%s/versions/%d", item_id, version);
  held = json_exchange("GET", path, NULL, &status);
  return cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(held, "body"), "motif");
}

static int saved_version_count(const char *item_id) {
  char path[300];
  int status = 0;
  snprintf(path, sizeof(path), "/api/materials/items/%s", item_id);
  cJSON *answer = json_exchange("GET", path, NULL, &status);
  int count = cJSON_GetArraySize(
      cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(answer, "item"), "versions"));
  cJSON_Delete(answer);
  return count;
}

/* The reason a motif was not analyzed is stored with its version, and Re-analyze writes a new version. */
static void test_motif_analysis_error(void) {
  int status = 0;
  char project[64] = "";
  cJSON *answer = json_exchange("GET", "/api/materials/projects", NULL, &status);
  snprintf(project, sizeof(project), "%s",
           string_of(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(answer, "projects"), 0), "id"));
  cJSON_Delete(answer);

  char *original = getenv("URSATZ_GUI_COMPOSER") ? strdup(getenv("URSATZ_GUI_COMPOSER")) : NULL;
  char crash[600];
  char noisy[600];
  char refuse[600];
  bool written = write_stand_in("crash-composer.sh", "#!/bin/sh\nkill -SEGV $$\n", crash, sizeof(crash)) &&
                 write_stand_in("noisy-composer.sh",
                                "#!/bin/sh\necho first line >&2\necho \"the note list is odd\" >&2\nexit 3\n", noisy,
                                sizeof(noisy)) &&
                 write_stand_in("refusing-composer.sh", "#!/bin/sh\necho \"no such note\" >&2\nexit 1\n", refuse,
                                sizeof(refuse));
  check(written, "stand-in Composers are written");

  /* Composer missing: saved without analysis, the reason kept with the version. */
  setenv("URSATZ_GUI_COMPOSER", "/nonexistent/ursatz-composer", 1);
  char *missing = create_item("motif", "NoBinary", project, kMotifBody);
  check(missing != NULL, "a motif is saved when the Composer is missing");
  if (!missing) {
    return;
  }
  const cJSON *motif = saved_motif(missing, 1);
  check(strcmp(string_of(motif, "analysis_error"), "composer not found") == 0 &&
            cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(motif, "analysis")),
        "the version stores analysis_error composer not found");

  /* A crashing Composer names the signal. */
  setenv("URSATZ_GUI_COMPOSER", crash, 1);
  char *crashed = create_item("motif", "Crashed", project, kMotifBody);
  check(crashed != NULL && strcmp(string_of(saved_motif(crashed, 1), "analysis_error"), "composer crashed (signal 11)") == 0,
        "a crash is stored with its signal");

  /* Any other failure keeps the last non-empty stderr line. */
  setenv("URSATZ_GUI_COMPOSER", noisy, 1);
  char *noisy_id = create_item("motif", "Noisy", project, kMotifBody);
  check(noisy_id != NULL &&
            strcmp(string_of(saved_motif(noisy_id, 1), "analysis_error"),
                   "composer could not analyze the motif: the note list is odd") == 0,
        "the last stderr line is stored");

  /* Re-analyze with a working Composer writes a new version and clears the reason. */
  if (original) {
    setenv("URSATZ_GUI_COMPOSER", original, 1);
  } else {
    unsetenv("URSATZ_GUI_COMPOSER");
  }
  char path[300];
  snprintf(path, sizeof(path), "/api/materials/items/%s/reanalyze", missing);
  answer = json_exchange("POST", path, "", &status);
  check(status == 200 && ok_true(answer), "re-analyze answers 200");
  cJSON_Delete(answer);
  check(saved_version_count(missing) == 2, "re-analyze writes a new version");
  motif = saved_motif(missing, 2);
  check(cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(motif, "analysis")) &&
            cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(motif, "analysis_error")),
        "the new version is analyzed with no stored reason");
  snprintf(path, sizeof(path), "/api/materials/items/%s", missing);
  answer = json_exchange("GET", path, NULL, &status);
  const cJSON *versions = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(answer, "item"), "versions");
  check(strcmp(string_of(cJSON_GetArrayItem(versions, 1), "note"), "Re-analyzed") == 0, "the new version's note is Re-analyzed");
  cJSON_Delete(answer);
  check(cJSON_IsString(cJSON_GetObjectItemCaseSensitive(saved_motif(missing, 1), "analysis_error")),
        "the first version keeps its reason");

  /* A refusal is 400 and writes nothing. */
  setenv("URSATZ_GUI_COMPOSER", refuse, 1);
  snprintf(path, sizeof(path), "/api/materials/items/%s/reanalyze", missing);
  answer = json_exchange("POST", path, "", &status);
  check(status == 400 && strstr(string_of(answer, "error"), "no such note") != NULL, "a refused re-analyze is 400 with the message");
  cJSON_Delete(answer);
  check(saved_version_count(missing) == 2, "a refused re-analyze writes no version");
  if (original) {
    setenv("URSATZ_GUI_COMPOSER", original, 1);
  } else {
    unsetenv("URSATZ_GUI_COMPOSER");
  }

  /* Only motifs are analyzed. */
  char *theme = create_item("theme", "NotAMotif", project, kThemeBody);
  check(theme != NULL, "a theme is saved");
  if (theme) {
    snprintf(path, sizeof(path), "/api/materials/items/%s/reanalyze", theme);
    answer = json_exchange("POST", path, "", &status);
    check(status == 400 && strcmp(string_of(answer, "error"), "only motifs are analyzed") == 0, "re-analyze of a theme is 400");
    cJSON_Delete(answer);
  }
  snprintf(path, sizeof(path), "/api/materials/items/mot-0000000000000000/reanalyze");
  answer = json_exchange("POST", path, "", &status);
  check(status == 404, "re-analyze of an unknown item is 404");
  cJSON_Delete(answer);
  free(missing);
  free(crashed);
  free(noisy_id);
  free(theme);
  free(original);
}
#endif

static void test_motif_forms(void) {
  char project[64] = "";
  first_project(project, sizeof(project));
  int status = 0;

  cJSON *saved = save_motif_form(project, "rhythm", "x:q x:e x:e x:h", "\"C major\"", "0", &status);
  check(status == 200 && ok_true(saved), "a rhythm-only motif in C is saved");
  cJSON *motif = read_motif(saved);
  check(strcmp(string_of(motif, "degrees"), "x:q x:e x:e x:h") == 0, "a rhythm motif stores its rhythm list as degrees");
  check(cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(motif, "analysis")), "a rhythm motif carries the analysis");
  cJSON_Delete(motif);
  cJSON_Delete(saved);

  saved = save_motif_form(project, "contour", "0 2 1 3", "\"C major\"", "0", &status);
  check(status == 200 && ok_true(saved), "a contour-only motif is saved");
  motif = read_motif(saved);
  check(strcmp(string_of(motif, "degrees"), "0 2 1 3") == 0, "a contour motif stores its contour list as degrees");
  cJSON_Delete(motif);
  cJSON_Delete(saved);

  saved = save_motif_form(project, "full", "C4:q Eb4:e G4:e C5:h", "\"C minor\"", "0", &status);
  check(status == 200 && ok_true(saved), "a full motif with a key is saved");
  motif = read_motif(saved);
  check(strcmp(string_of(motif, "degrees"), "1:q 3:e 5:e 1':h") == 0, "a full motif stores its degree list");
  cJSON_Delete(motif);
  cJSON_Delete(saved);

  saved = save_motif_form(project, "rhythm", "x:q x:q x:h", "\"C major\"", "1", &status);
  check(status == 200 && ok_true(saved), "a motif with a pickup is saved");
  motif = read_motif(saved);
  const cJSON *inner = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(motif, "analysis"), "analysis");
  check(strcmp(string_of(inner, "pickup"), "1") == 0, "the pickup reaches the Composer");
  cJSON_Delete(motif);
  cJSON_Delete(saved);

  saved = save_motif_form(project, "full", "C4:q Eb4:e G4:e C5:h", "null", "0", &status);
  check(status == 400 && strstr(string_of(saved, "error"), "C as the tonic") != NULL, "a full motif without a key is 400");
  cJSON_Delete(saved);

  saved = save_motif_form(project, "rhythm", "x:q x:q x:h", "\"C major\"", "1;rm", &status);
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
  check(strcmp(string_of(answer, "notes_text"), "C4:q D#4:q") == 0, "the import answers the notes moved to C");
  check(strcmp(string_of(answer, "meter"), "4/4") == 0, "the import answers the meter");
  check(strcmp(string_of(answer, "key"), "C minor") == 0, "the import answers the key in C");
  check(strcmp(string_of(answer, "moved_from"), "D minor") == 0, "the import answers the key it moved from");
  check(strcmp(string_of(answer, "pickup"), "0") == 0, "the import answers the pickup");
  check(cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(answer, "analysis")), "the import answers the analysis");
  cJSON_Delete(answer);

  answer = import_exchange(file, size, "", &status);
  check(status == 200 && strcmp(string_of(answer, "key"), "C major") == 0 &&
            cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(answer, "moved_from")) &&
            strcmp(string_of(answer, "notes_text"), "D4:q F4:q") == 0,
        "a file without a key is read as C major with the notes unchanged");
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

/* Motifs are written with C as the tonic (R-179): other keys are refused, and the to-c route writes degrees in C. */
static void test_motifs_in_c(void) {
  char project[64] = "";
  first_project(project, sizeof(project));
  int status = 0;

  cJSON *saved = save_motif_form(project, "full", "D4:q F4:e A4:e D5:h", "\"D minor\"", "0", &status);
  check(status == 400 && strstr(string_of(saved, "error"), "motifs are written with C as the tonic") != NULL,
        "a motif in D minor is 400");
  cJSON_Delete(saved);

  saved = save_motif_form(project, "rhythm", "x:q x:q x:h", "\"G major\"", "0", &status);
  check(status == 400, "a rhythm motif in G major is 400");
  cJSON_Delete(saved);

  saved = save_motif_form(project, "rhythm", "x:q x:q x:h", "\"C major\"", "0", &status);
  check(status == 200 && ok_true(saved), "a motif in C major is saved");
  char path[256];
  snprintf(path, sizeof(path), "/api/materials/items/%s/versions", string_of(cJSON_GetObjectItemCaseSensitive(saved, "item"), "id"));
  cJSON *answer = json_exchange("POST", path,
                                "{\"note\":\"E minor\",\"body\":{\"uses\":[],\"motif\":{\"role\":\"melodic\",\"form\":\"rhythm\",\"notes\":\"x:q x:q x:h\","
                                "\"meter\":\"4/4\",\"key\":\"E minor\",\"pickup\":\"0\",\"features\":[],\"analysis\":null,"
                                "\"derived\":null}}}",
                                &status);
  check(status == 400 && strstr(string_of(answer, "error"), "C as the tonic") != NULL, "a new version in E minor is 400");
  cJSON_Delete(answer);
  cJSON_Delete(saved);

  answer = json_exchange("POST", "/api/materials/motifs/to-c",
                         "{\"degrees\":\"1:q 3:e 5:e 1':h\",\"mode\":\"minor\",\"meter\":\"4/4\",\"pickup\":\"0\"}", &status);
  check(status == 200 && ok_true(answer) && strcmp(string_of(answer, "notes_text"), "C4:q D#4:e G4:e C5:h") == 0,
        "to-c writes minor degrees in C minor");
  cJSON_Delete(answer);

  answer = json_exchange("POST", "/api/materials/motifs/to-c",
                         "{\"degrees\":\"1:q 3:e 5:e 1':h\",\"mode\":\"major\",\"meter\":\"4/4\"}", &status);
  check(status == 200 && strcmp(string_of(answer, "notes_text"), "C4:q E4:e G4:e C5:h") == 0, "to-c writes major degrees in C major");
  cJSON_Delete(answer);

  answer = json_exchange("POST", "/api/materials/motifs/to-c",
                         "{\"degrees\":\"1:q\",\"mode\":\"dorian\",\"meter\":\"4/4\"}", &status);
  check(status == 400, "to-c refuses a mode that is not major or minor");
  cJSON_Delete(answer);

  answer = json_exchange("POST", "/api/materials/motifs/to-c", "{\"degrees\":\"\",\"mode\":\"major\",\"meter\":\"4/4\"}", &status);
  check(status == 400, "to-c refuses an empty degree list");
  cJSON_Delete(answer);

  answer = json_exchange("POST", "/api/materials/motifs/to-c", "{\"degrees\":\"1:q\",\"mode\":\"major\",\"meter\":\"four\"}", &status);
  check(status == 400, "to-c refuses a bad meter");
  cJSON_Delete(answer);

  answer = json_exchange("GET", "/api/materials/motifs/to-c", NULL, &status);
  check(status == 405, "to-c is POST only");
  cJSON_Delete(answer);
}

/* A new version of a motif says what changed (P18 B3); a new motif's first version is "Created". */
static void test_version_notes(void) {
  char project[64] = "";
  first_project(project, sizeof(project));
  int status = 0;
  char *motif = create_item("motif", "Noted", project, kMotifBody);
  check(motif != NULL, "a motif is saved without a note");
  if (!motif) {
    return;
  }
  char note[600];
  version_note(motif, 1, note, sizeof(note));
  check(strcmp(note, "Created") == 0, "a new motif's first version is noted Created");

  char path[256];
  snprintf(path, sizeof(path), "/api/materials/items/%s/versions", motif);
  char request[2048];
  snprintf(request, sizeof(request), "{\"body\":%s}", kMotifBody);
  cJSON *answer = json_exchange("POST", path, request, &status);
  check(status == 400 && strcmp(string_of(answer, "error"), "give a version note") == 0, "a motif version without a note is 400");
  cJSON_Delete(answer);
  snprintf(request, sizeof(request), "{\"note\":\"   \",\"body\":%s}", kMotifBody);
  answer = json_exchange("POST", path, request, &status);
  check(status == 400 && strcmp(string_of(answer, "error"), "give a version note") == 0, "a blank note is 400");
  cJSON_Delete(answer);
  check(saved_version_count(motif) == 1, "no version was written without a note");

  snprintf(request, sizeof(request), "{\"note\":\"Shorter\",\"body\":%s}", kMotifBody);
  answer = json_exchange("POST", path, request, &status);
  check(status == 200 && ok_true(answer), "a motif version with a note is saved");
  cJSON_Delete(answer);
  version_note(motif, 2, note, sizeof(note));
  check(strcmp(note, "Shorter") == 0, "the note is kept");

  /* A caller's own note on create is kept, and the other item types need no note. */
  snprintf(request, sizeof(request), "{\"type\":\"motif\",\"name\":\"Own\",\"project_id\":\"%s\",\"note\":\"From MIDI\",\"body\":%s}",
           project, kMotifBody);
  answer = json_exchange("POST", "/api/materials/items", request, &status);
  const char *own = string_of(cJSON_GetObjectItemCaseSensitive(answer, "item"), "id");
  version_note(own, 1, note, sizeof(note));
  check(status == 200 && strcmp(note, "From MIDI") == 0, "a note given on create is kept");
  cJSON_Delete(answer);
  free(motif);
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
  snprintf(bad_notes, sizeof(bad_notes), "{\"note\":\"bad\",\"body\":%s}", kBadMotifBody);
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
  cJSON *saved = save_motif_form(project, "full", "C4:q Eb4:e G4:e C5:h", "\"C minor\"", "0", &status);
  const char *id = string_of(cJSON_GetObjectItemCaseSensitive(saved, "item"), "id");
  char path[256];
  snprintf(path, sizeof(path), "/api/materials/items/%s/versions/1/playback", id);
  answer = json_exchange("GET", path, NULL, &status);
  check(status == 200 && first_note(answer, &pitch, &on_num, &on_den) && pitch == 60,
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

/* The source motif of the variant tests: saved and analyzed in D minor, so it has degrees. */
static char *variant_source(void) {
  char project[64] = "";
  first_project(project, sizeof(project));
  int status = 0;
  cJSON *saved = save_motif_form(project, "full", "C4:q Eb4:e G4:e C5:h", "\"C minor\"", "0", &status);
  char *id = status == 200 ? strdup(string_of(cJSON_GetObjectItemCaseSensitive(saved, "item"), "id")) : NULL;
  cJSON_Delete(saved);
  return id;
}

/* POST /api/materials/items/<id>/<tail> with a JSON body. */
static cJSON *item_post(const char *id, const char *tail, const char *body, int *status) {
  char path[256];
  snprintf(path, sizeof(path), "/api/materials/items/%s/%s", id, tail);
  return json_exchange("POST", path, body, status);
}

static cJSON *item_get(const char *id, const char *tail, int *status) {
  char path[256];
  snprintf(path, sizeof(path), "/api/materials/items/%s%s", id, tail);
  return json_exchange("GET", path, NULL, status);
}

/* The degrees text of the i-th candidate of a variants answer. */
static const char *candidate_degrees(const cJSON *answer, int i) {
  const cJSON *variant = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(answer, "variants"), i);
  return string_of(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(variant, "motive"), "analysis"),
                   "degrees_text");
}

/* A motif with its own harmony and features: stored, refused when the Composer cannot read it, passed to variants. */
static void test_harmony(void) {
  char project[64] = "";
  first_project(project, sizeof(project));
  int status = 0;
  char request[2048];
  const char *shape = "{\"type\":\"motif\",\"name\":\"Own chords\",\"project_id\":\"%s\",\"body\":{\"uses\":[],\"motif\":{"
                      "\"role\":\"melodic\",\"form\":\"full\",\"notes\":\"C4:q Eb4:e G4:e C5:h\",\"meter\":\"4/4\","
                      "\"key\":\"C minor\",\"pickup\":\"0\",\"harmony\":%s,"
                      "\"features\":[{\"label\":\"a\",\"first\":0,\"last\":1}],\"analysis\":null,\"derived\":null}}}";
  snprintf(request, sizeof(request), shape, project, "\"I V@2\"");
  cJSON *saved = json_exchange("POST", "/api/materials/items", request, &status);
  check(status == 200 && ok_true(saved), "a motif with its own harmony is saved");
  cJSON *motif = status == 200 ? read_motif(saved) : NULL;
  check(strcmp(string_of(motif, "harmony"), "I V@2") == 0, "the harmony is stored with the motif");
  const cJSON *analysis = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(motif, "analysis"), "analysis");
  check(strcmp(string_of(analysis, "harmony_text"), "I V@2") == 0, "the Composer analyzed the motif with its harmony");
  cJSON_Delete(motif);
  char id[64];
  snprintf(id, sizeof(id), "%s", string_of(cJSON_GetObjectItemCaseSensitive(saved, "item"), "id"));
  cJSON_Delete(saved);

  snprintf(request, sizeof(request), shape, project, "\"not a chord\"");
  saved = json_exchange("POST", "/api/materials/items", request, &status);
  check(status == 400, "harmony the Composer cannot read refuses the save");
  cJSON_Delete(saved);
  snprintf(request, sizeof(request), shape, project, "5");
  saved = json_exchange("POST", "/api/materials/items", request, &status);
  check(status == 400, "harmony must be text");
  cJSON_Delete(saved);

  cJSON *answer = item_post(id, "variants", "{\"chain\":{\"steps\":[{\"op\":\"transpose\"}]},\"count\":1,\"seed\":3}", &status);
  check(status == 200 && ok_true(answer), "variants take the motif's harmony and features");
  const cJSON *variant = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(answer, "variants"), 0);
  check(cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(variant, "remoteness")) &&
            cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(variant, "functions")),
        "a candidate has its remoteness and functions");
  cJSON *save = cJSON_CreateObject();
  cJSON_AddItemToObject(save, "variant", cJSON_Duplicate(variant, 1));
  char *save_text = cJSON_PrintUnformatted(save);
  cJSON *form = item_post(id, "variants/save", save_text, &status);
  free(save_text);
  cJSON_Delete(save);
  check(status == 200 && ok_true(form), "the form with chords is saved");
  motif = read_motif(form);
  const cJSON *derived = cJSON_GetObjectItemCaseSensitive(motif, "derived");
  check(strcmp(string_of(motif, "harmony"), string_of(variant, "harmony")) == 0 &&
            strcmp(string_of(derived, "harmony"), string_of(variant, "harmony")) == 0,
        "the saved form keeps the variant's own harmony on the motif and in the record");
  check(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(motif, "features")) == 1 &&
            fabs(number_of(derived, "remoteness") - number_of(variant, "remoteness")) < 1e-9,
        "the saved form keeps its feature and its remoteness");
  cJSON_Delete(motif);
  char form_id[64];
  snprintf(form_id, sizeof(form_id), "%s", string_of(cJSON_GetObjectItemCaseSensitive(form, "item"), "id"));
  cJSON_Delete(form);
  cJSON_Delete(answer);

  /* A derived source measures remoteness from the basic motive, the root of the tree. */
  answer = item_post(form_id, "variants", "{\"chain\":{\"steps\":[{\"op\":\"transpose\"}]},\"count\":1,\"seed\":4}", &status);
  check(status == 200 && cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(
                             cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(answer, "variants"), 0), "remoteness")),
        "a form of a form has its remoteness from the basic motive");
  cJSON_Delete(answer);

  answer = item_post(id, "variants",
                     "{\"functions\":[\"beginning\",\"continuation\",\"contrast\",\"cadential\"],\"count\":3,\"seed\":1}", &status);
  bool all_suit = status == 200;
  const cJSON *candidate = NULL;
  cJSON_ArrayForEach(candidate, cJSON_GetObjectItemCaseSensitive(answer, "variants")) {
    all_suit = all_suit && cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(candidate, "functions")) > 0;
  }
  check(all_suit, "a function filter keeps only the forms that suit one");
  cJSON_Delete(answer);
  answer = item_post(id, "variants", "{\"functions\":[\"finale\"]}", &status);
  check(status == 400, "an unknown function is refused");
  cJSON_Delete(answer);
  answer = item_post(id, "variants", "{\"functions\":\"contrast\"}", &status);
  check(status == 400, "functions must be a list");
  cJSON_Delete(answer);
}

/* Variants are listed, generated without saving, and saved as derived motifs. */
static void test_variants(void) {
  int status = 0;
  cJSON *answer = json_exchange("GET", "/api/materials/variant-operations", NULL, &status);
  check(status == 200 && cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(answer, "operations")) >= 9,
        "the variant operations are listed");
  cJSON_Delete(answer);

  char *id = variant_source();
  check(id != NULL, "the source motif is saved");
  if (!id) {
    return;
  }
  const char *chain = "{\"steps\":[{\"op\":\"invert\"},{\"op\":\"augment\",\"params\":{\"ratio\":\"2:1\"}}]}";
  char request[1024];
  snprintf(request, sizeof(request), "{\"chain\":%s,\"count\":2,\"seed\":11}", chain);
  answer = item_post(id, "variants", request, &status);
  check(status == 200 && ok_true(answer) && number_of(answer, "seed") == 11, "variants answer with the seed");
  check(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(answer, "variants")) == 2, "two candidates are made");
  char first[256];
  snprintf(first, sizeof(first), "%s", candidate_degrees(answer, 0));
  cJSON *again = item_post(id, "variants", request, &status);
  check(status == 200 && strcmp(candidate_degrees(again, 0), first) == 0, "the same seed gives the same candidates");
  cJSON_Delete(again);

  cJSON *items = json_exchange("GET", "/api/materials/items?type=motif", NULL, &status);
  int before = cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(items, "items"));
  cJSON_Delete(items);
  check(status == 200, "the motif list reads");

  const cJSON *variant = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(answer, "variants"), 0);
  cJSON *save = cJSON_CreateObject();
  cJSON_AddItemToObject(save, "variant", cJSON_Duplicate(variant, 1));
  cJSON_AddStringToObject(save, "flag", "approved");
  char *save_text = cJSON_PrintUnformatted(save);
  cJSON *saved = item_post(id, "variants/save", save_text, &status);
  check(status == 200 && ok_true(saved), "a candidate is saved");
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(saved, "item");
  check(strcmp(string_of(item, "name"), "Form invert + augment") == 0, "the name defaults to the source and its operations");
  const char *derivation = string_of(variant, "derivation");
  check(strcmp(string_of(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(item, "versions"), 0), "note"), derivation) == 0,
        "the version note is the derivation line");
  char new_id[64];
  snprintf(new_id, sizeof(new_id), "%s", string_of(item, "id"));

  cJSON *read = item_get(new_id, "", &status);
  const cJSON *motif = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(read, "body"), "motif");
  const cJSON *derived = cJSON_GetObjectItemCaseSensitive(motif, "derived");
  check(status == 200 && strcmp(string_of(derived, "source_item"), id) == 0 &&
            number_of(derived, "source_version") == 1 && strcmp(string_of(derived, "flag"), "approved") == 0,
        "the derived record names the source, its version and the flag");
  check(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(derived, "operations")) == 2 &&
            number_of(derived, "seed") == 11 && cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(derived, "harmony")) &&
            cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(derived, "functions")) ==
                cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(variant, "functions")) &&
            fabs(number_of(derived, "remoteness") - number_of(variant, "remoteness")) < 1e-9,
        "the derived record holds the chain, the seed, no harmony, its functions and its remoteness");
  check(strcmp(string_of(motif, "degrees"), first) == 0 && strcmp(string_of(motif, "meter"), "4/4") == 0 &&
            strcmp(string_of(motif, "pickup"), "0") == 0,
        "the saved motif has the candidate's degrees and the source's meter and pickup");
  cJSON_Delete(read);

  items = json_exchange("GET", "/api/materials/items?type=motif", NULL, &status);
  check(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(items, "items")) == before + 1, "saving adds one motif");
  cJSON_Delete(items);

  cJSON *refused = item_post(id, "variants/save", "{\"variant\":{}}", &status);
  check(status == 400, "a candidate without a chain is refused");
  cJSON_Delete(refused);
  refused = item_post(id, "variants", "{\"count\":99}", &status);
  check(status == 400, "a count over 16 is refused");
  cJSON_Delete(refused);
  /* R-189: a distance goes to the Composer as --distance and bounds each variant's remoteness. */
  cJSON *close = item_post(id, "variants", "{\"distance\":\"close\",\"count\":4,\"seed\":3}", &status);
  const cJSON *close_variants = cJSON_GetObjectItemCaseSensitive(close, "variants");
  bool within = cJSON_GetArraySize(close_variants) > 0;
  const cJSON *near = NULL;
  cJSON_ArrayForEach(near, close_variants) {
    within = within && number_of(near, "remoteness") <= 1.0 / 3.0 + 1e-4;
  }
  check(status == 200 && within, "a close distance gives variants at most a third remote");
  cJSON_Delete(close);
  snprintf(request, sizeof(request), "{\"distance\":\"far\",\"chain\":%s}", chain);
  refused = item_post(id, "variants", request, &status);
  check(status == 400 && strstr(string_of(refused, "error"), "distance takes random operations") != NULL,
        "a distance with chosen operations is refused");
  cJSON_Delete(refused);
  refused = item_post(id, "variants", "{\"distance\":\"nearby\"}", &status);
  check(status == 400, "a distance that is not close, medium or far is refused");
  cJSON_Delete(refused);
  refused = item_post(id, "variants", "{\"chain\":{\"steps\":[{\"op\":\"nonsense\"}]}}", &status);
  check(status == 400, "an unknown operation is refused");
  cJSON_Delete(refused);

  free(save_text);
  cJSON_Delete(save);
  cJSON_Delete(saved);
  cJSON_Delete(answer);
  free(id);
}

/* Generates one candidate of id with an invert chain and saves it; returns the new item id (caller frees). */
static char *save_inverted(const char *id, int seed) {
  int status = 0;
  char request[128];
  snprintf(request, sizeof(request), "{\"chain\":{\"steps\":[{\"op\":\"invert\"}]},\"count\":1,\"seed\":%d}", seed);
  cJSON *answer = item_post(id, "variants", request, &status);
  char *new_id = NULL;
  if (status == 200) {
    cJSON *save = cJSON_CreateObject();
    cJSON_AddItemToObject(save, "variant", cJSON_Duplicate(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(answer, "variants"), 0), 1));
    char *text = cJSON_PrintUnformatted(save);
    cJSON *saved = item_post(id, "variants/save", text, &status);
    new_id = status == 200 ? strdup(string_of(cJSON_GetObjectItemCaseSensitive(saved, "item"), "id")) : NULL;
    cJSON_Delete(saved);
    free(text);
    cJSON_Delete(save);
  }
  cJSON_Delete(answer);
  return new_id;
}

static int version_count(const cJSON *answer) {
  return cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(answer, "item"), "versions"));
}

/* POST /api/materials/recognize {source, target} with the two references written out. */
static cJSON *recognize_post(const char *source, int source_version, const char *target, int target_version, int *status) {
  char request[512];
  snprintf(request, sizeof(request), "{\"source\":{\"item\":\"%s\",\"version\":%d},\"target\":{\"item\":\"%s\",\"version\":%d}}",
           source, source_version, target, target_version);
  return json_exchange("POST", "/api/materials/recognize", request, status);
}

/* Recognizing how an inverted motif derives from its source, and what the route refuses. */
static void test_recognize(void) {
  int status = 0;
  char *id = variant_source();
  char *inverted = id ? save_inverted(id, 5) : NULL;
  check(id && inverted, "a motif and its inversion are saved");
  if (!id || !inverted) {
    free(id);
    free(inverted);
    return;
  }
  cJSON *answer = recognize_post(id, 1, inverted, 1, &status);
  const cJSON *steps = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(answer, "chain"), "steps");
  check(status == 200 && ok_true(answer) && cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(answer, "found")) &&
            cJSON_GetArraySize(steps) == 1 && strcmp(string_of(cJSON_GetArrayItem(steps, 0), "op"), "invert") == 0,
        "recognize finds the inversion");
  check(cJSON_IsString(cJSON_GetObjectItemCaseSensitive(answer, "derivation")) &&
            cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(answer, "kept")) &&
            cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(answer, "distance")),
        "recognize answers the derivation, what was kept and the distance");
  cJSON_Delete(answer);

  answer = recognize_post(id, 1, id, 1, &status);
  check(status == 200 && cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(answer, "found")) &&
            number_of(answer, "distance") == 0,
        "a motif recognizes as itself at no distance");
  cJSON_Delete(answer);

  answer = recognize_post(id, 1, inverted, 9, &status);
  check(status == 404, "a version that is not there is 404");
  cJSON_Delete(answer);
  answer = json_exchange("POST", "/api/materials/recognize", "{\"source\":{\"item\":\"x\"},\"target\":{}}", &status);
  check(status == 400, "a reference without a version is 400");
  cJSON_Delete(answer);
  answer = json_exchange("GET", "/api/materials/recognize", NULL, &status);
  check(status == 405, "recognize takes POST only");
  cJSON_Delete(answer);

  char project[64] = "";
  first_project(project, sizeof(project));
  char *theme = create_item("theme", "Not a motif", project, "{\"uses\":[],\"theme\":{\"form\":null,\"slots\":[]}}");
  check(theme != NULL, "a theme is saved");
  if (theme) {
    answer = recognize_post(theme, 1, inverted, 1, &status);
    check(status == 400, "a theme is not a motif to compare");
    cJSON_Delete(answer);
    free(theme);
  }
  free(id);
  free(inverted);
}

/* A placeholder theme still saves and reads, and a version without files has none to serve. */
static void test_theme_placeholder(void) {
  int status = 0;
  char project[64] = "";
  first_project(project, sizeof(project));
  char *theme = create_item("theme", "Placeholder", project, "{\"uses\":[],\"theme\":{\"form\":\"sentence\",\"slots\":[1]}}");
  check(theme != NULL, "a placeholder theme with a string form and loose slots is saved");
  if (!theme) {
    return;
  }
  char path[256];
  snprintf(path, sizeof(path), "/api/materials/items/%s/versions/1/files/plan.json", theme);
  cJSON *answer = json_exchange("GET", path, NULL, &status);
  check(status == 404 && !ok_true(answer), "a version without files has no plan to serve");
  cJSON_Delete(answer);
  snprintf(path, sizeof(path), "/api/materials/items/%s/versions/1/files/item.json", theme);
  answer = json_exchange("GET", path, NULL, &status);
  check(status == 404, "only the five file names are served");
  cJSON_Delete(answer);
  snprintf(path, sizeof(path), "/api/materials/items/%s/compose", theme);
  answer = json_exchange("POST", path, "{}", &status);
  check(status == 400 && !ok_true(answer), "a placeholder theme cannot be composed");
  cJSON_Delete(answer);
  free(theme);
}

/* POST /api/materials/items/<target>/link-form with the recognition of source -> target as the recognize route gave it. */
static cJSON *link_form_post(const char *source, int source_version, const char *target, cJSON *recognition, int *status) {
  cJSON *request = cJSON_CreateObject();
  cJSON *reference = cJSON_AddObjectToObject(request, "source");
  cJSON_AddStringToObject(reference, "item", source);
  cJSON_AddNumberToObject(reference, "version", source_version);
  cJSON_AddItemToObject(request, "recognition", cJSON_Duplicate(recognition, 1));
  char *text = cJSON_PrintUnformatted(request);
  cJSON *answer = item_post(target, "link-form", text, status);
  free(text);
  cJSON_Delete(request);
  return answer;
}

/* Linking a motif as a form of another from a recognition writes a new version with derived; a loop is refused. */
static void test_link_form(void) {
  int status = 0;
  char *id = variant_source();
  char *other = variant_source();
  char *inverted = id ? save_inverted(id, 6) : NULL;
  check(id && other && inverted, "three motifs are saved to link");
  if (!id || !other || !inverted) {
    free(id);
    free(other);
    free(inverted);
    return;
  }
  cJSON *recognition = recognize_post(other, 1, inverted, 1, &status);
  cJSON *linked = link_form_post(other, 1, inverted, recognition, &status);
  check(status == 200 && ok_true(linked) && version_count(linked) == 2, "linking writes a second version of the target");
  cJSON *read = item_get(inverted, "", &status);
  const cJSON *motif = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(read, "body"), "motif");
  const cJSON *derived = cJSON_GetObjectItemCaseSensitive(motif, "derived");
  check(status == 200 && strcmp(string_of(derived, "source_item"), other) == 0 &&
            number_of(derived, "source_version") == 1 && cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(derived, "seed")) &&
            strcmp(string_of(derived, "flag"), "unflagged") == 0,
        "derived names the source and its version, has no seed and is unflagged");
  check(strcmp(string_of(derived, "kind"), "motive-form") == 0 &&
            cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(derived, "operations")) ==
                cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(recognition, "chain"), "steps")) &&
            cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(derived, "kept")) &&
            cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(derived, "functions")),
        "derived holds the recognition's chain, its kind and what it kept");
  check(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(derived, "standalone")),
        "a linked form is a standalone motif, not a variant");
  cJSON_Delete(read);
  cJSON_Delete(linked);

  cJSON *refused = link_form_post(inverted, 1, other, recognition, &status);
  check(status == 409, "a motif is not a form of one of its own forms");
  cJSON_Delete(refused);
  refused = link_form_post(other, 1, other, recognition, &status);
  check(status == 409, "a motif is not a form of itself");
  cJSON_Delete(refused);
  refused = link_form_post(inverted, 1, id, recognition, &status);
  check(status == 200, "a form of a form can be linked");
  cJSON_Delete(refused);
  refused = link_form_post(id, 1, other, recognition, &status);
  check(status == 409, "the root of a chain of forms is not a form of its last");
  cJSON_Delete(refused);

  cJSON *not_found = cJSON_Parse("{\"found\":false}");
  refused = link_form_post(other, 1, inverted, not_found, &status);
  check(status == 400, "a recognition that found no chain is refused");
  cJSON_Delete(refused);
  cJSON_Delete(not_found);
  refused = link_form_post(other, 9, inverted, recognition, &status);
  check(status == 404, "a source version that is not there is 404");
  cJSON_Delete(refused);
  refused = json_exchange("POST", "/api/materials/items/nothing/link-form", "{}", &status);
  check(status == 400 || status == 404, "a request without a source is refused");
  cJSON_Delete(refused);
  cJSON_Delete(recognition);
  free(id);
  free(other);
  free(inverted);
}

/* Flags are new versions, a reroll changes the seed, and the tree nests the variants of variants. */
static void test_variant_flags_and_tree(void) {
  int status = 0;
  char *source = variant_source();
  char *child = source ? save_inverted(source, 3) : NULL;
  char *grandchild = child ? save_inverted(child, 4) : NULL;
  check(source && child && grandchild, "a source, a variant and a variant of the variant are saved");
  if (!grandchild) {
    free(source);
    free(child);
    return;
  }

  cJSON *answer = item_post(child, "flag", "{\"flag\":\"rejected\",\"kind\":\"variant\"}", &status);
  check(status == 200 && version_count(answer) == 2, "a flag is a new version");
  check(strcmp(string_of(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(answer, "item"), "versions"), 1), "note"), "rejected") == 0,
        "the version note is the flag");
  cJSON_Delete(answer);
  answer = item_get(child, "", &status);
  const cJSON *motif = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(answer, "body"), "motif");
  const cJSON *derived = cJSON_GetObjectItemCaseSensitive(motif, "derived");
  check(strcmp(string_of(derived, "flag"), "rejected") == 0 && strcmp(string_of(derived, "kind"), "variant") == 0,
        "the flag and the kind are changed");
  char degrees[256];
  snprintf(degrees, sizeof(degrees), "%s", string_of(motif, "degrees"));
  double seed = number_of(derived, "seed");
  cJSON_Delete(answer);
  answer = item_get(child, "/versions/1", &status);
  check(status == 200 && strcmp(string_of(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(answer, "body"), "motif"), "derived"), "flag"), "unflagged") == 0,
        "the first version keeps the old flag");
  cJSON_Delete(answer);
  answer = item_post(child, "flag", "{\"flag\":\"maybe\"}", &status);
  check(status == 400, "an unknown flag is 400");
  cJSON_Delete(answer);
  answer = item_post(source, "flag", "{\"flag\":\"approved\"}", &status);
  check(status == 400, "a motif without derived has no flag");
  cJSON_Delete(answer);

  answer = item_post(child, "reroll", NULL, &status);
  check(status == 200 && version_count(answer) == 3, "a reroll is a new version");
  cJSON_Delete(answer);
  answer = item_get(child, "", &status);
  motif = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(answer, "body"), "motif");
  derived = cJSON_GetObjectItemCaseSensitive(motif, "derived");
  check(number_of(derived, "seed") == seed + 1000003 && strcmp(string_of(motif, "degrees"), degrees) != 0,
        "a reroll moves the seed and changes the degrees");
  check(strcmp(string_of(derived, "flag"), "unflagged") == 0 && strcmp(string_of(derived, "source_item"), source) == 0,
        "a reroll resets the flag and keeps the source");
  cJSON_Delete(answer);
  answer = item_get(child, "", &status);
  check(strncmp(string_of(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(answer, "item"), "versions"), 2), "note"), "rerolled: ", 10) == 0,
        "the reroll note is the derivation");
  cJSON_Delete(answer);
  answer = item_post(source, "reroll", NULL, &status);
  check(status == 400, "a motif without derived cannot be rerolled");
  cJSON_Delete(answer);

  answer = item_get(source, "/variant-tree", &status);
  const cJSON *tree = cJSON_GetObjectItemCaseSensitive(answer, "tree");
  const cJSON *kids = cJSON_GetObjectItemCaseSensitive(tree, "children");
  check(status == 200 && strcmp(string_of(tree, "id"), source) == 0 && cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(tree, "derived")) &&
            cJSON_GetArraySize(kids) == 1,
        "the tree starts at the source with one child");
  const cJSON *node = cJSON_GetArrayItem(kids, 0);
  check(strcmp(string_of(node, "id"), child) == 0 && number_of(node, "current_version") == 3 &&
            cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(node, "children")) == 1 &&
            strcmp(string_of(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(node, "children"), 0), "id"), grandchild) == 0,
        "the tree nests the variant of the variant");
  cJSON_Delete(answer);
  answer = item_get(grandchild, "/variant-tree", &status);
  check(status == 200 && cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(answer, "tree"), "children")) == 0 &&
            strcmp(string_of(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(answer, "tree"), "derived"), "source_item"), child) == 0,
        "a leaf has no children and its derived record");
  cJSON_Delete(answer);
  answer = item_get("mot-0000000000000000", "/variant-tree", &status);
  check(status == 404, "the tree of a missing item is 404");
  cJSON_Delete(answer);

  free(source);
  free(child);
  free(grandchild);
}

/* R-187: items carry variant_of, and a motif is deleted with its variants. */
static void test_variant_ownership(void) {
  int status = 0;
  char project[64] = "";
  first_project(project, sizeof(project));
  char *source = variant_source();
  char *child = source ? save_inverted(source, 3) : NULL;
  char *grandchild = child ? save_inverted(child, 4) : NULL;
  check(source && child && grandchild, "a motif, its variant and the variant's variant are saved");
  if (!grandchild) {
    free(source);
    free(child);
    return;
  }

  cJSON *answer = item_get(source, "", &status);
  check(status == 200 && cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(answer, "item"), "variant_of")),
        "the item answer of a motif has variant_of null");
  cJSON_Delete(answer);
  answer = item_get(grandchild, "", &status);
  check(status == 200 && strcmp(string_of(cJSON_GetObjectItemCaseSensitive(answer, "item"), "variant_of"), child) == 0,
        "the item answer of a variant names its motif in variant_of");
  cJSON_Delete(answer);
  answer = json_exchange("GET", "/api/materials/items?type=motif", NULL, &status);
  const cJSON *items = cJSON_GetObjectItemCaseSensitive(answer, "items");
  int named = 0;
  const cJSON *entry = NULL;
  cJSON_ArrayForEach(entry, items) {
    if (strcmp(string_of(entry, "id"), child) == 0 && strcmp(string_of(entry, "variant_of"), source) == 0) {
      named++;
    }
    if (strcmp(string_of(entry, "id"), source) == 0 && cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(entry, "variant_of"))) {
      named++;
    }
  }
  check(status == 200 && named == 2, "the item list carries variant_of");
  cJSON_Delete(answer);

  /* A theme using the variant of the variant keeps the motif from being deleted until forced. */
  char theme_body[1024];
  theme_using(grandchild, 1, theme_body, sizeof(theme_body));
  char *theme = create_item("theme", "Variant user", project, theme_body);
  check(theme != NULL, "a theme that uses the variant is created");
  char path[256];
  snprintf(path, sizeof(path), "/api/materials/items/%s", source);
  answer = json_exchange("DELETE", path, NULL, &status);
  const cJSON *used_by = cJSON_GetObjectItemCaseSensitive(answer, "used_by");
  check(status == 409 && cJSON_GetArraySize(used_by) == 1 && strcmp(string_of(cJSON_GetArrayItem(used_by, 0), "id"), theme ? theme : "") == 0,
        "deleting a motif whose variant is used is 409 with that user");
  cJSON_Delete(answer);
  answer = item_get(grandchild, "", &status);
  check(status == 200, "the 409 deleted nothing");
  cJSON_Delete(answer);
  snprintf(path, sizeof(path), "/api/materials/items/%s?force=1", source);
  answer = json_exchange("DELETE", path, NULL, &status);
  check(status == 200 && ok_true(answer), "a forced delete goes through");
  cJSON_Delete(answer);
  int gone = 0;
  const char *ids[] = {source, child, grandchild};
  for (int i = 0; i < 3; i++) {
    answer = item_get(ids[i], "", &status);
    gone += status == 404;
    cJSON_Delete(answer);
  }
  check(gone == 3, "the motif, its variant and the variant's variant are gone");
  answer = item_get(theme, "", &status);
  check(status == 200, "the theme stays");
  cJSON_Delete(answer);

  /* No user: the variants go with the motif. */
  char *other = variant_source();
  char *other_child = other ? save_inverted(other, 5) : NULL;
  check(other && other_child, "a second motif and its variant are saved");
  if (other_child) {
    snprintf(path, sizeof(path), "/api/materials/items/%s", other);
    answer = json_exchange("DELETE", path, NULL, &status);
    check(status == 200, "deleting a motif without users is 200");
    cJSON_Delete(answer);
    answer = item_get(other_child, "", &status);
    check(status == 404, "its variant went with it");
    cJSON_Delete(answer);
  }
  free(other);
  free(other_child);
  free(theme);
  free(source);
  free(child);
  free(grandchild);
}

/* Promoting a variant writes a standalone version, renames and moves it, and the tree marks it. */
static void test_promote(void) {
  int status = 0;
  char project[64] = "";
  first_project(project, sizeof(project));
  char *source = variant_source();
  char *child = source ? save_inverted(source, 7) : NULL;
  char *grandchild = child ? save_inverted(child, 8) : NULL;
  check(source && child && grandchild, "a motif, its variant and the variant's variant are saved to promote");
  if (!grandchild) {
    free(source);
    free(child);
    return;
  }
  char text[256];
  snprintf(text, sizeof(text), "{\"name\":\"Own motif\",\"project_id\":\"%s\"}", project);
  cJSON *answer = item_post(child, "promote", text, &status);
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(answer, "item");
  check(status == 200 && ok_true(answer) && strcmp(string_of(item, "name"), "Own motif") == 0 &&
            strcmp(string_of(item, "project_id"), project) == 0 && strcmp(string_of(item, "id"), child) == 0 &&
            version_count(answer) == 2,
        "promoting keeps the item and writes a second version under the new name");
  check(strcmp(string_of(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(item, "versions"), 1), "note"), "Saved as new motif") == 0,
        "the note defaults to Saved as new motif");
  cJSON_Delete(answer);
  answer = item_get(child, "", &status);
  const cJSON *derived = cJSON_GetObjectItemCaseSensitive(
      cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(answer, "body"), "motif"), "derived");
  check(status == 200 && cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(derived, "standalone")) &&
            strcmp(string_of(derived, "source_item"), source) == 0 &&
            cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(answer, "item"), "variant_of")),
        "the version is standalone, keeps its derivation and is no longer a variant");
  cJSON_Delete(answer);

  answer = item_get(source, "/variant-tree", &status);
  const cJSON *tree = cJSON_GetObjectItemCaseSensitive(answer, "tree");
  const cJSON *first = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(tree, "children"), 0);
  check(status == 200 && !cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(tree, "standalone")) && first &&
            cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(first, "standalone")),
        "the tree marks the promoted node standalone and the root not");
  cJSON_Delete(answer);

  cJSON *refused = item_post(source, "promote", text, &status);
  check(status == 400, "a motif that is not a variant cannot be promoted");
  cJSON_Delete(refused);
  refused = item_post(child, "promote", text, &status);
  check(status == 400, "a promoted motif is not a variant to promote again");
  cJSON_Delete(refused);
  refused = item_post(grandchild, "promote", "{\"name\":\"X\"}", &status);
  check(status == 400, "promote needs a project");
  cJSON_Delete(refused);
  refused = item_post(grandchild, "promote", "{\"name\":\"X\",\"project_id\":\"prj-0000000000000000\"}", &status);
  check(status == 404, "a project that is not there is 404");
  cJSON_Delete(refused);
  answer = item_get(grandchild, "", &status);
  check(status == 200 && strcmp(string_of(cJSON_GetObjectItemCaseSensitive(answer, "item"), "name"), "X") != 0,
        "a refused promote renames nothing");
  cJSON_Delete(answer);
  free(source);
  free(child);
  free(grandchild);
}

/* R-190: each-op runs every registry operation on the motif, and the marks are kept per motif and not as a version. */
static void test_each_op(void) {
  int status = 0;
  char *id = variant_source();
  check(id != NULL, "a motif is saved for Try every operation");
  if (!id) {
    return;
  }
  cJSON *answer = item_post(id, "each-op", "{\"seed\":5}", &status);
  const cJSON *variants = cJSON_GetObjectItemCaseSensitive(answer, "variants");
  const cJSON *skipped = cJSON_GetObjectItemCaseSensitive(answer, "skipped");
  check(status == 200 && ok_true(answer) && number_of(answer, "seed") == 5 && cJSON_GetArraySize(variants) >= 9,
        "each-op answers one variant per operation with the seed");
  bool one_step = cJSON_GetArraySize(variants) > 0;
  const cJSON *variant = NULL;
  cJSON_ArrayForEach(variant, variants) {
    one_step = one_step &&
               cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(variant, "chain"), "steps")) == 1;
  }
  check(one_step, "each result is a one-step chain");
  bool listed = cJSON_IsArray(skipped) && cJSON_GetArraySize(skipped) > 0;
  const cJSON *entry = cJSON_GetArrayItem(skipped, 0);
  check(listed && strlen(string_of(entry, "op")) > 0 && strlen(string_of(entry, "reason")) > 0,
        "the operations the motif refuses are skipped with their reason");
  cJSON_Delete(answer);

  cJSON *refused = item_post(id, "each-op", "{\"seed\":-1}", &status);
  check(status == 400, "a seed below 0 is refused");
  cJSON_Delete(refused);

  /* Marks. */
  cJSON *marks = item_get(id, "/op-checks", &status);
  check(status == 200 && cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(marks, "checks")) == 0, "a motif starts without marks");
  cJSON_Delete(marks);
  char path[256];
  snprintf(path, sizeof(path), "/api/materials/items/%s/op-checks", id);
  const char *put =
      "{\"checks\":{\"invert\":{\"verdict\":\"wrong\",\"comment\":\"too low\",\"seed\":5,\"notes_text\":\"C4:q\","
      "\"date\":\"2026-10-23\"}}}";
  marks = json_exchange("PUT", path, put, &status);
  check(status == 200 && ok_true(marks), "the marks are put");
  cJSON_Delete(marks);
  marks = item_get(id, "/op-checks", &status);
  const cJSON *invert = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(marks, "checks"), "invert");
  check(status == 200 && strcmp(string_of(invert, "verdict"), "wrong") == 0 && strcmp(string_of(invert, "comment"), "too low") == 0 &&
            number_of(invert, "seed") == 5,
        "the marks read back");
  cJSON_Delete(marks);
  cJSON *read = item_get(id, "", &status);
  check(status == 200 && version_count(read) == 1, "marking writes no version");
  cJSON_Delete(read);
  refused = json_exchange("PUT", path, "{\"checks\":{\"invert\":{\"verdict\":\"fine\"}}}", &status);
  check(status == 400, "a mark with a verdict that is not works or wrong is refused");
  cJSON_Delete(refused);
  refused = json_exchange("PUT", "/api/materials/items/mot-0000000000000000/op-checks", put, &status);
  check(status == 404, "marks for a motif that is not there are 404");
  cJSON_Delete(refused);
  refused = item_post(id, "op-checks", put, &status);
  check(status == 405, "op-checks takes GET and PUT only");
  cJSON_Delete(refused);
  free(id);
}

/* Random motifs come from the Composer unsaved, with a key of the mode, and bad inputs are refused. */
static void test_random_motifs(void) {
  int status = 0;
  cJSON *answer = json_exchange("POST", "/api/materials/motifs/random",
                                "{\"mode\":\"minor\",\"meter\":\"3/4\",\"form\":\"full\",\"count\":3,\"seed\":5}", &status);
  const cJSON *motives = cJSON_GetObjectItemCaseSensitive(answer, "motives");
  check(status == 200 && ok_true(answer) && cJSON_GetArraySize(motives) == 3, "three minor 3/4 motives are drawn");
  const cJSON *analysis = cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(motives, 0), "analysis");
  check(string_of(analysis, "notes_text")[0] != '\0' && string_of(analysis, "degrees_text")[0] != '\0',
        "a motive carries its note list and degrees");
  char first[256];
  snprintf(first, sizeof(first), "%s", string_of(analysis, "notes_text"));
  cJSON_Delete(answer);
  answer = json_exchange("POST", "/api/materials/motifs/random",
                         "{\"mode\":\"minor\",\"meter\":\"3/4\",\"form\":\"full\",\"count\":3,\"seed\":5}", &status);
  analysis = cJSON_GetObjectItemCaseSensitive(
      cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(answer, "motives"), 0), "analysis");
  check(status == 200 && strcmp(string_of(analysis, "notes_text"), first) == 0, "the same seed draws the same motives");
  cJSON_Delete(answer);

  answer = json_exchange("POST", "/api/materials/motifs/random",
                         "{\"mode\":\"major\",\"meter\":\"4/4\",\"key\":\"D major\",\"form\":\"rhythm\",\"count\":1}", &status);
  analysis = cJSON_GetObjectItemCaseSensitive(
      cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(answer, "motives"), 0), "analysis");
  check(status == 200 && string_of(analysis, "rhythm_text")[0] != '\0', "a rhythm motive is drawn with a given key");
  cJSON_Delete(answer);

  answer = json_exchange("POST", "/api/materials/motifs/random",
                         "{\"mode\":\"major\",\"meter\":\"4/4\",\"style_id\":\"sty-0000000000000000\"}", &status);
  check(status == 400, "an unknown style is refused");
  cJSON_Delete(answer);
  answer = json_exchange("POST", "/api/materials/motifs/random", "{\"mode\":\"major\",\"meter\":\"four\"}", &status);
  check(status == 400, "a bad meter is refused");
  cJSON_Delete(answer);
  answer = json_exchange("POST", "/api/materials/motifs/random", "{\"mode\":\"dorian\",\"meter\":\"4/4\"}", &status);
  check(status == 400, "a church mode is refused");
  cJSON_Delete(answer);
  answer = json_exchange("POST", "/api/materials/motifs/random", "{\"mode\":\"major\",\"meter\":\"4/4\",\"count\":99}", &status);
  check(status == 400, "a count over 16 is refused");
  cJSON_Delete(answer);
  answer = json_exchange("GET", "/api/materials/motifs/random", NULL, &status);
  check(status == 405, "random motifs take only POST");
  cJSON_Delete(answer);
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
  test_version_notes();
  test_motif_refused();
#ifndef _WIN32
  test_motif_analysis_error();
#endif
  test_motif_forms();
  test_motif_import();
  test_motifs_in_c();
  test_motif_render();
  test_harmony();
  test_variants();
  test_variant_flags_and_tree();
  test_variant_ownership();
  test_promote();
  test_each_op();
  test_recognize();
  test_link_form();
  test_theme_placeholder();
  test_random_motifs();
  test_body_limit();

  ursatz_gui_server_stop(ctx);

  if (failures > 0) {
    fprintf(stderr, "%d check(s) failed\n", failures);
    return 1;
  }
  return 0;
}
