/* Theme bodies, composing and the files of a version end to end (R-167). The theme runs on the job queue through the
 * Composer API, in process; the request the route built is read back from the version's request.json. The motifs are
 * saved with the Composer too, and the composed files are checked as files (version_check.h). Uses a plain TCP socket
 * for the HTTP requests, same as test_composer_route.c. Runs in its own working directory (materials/, subjects/ and
 * pieces/ are created there). */

#include <civetweb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <cJSON.h>

#include "server.h"
#include "version_check.h"

#ifndef URSATZ_GUI_TEST_LINK_FIXTURES
#error "URSATZ_GUI_TEST_LINK_FIXTURES must name the link fixtures directory"
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

/* One HTTP exchange; the whole response (status line, headers, body), newly allocated, or NULL. */
static char *http_exchange(const char *method, const char *path, const char *extra_headers, const void *body,
                           size_t body_len, size_t *out_size) {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    return NULL;
  }
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(18773);
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
  if (out_size) {
    *out_size = total;
  }
  return response;
}

static const char *response_body(const char *response) {
  const char *split = response ? strstr(response, "\r\n\r\n") : NULL;
  return split ? split + 4 : "";
}

static int response_status(const char *response) {
  int status = 0;
  return response && sscanf(response, "HTTP/1.1 %d", &status) == 1 ? status : 0;
}

/* The body parsed as JSON; the status goes to *status. Caller frees the JSON. */
static cJSON *exchange(const char *method, const char *path, const char *body, int *status) {
  char *response = http_exchange(method, path, NULL, body, body ? strlen(body) : 0, NULL);
  *status = response_status(response);
  cJSON *json = cJSON_Parse(response_body(response));
  free(response);
  return json;
}

/* The content type of a response, copied into out. */
static void content_type_of(const char *response, char *out, size_t size) {
  const char *line = response ? strstr(response, "Content-Type: ") : NULL;
  out[0] = '\0';
  if (line) {
    line += strlen("Content-Type: ");
    size_t length = strcspn(line, "\r\n");
    snprintf(out, size, "%.*s", (int)(length < size - 1 ? length : size - 1), line);
  }
}

/* The whole file; its size goes to *out_size when that is not NULL. */
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
        buffer = NULL;
      } else {
        buffer = grown;
      }
    }
  }
  fclose(file);
  if (buffer) {
    buffer[length] = '\0';
  }
  if (out_size) {
    *out_size = length;
  }
  return buffer;
}

static cJSON *read_json_file(const char *path) {
  char *text = read_file(path, NULL);
  cJSON *json = text ? cJSON_Parse(text) : NULL;
  free(text);
  return json;
}

static bool file_exists(const char *path) {
  struct stat st;
  return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static bool dir_exists(const char *path) {
  struct stat st;
  return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static const char *string_of(const cJSON *object, const char *key) {
  const cJSON *item = object ? cJSON_GetObjectItemCaseSensitive(object, key) : NULL;
  return cJSON_IsString(item) ? item->valuestring : "";
}

static double number_of(const cJSON *object, const char *key) {
  const cJSON *item = object ? cJSON_GetObjectItemCaseSensitive(object, key) : NULL;
  return cJSON_IsNumber(item) ? item->valuedouble : -1;
}

static bool ok_true(const cJSON *answer) {
  return cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(answer, "ok"));
}

static const cJSON *member(const cJSON *object, const char *key) {
  return object ? cJSON_GetObjectItemCaseSensitive(object, key) : NULL;
}

static char *copy_string(const char *text) {
  size_t length = strlen(text) + 1;
  char *copy = malloc(length);
  if (copy) {
    memcpy(copy, text, length);
  }
  return copy;
}

/* --- the store ----------------------------------------------------------- */

static char g_project[64] = "";
static const char *const kDegreesA = "1:e 3:e 5:e 3:e 1:e 3:e 5:e 1':e";

/* POST /api/materials/items; the status goes to *status, and a new item's id is returned (caller frees) or NULL. */
static char *create_item(const char *type, const char *name, const char *body, int *status) {
  char *request = malloc(strlen(body) + 512);
  char *id = NULL;
  if (!request) {
    return NULL;
  }
  sprintf(request, "{\"type\":\"%s\",\"name\":\"%s\",\"project_id\":\"%s\",\"body\":%s}", type, name, g_project, body);
  cJSON *answer = exchange("POST", "/api/materials/items", request, status);
  const cJSON *item = member(answer, "item");
  if (*status == 200 && cJSON_IsString(member(item, "id"))) {
    id = copy_string(string_of(item, "id"));
  }
  cJSON_Delete(answer);
  free(request);
  return id;
}

/* A motif created from a whole body text. */
static char *create_item_plain(const char *type, const char *name, const char *body) {
  int status = 0;
  return create_item(type, name, body, &status);
}

/* A motif of the given form and role; derived is a JSON text ("null" for none). */
static char *create_motif(const char *name, const char *form, const char *role, const char *notes, const char *derived) {
  char body[1500];
  int status = 0;
  snprintf(body, sizeof(body),
           "{\"uses\":[],\"motif\":{\"role\":\"%s\",\"form\":\"%s\",\"notes\":\"%s\",\"meter\":\"4/4\","
           "\"key\":\"C minor\",\"pickup\":\"0\",\"features\":[],\"analysis\":null,\"derived\":%s}}",
           role, form, notes, derived);
  return create_item("motif", name, body, &status);
}

/* A motif promoted from source (R-192): derived from it, with standalone true, approved. */
static char *create_promoted(const char *name, const char *source, const char *notes) {
  char derived[800];
  snprintf(derived, sizeof(derived),
           "{\"source_item\":\"%s\",\"source_version\":1,\"operations\":[{\"op\":\"transpose\",\"params\":"
           "{\"tonal\":1,\"by\":1}}],\"seed\":0,\"kind\":\"motive-form\",\"kept\":[\"rhythm\"],\"changed\":"
           "[\"intervals\"],\"harmony\":null,\"functions\":[\"beginning\"],\"flag\":\"approved\",\"standalone\":true}",
           source);
  return create_motif(name, "full", "melodic", notes, derived);
}

/* A motif derived from source (version 1) with the given flag. */
static char *create_derived(const char *name, const char *source, const char *flag, const char *notes) {
  char derived[800];
  snprintf(derived, sizeof(derived),
           "{\"source_item\":\"%s\",\"source_version\":1,\"operations\":[{\"op\":\"transpose\",\"params\":"
           "{\"tonal\":1,\"by\":1}}],\"seed\":0,\"kind\":\"motive-form\",\"kept\":[\"rhythm\"],\"changed\":"
           "[\"intervals\"],\"harmony\":null,\"functions\":[\"beginning\"],\"flag\":\"%s\"}",
           source, flag);
  return create_motif(name, "full", "melodic", notes, derived);
}

/* The theme body text with the given slots (JSON array text) and the members after "slots" (JSON members, may be ""). */
static void theme_body(const char *slots, const char *more, char *out, size_t size) {
  snprintf(out, size,
           "{\"uses\":[],\"theme\":{\"form\":{\"into\":\"sentence\",\"hybrid\":null,\"mode\":\"minor\","
           "\"meter\":\"4/4\",\"bars\":8,\"cadence\":\"PAC\",\"first_cadence\":null},\"slots\":%s%s}}",
           slots, more);
}

static void slots_of(const char *basic, const char *cadential, char *out, size_t size) {
  snprintf(out, size,
           "[{\"slot\":\"basic\",\"item_id\":\"%s\",\"version\":1},{\"slot\":\"cadential\",\"item_id\":\"%s\","
           "\"version\":1}]",
           basic, cadential);
}

static void basic_slot_of(const char *basic, char *out, size_t size) {
  snprintf(out, size, "[{\"slot\":\"basic\",\"item_id\":\"%s\",\"version\":1}]", basic);
}

static cJSON *get_item(const char *id, int *status) {
  char path[160];
  snprintf(path, sizeof(path), "/api/materials/items/%s", id);
  return exchange("GET", path, NULL, status);
}

static int current_version(const char *id) {
  int status = 0;
  cJSON *answer = get_item(id, &status);
  int version = (int)number_of(member(answer, "item"), "current_version");
  cJSON_Delete(answer);
  return version;
}

/* The theme member of a GET answer's body. */
static const cJSON *theme_of(const cJSON *answer) {
  return member(member(answer, "body"), "theme");
}

/* Waits for the job an accepted request names; its result is the answer the route gave before jobs, and *status the
   status of it. */
static cJSON *finish_job(cJSON *accepted, int *status) {
  const cJSON *job = cJSON_GetObjectItemCaseSensitive(accepted, "job");
  const cJSON *id = cJSON_GetObjectItemCaseSensitive(job, "id");
  if (*status != 202 || !cJSON_IsString(id)) {
    return accepted;
  }
  char path[160];
  snprintf(path, sizeof(path), "/api/jobs/%s", id->valuestring);
  for (int i = 0; i < 6000; i++) {
    int poll = 0;
    cJSON *answer = exchange("GET", path, NULL, &poll);
    const cJSON *current = cJSON_GetObjectItemCaseSensitive(answer, "job");
    const cJSON *state = cJSON_GetObjectItemCaseSensitive(current, "state");
    if (cJSON_IsString(state) && strcmp(state->valuestring, "queued") != 0 && strcmp(state->valuestring, "running") != 0) {
      const cJSON *http = cJSON_GetObjectItemCaseSensitive(current, "http_status");
      cJSON *result = cJSON_DetachItemFromObjectCaseSensitive((cJSON *)current, "result");
      *status = cJSON_IsNumber(http) ? http->valueint : 499;
      cJSON_Delete(answer);
      cJSON_Delete(accepted);
      return result;
    }
    cJSON_Delete(answer);
    sleep_ms(50);
  }
  cJSON_Delete(accepted);
  *status = 0;
  return NULL;
}

/* The request.json of the version the last successful compose wrote. */
static char g_last_request_path[512];

static cJSON *compose(const char *id, const char *body, int *status) {
  char path[160];
  snprintf(path, sizeof(path), "/api/materials/items/%s/compose", id);
  cJSON *answer = finish_job(exchange("POST", path, body, status), status);
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(answer, "item");
  const cJSON *version = cJSON_GetObjectItemCaseSensitive(item, "current_version");
  if (*status == 200 && cJSON_IsNumber(version)) {
    snprintf(g_last_request_path, sizeof(g_last_request_path), "materials/items/%s/v%d/request.json", id, version->valueint);
  }
  return answer;
}

static void version_dir(const char *id, int version, char *out, size_t size) {
  snprintf(out, size, "materials/items/%s/v%d", id, version);
}

/* The request the last compose read, as the version kept it. */
static cJSON *last_request(void) {
  return read_json_file(g_last_request_path);
}

/* Uploads a link fixture to the Library; the new piece's id (caller frees), or NULL. */
static char *upload_piece(const char *name) {
  char path[600];
  char headers[256];
  size_t size = 0;
  snprintf(path, sizeof(path), "%s/%s", URSATZ_GUI_TEST_LINK_FIXTURES, name);
  char *data = read_file(path, &size);
  snprintf(headers, sizeof(headers), "X-Filename: %s\r\n", name);
  char *response = data ? http_exchange("POST", "/api/analyzer/pieces", headers, data, size, NULL) : NULL;
  free(data);
  cJSON *json = cJSON_Parse(response_body(response));
  char *id = cJSON_IsString(member(json, "id")) ? copy_string(string_of(json, "id")) : NULL;
  cJSON_Delete(json);
  free(response);
  return id;
}

/* --- the tests ----------------------------------------------------------- */

typedef struct {
  char a[64];
  char b[64];
  char c[64];
  char rhythm[64];
  char accompaniment[64];
  char approved[64];
  char unflagged[64];
  char rejected[64];
  char grandchild[64];
  char unrelated[64];
  char promoted[64];
  char promoted_child[64];
  char toned[64];
} Motifs;

static bool take(char *out, char *id) {
  bool ok = id != NULL;
  if (id) {
    snprintf(out, 64, "%s", id);
  }
  free(id);
  return ok;
}

/* The motifs and their saved forms: A and B are the slot motives, C is another, and A has four forms. */
static bool make_motifs(Motifs *m) {
  bool ok = true;
  ok = take(m->a, create_motif("A", "full", "melodic", "C4:e Eb4:e G4:e Eb4:e C4:e Eb4:e G4:e C5:e", "null")) && ok;
  ok = take(m->b, create_motif("B", "full", "melodic", "G4:e F4:e Eb4:e D4:e F4:e Eb4:e D4:e C4:e", "null")) && ok;
  ok = take(m->c, create_motif("C", "full", "melodic", "C4:q Eb4:e G4:e C5:h", "null")) && ok;
  ok = take(m->rhythm, create_motif("Rhythm", "rhythm", "melodic", "x:q x:e x:e x:h", "null")) && ok;
  ok = take(m->accompaniment, create_motif("Acc", "full", "accompaniment", "C3:q G3:q Eb3:q G3:q", "null")) && ok;
  ok = take(m->approved, create_derived("A up", m->a, "approved", "D4:e F4:e A4:e F4:e D4:e F4:e A4:e D5:e")) && ok;
  ok = take(m->unflagged, create_derived("A up 2", m->a, "unflagged", "Eb4:e G4:e Bb4:e G4:e Eb4:e G4:e Bb4:e Eb5:e")) && ok;
  ok = take(m->rejected, create_derived("A up 3", m->a, "rejected", "F4:e A4:e C5:e A4:e F4:e A4:e C5:e F5:e")) && ok;
  ok = take(m->grandchild,
            create_derived("A up up", m->approved, "unflagged", "Eb4:e G4:e C5:e G4:e Eb4:e G4:e C5:e Eb5:e")) && ok;
  ok = take(m->unrelated, create_derived("C up", m->c, "approved", "D4:q F4:e A4:e D5:h")) && ok;
  ok = take(m->promoted, create_promoted("A own", m->a, "F4:e A4:e C5:e A4:e F4:e A4:e C5:e A5:e")) && ok;
  ok = take(m->promoted_child,
            create_derived("A own up", m->promoted, "approved", "G4:e Bb4:e D5:e Bb4:e G4:e Bb4:e D5:e G5:e")) && ok;
  /* A motif with its own harmony and a tone override. */
  ok = take(m->toned,
            create_item_plain("motif", "Toned",
                             "{\"uses\":[],\"motif\":{\"role\":\"melodic\",\"form\":\"full\",\"notes\":\"C4:e Eb4:e G4:e Eb4:e C4:e "
                             "Eb4:e G4:e C5:e\",\"meter\":\"4/4\",\"key\":\"C minor\",\"pickup\":\"0\",\"harmony\":\"I\","
                             "\"tone_overrides\":{\"2\":\"passing\"},\"features\":[],\"analysis\":null,\"derived\":null}}")) && ok;
  return ok;
}

/* The bodies the validator takes and refuses, through the create route. */
static void test_bodies(const Motifs *m) {
  int status = 0;
  int read_status = 0;
  char slots[600];
  char basic[300];
  char body[2400];
  char more[1000];
  char *id = NULL;

  id = create_item("theme", "Placeholder", "{\"uses\":[],\"theme\":{\"form\":null,\"slots\":[]}}", &status);
  check(status == 200 && id, "the old placeholder theme is still valid");
  free(id);
  id = create_item("theme", "Placeholder 2",
                   "{\"uses\":[],\"theme\":{\"form\":\"sentence\",\"slots\":[\"anything\",{\"x\":1}]}}", &status);
  check(status == 200 && id, "an old theme with a string form and slots of anything is valid");
  free(id);

  slots_of(m->a, m->b, slots, sizeof(slots));
  basic_slot_of(m->a, basic, sizeof(basic));
  theme_body(slots,
             ",\"accompaniment\":{\"pattern\":\"alberti\"},\"brief\":{\"scopes\":[{\"kind\":\"all\"}],"
             "\"piece_ids\":[],\"style_ids\":[]},\"style_weight\":\"1/2\",\"without\":[\"motive-rhythm\"],\"tempo\":120,"
             "\"check\":true,\"seed\":5,\"result\":null",
             body, sizeof(body));
  id = create_item("theme", "Full", body, &status);
  check(status == 200 && id, "a theme with every member is valid");
  if (id) {
    cJSON *answer = get_item(id, &read_status);
    const cJSON *uses = member(member(answer, "body"), "uses");
    check(cJSON_GetArraySize(uses) == 2 && strcmp(string_of(cJSON_GetArrayItem(uses, 0), "item_id"), m->a) == 0 &&
              strcmp(string_of(cJSON_GetArrayItem(uses, 1), "item_id"), m->b) == 0,
          "the uses are filled from the slots");
    check(strcmp(string_of(member(member(cJSON_GetArrayItem(uses, 0), "copy"), "motif"), "degrees"), kDegreesA) == 0,
          "each use carries a copy of its motif");
    cJSON_Delete(answer);
  }
  free(id);

  /* A client's uses of the slot items are replaced; its other uses stay. */
  snprintf(body, sizeof(body),
           "{\"uses\":[{\"item_id\":\"%s\",\"version\":1,\"copy\":{\"junk\":1}},{\"item_id\":\"%s\",\"version\":1}],"
           "\"theme\":{\"form\":{\"into\":\"sentence\",\"mode\":\"minor\",\"meter\":\"4/4\"},\"slots\":%s}}",
           m->a, m->c, slots);
  id = create_item("theme", "Replaced", body, &status);
  check(status == 200 && id, "a theme with a client use of a slot item is valid");
  if (id) {
    cJSON *answer = get_item(id, &read_status);
    const cJSON *uses = member(member(answer, "body"), "uses");
    check(cJSON_GetArraySize(uses) == 3 && strcmp(string_of(cJSON_GetArrayItem(uses, 0), "item_id"), m->c) == 0,
          "the client's other use stays and the slot uses follow it");
    bool junk_gone = true;
    const cJSON *use = NULL;
    cJSON_ArrayForEach(use, uses) {
      junk_gone = junk_gone && !member(member(use, "copy"), "junk");
    }
    check(junk_gone, "a client copy of a slot item is replaced");
    cJSON_Delete(answer);
  }
  free(id);

  /* Slots. */
  theme_body("[{\"slot\":\"cadential\",\"item_id\":\"mot-0123456789abcdef\",\"version\":1}]", "", body, sizeof(body));
  free(create_item("theme", "x", body, &status));
  check(status == 400, "a theme without a basic slot is refused");
  snprintf(more, sizeof(more),
           "[{\"slot\":\"basic\",\"item_id\":\"%s\",\"version\":1},{\"slot\":\"basic\",\"item_id\":\"%s\",\"version\":1}]",
           m->a, m->b);
  theme_body(more, "", body, sizeof(body));
  free(create_item("theme", "x", body, &status));
  check(status == 400, "a slot named twice is refused");
  snprintf(more, sizeof(more), "[{\"slot\":\"intro\",\"item_id\":\"%s\",\"version\":1}]", m->a);
  theme_body(more, "", body, sizeof(body));
  free(create_item("theme", "x", body, &status));
  check(status == 400, "an unknown slot is refused");
  theme_body("[{\"slot\":\"basic\",\"item_id\":\"mot-0123456789abcdef\",\"version\":1}]", "", body, sizeof(body));
  free(create_item("theme", "x", body, &status));
  check(status == 400, "a slot naming an item that does not exist is refused");
  snprintf(more, sizeof(more), "[{\"slot\":\"basic\",\"item_id\":\"%s\",\"version\":2}]", m->a);
  theme_body(more, "", body, sizeof(body));
  free(create_item("theme", "x", body, &status));
  check(status == 400, "a slot naming a version that does not exist is refused");
  basic_slot_of(m->rhythm, more, sizeof(more));
  theme_body(more, "", body, sizeof(body));
  free(create_item("theme", "x", body, &status));
  check(status == 400, "a slot naming a rhythm motif is refused");
  basic_slot_of(m->accompaniment, more, sizeof(more));
  theme_body(more, "", body, sizeof(body));
  free(create_item("theme", "x", body, &status));
  check(status == 400, "a slot naming an accompaniment motif is refused");

  /* The accompaniment. */
  snprintf(more, sizeof(more), ",\"accompaniment\":{\"pattern\":\"motive\",\"item_id\":\"%s\",\"version\":1}",
           m->accompaniment);
  theme_body(basic, more, body, sizeof(body));
  id = create_item("theme", "Acc", body, &status);
  check(status == 200 && id, "an accompaniment motif is valid");
  if (id) {
    cJSON *answer = get_item(id, &read_status);
    const cJSON *uses = member(member(answer, "body"), "uses");
    check(cJSON_GetArraySize(uses) == 2 && strcmp(string_of(cJSON_GetArrayItem(uses, 1), "item_id"), m->accompaniment) == 0,
          "the accompaniment motif is a use");
    cJSON_Delete(answer);
  }
  free(id);
  snprintf(more, sizeof(more), ",\"accompaniment\":{\"pattern\":\"motive\",\"item_id\":\"%s\",\"version\":1}", m->b);
  theme_body(basic, more, body, sizeof(body));
  free(create_item("theme", "x", body, &status));
  check(status == 400, "a melodic motif as the accompaniment is refused");
  theme_body(basic, ",\"accompaniment\":{\"pattern\":\"waltz\"}", body, sizeof(body));
  free(create_item("theme", "x", body, &status));
  check(status == 400, "an unknown accompaniment pattern is refused");

  /* The other members. */
  static const char *const bad[] = {",\"pool\":\"approved\"",
                                    ",\"brief\":{}",
                                    ",\"brief\":{\"scopes\":[{\"kind\":\"galaxy\"}]}",
                                    ",\"brief\":{\"piece_ids\":[\"../x\"]}",
                                    ",\"style_weight\":\"half\"",
                                    ",\"without\":[\"mode\"]",
                                    ",\"without\":[\"motive-rhythm\",\"motive-degrees\",\"motive-intervals\","
                                    "\"motive-nct-figure\",\"motive-rhythm\"]",
                                    ",\"check\":\"yes\"",
                                    ",\"seed\":-1",
                                    ",\"seed\":1.5",
                                    ",\"result\":\"done\""};
  static const char *const names[] = {"a pool (themes have no pool)",
                                      "an empty brief",
                                      "a brief scope of an unknown kind",
                                      "a brief piece id that is not an id",
                                      "a style weight that is not n/d",
                                      "a without that is not a motive category",
                                      "a without of five",
                                      "a check that is not a boolean",
                                      "a negative seed",
                                      "a seed that is not whole",
                                      "a result that is not an object"};
  for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
    theme_body(basic, bad[i], body, sizeof(body));
    free(create_item("theme", "x", body, &status));
    char what[160];
    snprintf(what, sizeof(what), "%s is refused", names[i]);
    check(status == 400, what);
  }
  snprintf(body, sizeof(body),
           "{\"uses\":[],\"theme\":{\"form\":{\"into\":\"sentence\",\"key\":\"D\",\"mode\":\"minor\"},\"slots\":%s}}", basic);
  free(create_item("theme", "x", body, &status));
  check(status == 400, "a form with a key is refused (themes are in C)");
  snprintf(body, sizeof(body),
           "{\"uses\":[],\"theme\":{\"form\":{\"into\":\"sentence\",\"key\":null},\"slots\":%s}}", basic);
  free(create_item("theme", "x", body, &status));
  check(status == 400, "a form with a null key is refused too");

  /* The same motif in two slots, ideas and chords. */
  snprintf(more, sizeof(more),
           "[{\"slot\":\"basic\",\"item_id\":\"%s\",\"version\":1},{\"slot\":\"cadential\",\"item_id\":\"%s\",\"version\":1}]",
           m->a, m->a);
  theme_body(more, "", body, sizeof(body));
  id = create_item("theme", "Same motif", body, &status);
  check(status == 200 && id, "the same motif in two slots is valid");
  free(id);
  snprintf(more, sizeof(more), ",\"ideas\":[{\"idea\":\"repetition\",\"item_id\":\"%s\",\"version\":1},"
           "{\"idea\":\"fragment 1\",\"item_id\":\"%s\",\"version\":1}],\"chords\":[{\"bar\":0,\"text\":\"I\"},"
           "{\"bar\":7,\"text\":\"V I@2\"}]", m->b, m->c);
  theme_body(basic, more, body, sizeof(body));
  id = create_item("theme", "Ideas and chords", body, &status);
  check(status == 200 && id, "ideas and chords are valid");
  if (id) {
    cJSON *answer = get_item(id, &read_status);
    const cJSON *uses = member(member(answer, "body"), "uses");
    check(cJSON_GetArraySize(uses) == 3, "the idea motifs are uses too");
    cJSON_Delete(answer);
  }
  free(id);
  char idea_item[160];
  snprintf(idea_item, sizeof(idea_item), "\"item_id\":\"%s\",\"version\":1", m->b);
  static const char *const bad_v2_names[] = {"an idea of an unknown name", "an idea named twice", "an idea with no item",
                                             "a chord bar of 8", "a chord bar named twice", "a chord text over 64 characters",
                                             "a chord bar that is not whole", "ideas that are not a list"};
  char bad_v2[8][700];
  snprintf(bad_v2[0], sizeof(bad_v2[0]), ",\"ideas\":[{\"idea\":\"bridge\",%s}]", idea_item);
  snprintf(bad_v2[1], sizeof(bad_v2[1]), ",\"ideas\":[{\"idea\":\"repetition\",%s},{\"idea\":\"repetition\",%s}]", idea_item,
           idea_item);
  snprintf(bad_v2[2], sizeof(bad_v2[2]), ",\"ideas\":[{\"idea\":\"repetition\"}]");
  snprintf(bad_v2[3], sizeof(bad_v2[3]), ",\"chords\":[{\"bar\":8,\"text\":\"I\"}]");
  snprintf(bad_v2[4], sizeof(bad_v2[4]), ",\"chords\":[{\"bar\":1,\"text\":\"I\"},{\"bar\":1,\"text\":\"V\"}]");
  snprintf(bad_v2[5], sizeof(bad_v2[5]), ",\"chords\":[{\"bar\":1,\"text\":\"%065d\"}]", 0);
  snprintf(bad_v2[6], sizeof(bad_v2[6]), ",\"chords\":[{\"bar\":1.5,\"text\":\"I\"}]");
  snprintf(bad_v2[7], sizeof(bad_v2[7]), ",\"ideas\":{\"repetition\":\"%s\"}", m->b);
  for (size_t i = 0; i < 8; i++) {
    theme_body(basic, bad_v2[i], body, sizeof(body));
    free(create_item("theme", "x", body, &status));
    char what[160];
    snprintf(what, sizeof(what), "%s is refused", bad_v2_names[i]);
    check(status == 400, what);
  }
  snprintf(body, sizeof(body), "{\"uses\":[],\"theme\":{\"form\":{\"into\":\"sentence\",\"meter\":\"5/4\"},\"slots\":%s}}",
           basic);
  free(create_item("theme", "x", body, &status));
  check(status == 200, "a form meter outside the four is saved (the Composer refuses it when composing)");
  snprintf(body, sizeof(body), "{\"uses\":[],\"theme\":{\"form\":{\"into\":\"hybrid\",\"hybrid\":\"fugue\"},\"slots\":%s}}",
           basic);
  free(create_item("theme", "x", body, &status));
  check(status == 200, "a hybrid pairing that is not one is saved (the Composer refuses it when composing)");
}

/* The request the compose route writes, the version it makes and its files. */
static void test_compose(const Motifs *m) {
  int status = 0;
  char slots[600];
  char basic[300];
  char body[2400];
  char more[1000];
  char dir[200];
  char path[400];

  slots_of(m->a, m->b, slots, sizeof(slots));
  basic_slot_of(m->a, basic, sizeof(basic));
  theme_body(slots, ",\"without\":[\"motive-rhythm\"],\"check\":true", body, sizeof(body));
  char *theme = create_item("theme", "Composed", body, &status);
  check(status == 200 && theme, "the theme to compose is saved");
  if (!theme) {
    return;
  }

  cJSON *answer = compose(theme, "{\"seed\":97}", &status);
  check(status == 200 && ok_true(answer), "compose answers 200");
  check(number_of(member(answer, "item"), "current_version") == 2, "compose makes version 2");
  cJSON_Delete(answer);

  version_dir(theme, 2, dir, sizeof(dir));
  static const char *const files[] = {"request.json", "plan.json", "theme.mid", "theme.musicxml"};
  bool all = true;
  for (size_t i = 0; i < 4; i++) {
    snprintf(path, sizeof(path), "%s/%s", dir, files[i]);
    all = all && file_exists(path);
  }
  check(all, "the version folder holds the request, the plan, the MIDI and the MusicXML");
  snprintf(path, sizeof(path), "%s/brief.style.json", dir);
  check(!file_exists(path), "a theme without a brief has no style file");

  answer = get_item(theme, &status);
  const cJSON *result = member(theme_of(answer), "result");
  /* With check on, the Composer moves to the next seed when a theme fails its checks; the result holds the seed it kept. */
  check(number_of(result, "seed") >= 97 && number_of(result, "seed") <= 105, "the result keeps the seed the Composer kept");
  check(cJSON_IsObject(member(result, "summary")), "the result keeps the command's summary");
  check(number_of(member(result, "brief"), "piece_count") == 0 && cJSON_IsArray(member(member(result, "brief"), "skipped")),
        "the result's brief is empty without one");
  check(cJSON_GetArraySize(member(result, "files")) == 4, "the result names the four files");
  const cJSON *versions = member(member(answer, "item"), "versions");
  check(strcmp(string_of(cJSON_GetArrayItem(versions, 1), "note"), "Composed") == 0, "the note is Composed");
  check(cJSON_GetArraySize(member(member(answer, "body"), "uses")) == 2, "the new version keeps its uses");
  cJSON_Delete(answer);

  /* Version 1 is untouched and has no result. */
  snprintf(path, sizeof(path), "/api/materials/items/%s/versions/1", theme);
  answer = exchange("GET", path, NULL, &status);
  const cJSON *first_result = member(theme_of(answer), "result");
  check(status == 200 && (!first_result || cJSON_IsNull(first_result)), "version 1 has no result");
  cJSON_Delete(answer);

  /* The request (version 2): in C, the motives by id, the slots, the ideas and the chords. */
  cJSON *request = last_request();
  check(strcmp(string_of(request, "schema"), "ursatz-theme-request") == 0 && number_of(request, "schema_version") == 2,
        "the request has the theme request schema");
  check(strcmp(string_of(request, "into"), "sentence") == 0 && strcmp(string_of(request, "key"), "C") == 0 &&
            strcmp(string_of(request, "mode"), "minor") == 0 && number_of(request, "bars") == 8,
        "the request has the form");
  const cJSON *slot_names = member(request, "slots");
  check(strcmp(string_of(slot_names, "basic"), "M1") == 0 && strcmp(string_of(slot_names, "cadential"), "M2") == 0 &&
            cJSON_IsNull(member(slot_names, "contrasting")),
        "the request names the slot motives");
  const cJSON *motive = cJSON_GetArrayItem(member(request, "motives"), 0);
  check(strcmp(string_of(motive, "degrees"), kDegreesA) == 0 && strcmp(string_of(motive, "pickup"), "0") == 0 &&
            strcmp(string_of(motive, "role"), "melodic") == 0,
        "a motive is the copy's degrees, pickup and role");
  check(!member(motive, "forms") && !member(motive, "item"), "a motive has no forms");
  check(cJSON_IsObject(member(request, "ideas")) && member(request, "ideas")->child == NULL &&
            cJSON_IsArray(member(request, "chords")) && cJSON_GetArraySize(member(request, "chords")) == 0,
        "a theme without ideas or chords has them empty");
  check(cJSON_IsNull(member(request, "accompaniment")) && !member(request, "style"),
        "the request has no accompaniment and no style");
  cJSON_Delete(request);

  /* The same motif twice is one motive; ideas, chords, the base harmony and the tone overrides go in. */
  snprintf(more, sizeof(more),
           ",\"ideas\":[{\"idea\":\"repetition\",\"item_id\":\"%s\",\"version\":1},{\"idea\":\"fragment 1\",\"item_id\":\"%s\","
           "\"version\":1}],\"chords\":[{\"bar\":0,\"text\":\"I\"},{\"bar\":1,\"text\":\"IV V@2\"}]",
           m->toned, m->a);
  snprintf(basic, sizeof(basic), "[{\"slot\":\"basic\",\"item_id\":\"%s\",\"version\":1},{\"slot\":\"cadential\","
           "\"item_id\":\"%s\",\"version\":1}]", m->a, m->a);
  theme_body(basic, more, body, sizeof(body));
  char *repeated = create_item("theme", "Repeated", body, &status);
  check(status == 200 && repeated, "a theme with a repeated motif, ideas and chords is saved");
  free(compose(repeated, "{}", &status));
  check(status == 200, "a theme with ideas and chords is composed");
  request = last_request();
  const cJSON *repeated_slots = member(request, "slots");
  check(cJSON_GetArraySize(member(request, "motives")) == 2, "the request has one motive for each distinct motif");
  check(strcmp(string_of(repeated_slots, "basic"), "M1") == 0 && strcmp(string_of(repeated_slots, "cadential"), "M1") == 0,
        "two slots of one motif share a motive");
  const cJSON *repeated_ideas = member(request, "ideas");
  check(strcmp(string_of(repeated_ideas, "fragment 1"), "M1") == 0 && strcmp(string_of(repeated_ideas, "repetition"), "M2") == 0,
        "the ideas name their motives");
  const cJSON *chords = member(request, "chords");
  check(cJSON_GetArraySize(chords) == 2 && number_of(cJSON_GetArrayItem(chords, 1), "bar") == 1 &&
            strcmp(string_of(cJSON_GetArrayItem(chords, 1), "text"), "IV V@2") == 0,
        "the chords are copied");
  const cJSON *plain_motive = cJSON_GetArrayItem(member(request, "motives"), 0);
  const cJSON *toned_motive = cJSON_GetArrayItem(member(request, "motives"), 1);
  check(strcmp(string_of(toned_motive, "harmony"), "I") == 0 &&
            strcmp(string_of(member(toned_motive, "tone_overrides"), "2"), "passing") == 0,
        "a motive has its own harmony and its tone overrides");
  check(!member(plain_motive, "tone_overrides"), "a motive without overrides sends none");
  cJSON *plain_item = get_item(m->a, &status);
  const cJSON *proposed =
      member(member(member(member(member(member(plain_item, "body"), "motif"), "analysis"), "analysis"), "base_harmony"), "text");
  const cJSON *sent = member(plain_motive, "harmony");
  check(cJSON_IsString(proposed) && proposed->valuestring[0] != '\0' ? cJSON_IsString(sent) && strcmp(sent->valuestring, proposed->valuestring) == 0
                                                                      : cJSON_IsNull(sent),
        "a motive without its own harmony sends the base harmony of its analysis");
  cJSON_Delete(plain_item);
  cJSON_Delete(request);
  free(repeated);

  /* Notes: a version needs one, the first is Created and compose's is Composed. */
  answer = get_item(theme, &status);
  check(strcmp(string_of(cJSON_GetArrayItem(member(member(answer, "item"), "versions"), 0), "note"), "Created") == 0,
        "the first version of a theme is noted Created");
  cJSON_Delete(answer);
  snprintf(path, sizeof(path), "/api/materials/items/%s/versions", theme);
  char wrapped[2600];
  snprintf(wrapped, sizeof(wrapped), "{\"body\":%s}", body);
  answer = exchange("POST", path, wrapped, &status);
  check(status == 400, "a theme version with no note is refused");
  cJSON_Delete(answer);
  snprintf(wrapped, sizeof(wrapped), "{\"note\":\"  \",\"body\":%s}", body);
  answer = exchange("POST", path, wrapped, &status);
  check(status == 400, "a theme version with a blank note is refused");
  cJSON_Delete(answer);
  snprintf(wrapped, sizeof(wrapped), "{\"note\":\"Added ideas\",\"body\":%s}", body);
  answer = exchange("POST", path, wrapped, &status);
  check(status == 200, "a theme version with a note is saved");
  cJSON_Delete(answer);

  /* An accompaniment motive and a pattern. */
  snprintf(more, sizeof(more), ",\"accompaniment\":{\"pattern\":\"motive\",\"item_id\":\"%s\",\"version\":1}",
           m->accompaniment);
  theme_body(basic, more, body, sizeof(body));
  char *with_motive = create_item("theme", "Acc motive", body, &status);
  free(compose(with_motive, "{}", &status));
  request = last_request();
  const cJSON *accompaniment = member(request, "accompaniment");
  const cJSON *accompaniment_motive = member(accompaniment, "motive");
  check(status == 200 && strcmp(string_of(accompaniment, "pattern"), "motive") == 0 &&
            strcmp(string_of(accompaniment_motive, "role"), "accompaniment") == 0 &&
            strcmp(string_of(accompaniment_motive, "form"), "full") == 0 &&
            strlen(string_of(accompaniment_motive, "degrees")) > 0,
        "an accompaniment motive goes in as the pattern");
  cJSON_Delete(request);
  free(with_motive);
  theme_body(basic, ",\"accompaniment\":{\"pattern\":\"oom-pah\"}", body, sizeof(body));
  char *with_pattern = create_item("theme", "Acc pattern", body, &status);
  free(compose(with_pattern, "{}", &status));
  request = last_request();
  check(status == 200 && strcmp(string_of(member(request, "accompaniment"), "pattern"), "oom-pah") == 0,
        "an accompaniment pattern goes in");
  cJSON_Delete(request);
  free(with_pattern);

  /* The seed in the body is used when the route gives none. */
  theme_body(basic, ",\"seed\":33", body, sizeof(body));
  char *seeded = create_item("theme", "Seeded", body, &status);
  answer = compose(seeded, "{}", &status);
  check(status == 200, "a theme with a seed in its body is composed");
  cJSON_Delete(answer);
  answer = get_item(seeded, &status);
  check(number_of(member(theme_of(answer), "result"), "seed") == 33, "the body's seed is used");
  cJSON_Delete(answer);
  free(seeded);

  /* Bad compose requests. */
  answer = compose(theme, "{\"seed\":-1}", &status);
  check(status == 400, "a negative seed is refused");
  cJSON_Delete(answer);
  answer = compose(theme, "{\"seed\":\"x\"}", &status);
  check(status == 400, "a seed that is not a number is refused");
  cJSON_Delete(answer);
  answer = compose(theme, "{\"update_uses\":3}", &status);
  check(status == 400, "update_uses that is not a boolean is refused");
  cJSON_Delete(answer);
  answer = compose("thm-0123456789abcdef", "{}", &status);
  check(status == 404, "composing an item that does not exist is 404");
  cJSON_Delete(answer);
  answer = compose(m->a, "{}", &status);
  check(status == 400, "composing a motif is refused");
  cJSON_Delete(answer);
  char *placeholder = create_item("theme", "Empty", "{\"uses\":[],\"theme\":{\"form\":null,\"slots\":[]}}", &status);
  answer = compose(placeholder, "{}", &status);
  check(status == 400 && current_version(placeholder) == 1, "composing a placeholder theme is refused and writes nothing");
  cJSON_Delete(answer);
  free(placeholder);
  snprintf(path, sizeof(path), "/api/materials/items/%s/compose", theme);
  answer = exchange("GET", path, NULL, &status);
  check(status == 405, "compose takes POST only");
  cJSON_Delete(answer);

  free(theme);
}

/* True when the folder of item id holds nothing but version files: no staging folder is left over from a run. */
static bool no_stage_left(const char *id) {
  char path[300];
  snprintf(path, sizeof(path), "materials/items/%s", id);
  DIR *folder = opendir(path);
  bool clean = folder != NULL;
  struct dirent *entry;
  while (folder && (entry = readdir(folder)) != NULL) {
    clean = clean && strstr(entry->d_name, ".stage-") == NULL;
  }
  if (folder) {
    closedir(folder);
  }
  return clean;
}

/* A run that fails writes no version and leaves no folder. */
static void test_failures(const Motifs *m) {
  int status = 0;
  char slots[600];
  char body[2400];
  char dir[200];
  slots_of(m->a, m->b, slots, sizeof(slots));
  theme_body(slots, "", body, sizeof(body));
  char *theme = create_item("theme", "Fails", body, &status);
  check(theme != NULL, "the theme for the failing runs is saved");
  if (!theme) {
    return;
  }
  version_dir(theme, 2, dir, sizeof(dir));

  /* A basic motive of nine bars in an 8-bar 4/4 theme fits no idea: the Composer refuses it at once as
     UNSATISFIABLE (the chord-bar refusal is covered by the Composer's own tests). */
  char *long_motif = create_motif("Long", "full", "melodic",
                                  "C4:w Eb4:w G4:w Eb4:w C4:w Eb4:w G4:w Eb4:w C5:w", "null");
  check(long_motif != NULL, "the motif longer than one bar is saved");
  slots_of(long_motif ? long_motif : m->a, m->b, slots, sizeof(slots));
  theme_body(slots, "", body, sizeof(body));
  char *refused = create_item("theme", "Refused", body, &status);
  char refused_dir[200];
  version_dir(refused, 2, refused_dir, sizeof(refused_dir));
  cJSON *answer = compose(refused, "{\"seed\":1}", &status);
  check(status == 422 && !ok_true(answer), "a request nothing satisfies is 422");
  check(strlen(string_of(answer, "error")) > 0, "the answer carries the Composer's message");
  cJSON_Delete(answer);
  check(current_version(refused) == 1 && !dir_exists(refused_dir) && no_stage_left(refused),
        "a refused run writes no version and leaves no folder");
  free(refused);
  free(long_motif);

  free(theme);
}

/* The files route serves the five names and nothing else. */
static void test_files(const Motifs *m) {
  int status = 0;
  char slots[600];
  char body[2400];
  char path[400];
  slots_of(m->a, m->b, slots, sizeof(slots));
  theme_body(slots, "", body, sizeof(body));
  char *theme = create_item("theme", "Files", body, &status);
  cJSON *answer = compose(theme, "{\"seed\":4}", &status);
  cJSON_Delete(answer);
  check(status == 200, "the theme for the files is composed");

  static const char *const names[] = {"request.json", "plan.json", "theme.mid", "theme.musicxml"};
  static const char *const types[] = {"application/json", "application/json", "audio/midi", "application/xml"};
  for (size_t i = 0; i < 4; i++) {
    snprintf(path, sizeof(path), "/api/materials/items/%s/versions/2/files/%s", theme, names[i]);
    char *response = http_exchange("GET", path, NULL, NULL, 0, NULL);
    char type[128];
    content_type_of(response, type, sizeof(type));
    char what[160];
    snprintf(what, sizeof(what), "%s is served as %s", names[i], types[i]);
    check(response_status(response) == 200 && strcmp(type, types[i]) == 0 && strlen(response_body(response)) > 0, what);
    free(response);
  }
  snprintf(path, sizeof(path), "/api/materials/items/%s/versions/2/files/theme.mid", theme);
  size_t size = 0;
  char *response = http_exchange("GET", path, NULL, NULL, 0, &size);
  check(size - (size_t)(response_body(response) - response) > 5 && strncmp(response_body(response), "MThd", 4) == 0,
        "the MIDI file is served whole");
  free(response);

  static const char *const refused[] = {"item.json", "brief.style.json", "..%2Fitem.json", "v2.json", "theme.MID"};
  for (size_t i = 0; i < sizeof(refused) / sizeof(refused[0]); i++) {
    snprintf(path, sizeof(path), "/api/materials/items/%s/versions/2/files/%s", theme, refused[i]);
    response = http_exchange("GET", path, NULL, NULL, 0, NULL);
    char what[160];
    snprintf(what, sizeof(what), "%s is not served", refused[i]);
    check(response_status(response) == 404, what);
    free(response);
  }
  snprintf(path, sizeof(path), "/api/materials/items/%s/versions/1/files/plan.json", theme);
  response = http_exchange("GET", path, NULL, NULL, 0, NULL);
  check(response_status(response) == 404, "a version without files is 404");
  free(response);
  snprintf(path, sizeof(path), "/api/materials/items/%s/versions/3/files/plan.json", theme);
  response = http_exchange("GET", path, NULL, NULL, 0, NULL);
  check(response_status(response) == 404, "a version that does not exist is 404");
  free(response);
  snprintf(path, sizeof(path), "/api/materials/items/%s/versions/x/files/plan.json", theme);
  response = http_exchange("GET", path, NULL, NULL, 0, NULL);
  check(response_status(response) == 404, "a version that is not a number is 404");
  free(response);
  response = http_exchange("GET", "/api/materials/items/thm-0123456789abcdef/versions/1/files/plan.json", NULL, NULL, 0, NULL);
  check(response_status(response) == 404, "an item that does not exist is 404");
  free(response);
  snprintf(path, sizeof(path), "/api/materials/items/%s/versions/2/files/plan.json", theme);
  response = http_exchange("POST", path, NULL, "{}", 2, NULL);
  check(response_status(response) == 405, "the files route takes GET only");
  free(response);

  /* Deleting the item removes its version folders. */
  char dir[200];
  snprintf(dir, sizeof(dir), "materials/items/%s", theme);
  check(dir_exists(dir), "the item directory exists before the delete");
  snprintf(path, sizeof(path), "/api/materials/items/%s", theme);
  answer = exchange("DELETE", path, NULL, &status);
  cJSON_Delete(answer);
  check(status == 200 && !dir_exists(dir), "deleting the item removes its directory and the version folders");
  free(theme);
}

/* A brief of Library pieces is built into the version folder; the style, weight and without reach the request. */
static void test_brief(const Motifs *m) {
  int status = 0;
  char slots[600];
  char body[2400];
  char path[400];
  char dir[200];
  char more[800];
  char *first = upload_piece("a.musicxml");
  char *second = upload_piece("b.musicxml");
  check(first && second, "two fixture pieces are uploaded");
  if (!first || !second) {
    free(first);
    free(second);
    return;
  }
  slots_of(m->a, m->b, slots, sizeof(slots));
  snprintf(more, sizeof(more),
           ",\"brief\":{\"scopes\":[],\"piece_ids\":[\"%s\",\"%s\",\"piece-0000000000000000\"],\"style_ids\":[]},"
           "\"style_weight\":\"1/4\",\"without\":[\"motive-rhythm\",\"motive-degrees\"],\"tempo\":132",
           first, second);
  theme_body(slots, more, body, sizeof(body));
  char *theme = create_item("theme", "Briefed", body, &status);
  check(status == 200 && theme, "a theme with a brief is saved");
  cJSON *answer = compose(theme, "{\"seed\":2}", &status);
  check(status == 200 && ok_true(answer), "a theme with a brief composes");
  cJSON_Delete(answer);

  version_dir(theme, 2, dir, sizeof(dir));
  snprintf(path, sizeof(path), "%s/brief.style.json", dir);
  check(file_exists(path), "the brief is built into the version folder");
  answer = get_item(theme, &status);
  const cJSON *result = member(theme_of(answer), "result");
  const cJSON *brief = member(result, "brief");
  check(number_of(brief, "piece_count") == 2, "the result counts the pieces used");
  const cJSON *skipped = member(brief, "skipped");
  check(cJSON_GetArraySize(skipped) == 1 && strcmp(string_of(cJSON_GetArrayItem(skipped, 0), "id"), "piece-0000000000000000") == 0,
        "the result lists the piece that was left out");
  check(cJSON_GetArraySize(member(result, "files")) == 5, "the result names the five files");
  cJSON_Delete(answer);

  cJSON *request = last_request();
  const char *style = string_of(request, "style");
  const char *suffix = "/v2/brief.style.json";
  size_t style_length = strlen(style);
  check(style[0] == '/' && style_length > strlen(suffix) && strcmp(style + style_length - strlen(suffix), suffix) == 0,
        "the request names the style file by an absolute path in the version folder");
  const cJSON *without = member(request, "without");
  check(strcmp(string_of(request, "style_weight"), "1/4") == 0 && cJSON_GetArraySize(without) == 2 &&
            strcmp(cJSON_GetArrayItem(without, 0)->valuestring, "motive-rhythm") == 0 && number_of(request, "tempo") == 132,
        "the style weight, the without list and the tempo pass through");
  cJSON_Delete(request);
  snprintf(path, sizeof(path), "/api/materials/items/%s/versions/2/files/brief.style.json", theme);
  char *response = http_exchange("GET", path, NULL, NULL, 0, NULL);
  cJSON *style_json = cJSON_Parse(response_body(response));
  check(response_status(response) == 200 && style_json, "the style file is served");
  cJSON_Delete(style_json);
  free(response);
  free(theme);

  /* A brief with nothing the Composer can use is 422 and leaves nothing. */
  snprintf(more, sizeof(more), ",\"brief\":{\"scopes\":[],\"piece_ids\":[\"piece-0000000000000000\"],\"style_ids\":[]}");
  theme_body(slots, more, body, sizeof(body));
  theme = create_item("theme", "Empty brief", body, &status);
  answer = compose(theme, "{}", &status);
  check(status == 422, "a brief with no usable piece is 422");
  cJSON_Delete(answer);
  version_dir(theme, 2, dir, sizeof(dir));
  check(current_version(theme) == 1 && !dir_exists(dir), "an unusable brief writes no version and leaves no folder");
  free(theme);
  free(first);
  free(second);
}

/* update_uses moves an updated use to its source's current version before the compose. */
static void test_update_uses(const Motifs *m) {
  int status = 0;
  char slots[600];
  char body[2400];
  char path[400];
  slots_of(m->a, m->b, slots, sizeof(slots));
  theme_body(slots, "", body, sizeof(body));
  char *theme = create_item("theme", "Updating", body, &status);

  /* A new version of motif A, analyzed by the real Composer. */
  snprintf(path, sizeof(path), "/api/materials/items/%s/versions", m->a);
  cJSON *answer = exchange("POST", path,
                           "{\"note\":\"changed\",\"body\":{\"uses\":[],\"motif\":{\"role\":\"melodic\",\"form\":\"full\","
                           "\"notes\":\"C4:e F4:e G4:e F4:e C4:e F4:e G4:e C5:e\",\"meter\":\"4/4\",\"key\":\"C minor\","
                           "\"pickup\":\"0\",\"features\":[],\"analysis\":null,\"derived\":null}}}",
                           &status);
  check(status == 200 && number_of(member(answer, "item"), "current_version") == 2, "motif A has a second version");
  cJSON_Delete(answer);

  answer = get_item(theme, &status);
  bool updated = false;
  const cJSON *entry = NULL;
  cJSON_ArrayForEach(entry, member(answer, "uses_status")) {
    updated = updated || (strcmp(string_of(entry, "item_id"), m->a) == 0 && cJSON_IsTrue(member(entry, "updated")));
  }
  check(updated, "the theme reports its use of A as updated");
  cJSON_Delete(answer);

  /* Without update_uses the compose reads the copy the theme has. */
  answer = compose(theme, "{}", &status);
  cJSON_Delete(answer);
  cJSON *request = last_request();
  const cJSON *motive = cJSON_GetArrayItem(member(request, "motives"), 0);
  check(status == 200 && strcmp(string_of(motive, "degrees"), kDegreesA) == 0 && current_version(theme) == 2,
        "a compose without update_uses reads the copy it has");
  cJSON_Delete(request);

  /* With it the use moves first (a version of its own), then the compose runs on the new copy. */
  answer = compose(theme, "{\"update_uses\":true}", &status);
  check(status == 200 && number_of(member(answer, "item"), "current_version") == 4,
        "update_uses writes the update and the composed version");
  cJSON_Delete(answer);
  request = last_request();
  motive = cJSON_GetArrayItem(member(request, "motives"), 0);
  check(strcmp(string_of(motive, "degrees"), "1:e 4:e 5:e 4:e 1:e 4:e 5:e 1':e") == 0, "the request is built from the updated copy");
  cJSON_Delete(request);
  answer = get_item(theme, &status);
  const cJSON *slots_json = member(theme_of(answer), "slots");
  const cJSON *uses = member(member(answer, "body"), "uses");
  check(number_of(cJSON_GetArrayItem(slots_json, 0), "version") == 2 && number_of(cJSON_GetArrayItem(uses, 0), "version") == 2,
        "the slot and the use name version 2 of A");
  const cJSON *versions = member(member(answer, "item"), "versions");
  check(strncmp(string_of(cJSON_GetArrayItem(versions, 2), "note"), "updated A to v2", 15) == 0,
        "the update has its own version and note");
  cJSON_Delete(answer);

  /* Nothing to update: the compose runs once. */
  answer = compose(theme, "{\"update_uses\":true}", &status);
  check(status == 200 && number_of(member(answer, "item"), "current_version") == 5,
        "update_uses with nothing updated writes only the composed version");
  cJSON_Delete(answer);
  free(theme);
}

/* Two composes of one theme may run at once; the staging folders keep both versions apart. */
static void test_concurrent(const Motifs *m) {
  int status = 0;
  int status_a = 0;
  int status_b = 0;
  char slots[600];
  char body[2400];
  char path[400];
  slots_of(m->a, m->b, slots, sizeof(slots));
  theme_body(slots, "", body, sizeof(body));
  char *theme = create_item("theme", "Concurrent", body, &status);
  check(theme != NULL, "the theme for two composes at once is saved");
  if (!theme) {
    return;
  }
  snprintf(path, sizeof(path), "/api/materials/items/%s/compose", theme);
  cJSON *first = exchange("POST", path, "{\"seed\":11}", &status_a);
  cJSON *second = exchange("POST", path, "{\"seed\":12}", &status_b);
  check(status_a == 202 && status_b == 202, "both composes are accepted as jobs at once");
  first = finish_job(first, &status_a);
  second = finish_job(second, &status_b);
  check(status_a == 200 && status_b == 200, "both composes finish");
  cJSON_Delete(first);
  cJSON_Delete(second);
  check(current_version(theme) == 3, "both versions are kept (version 3)");
  bool all = true;
  for (int version = 2; version <= 3; version++) {
    static const char *const files[] = {"request.json", "plan.json", "theme.mid", "theme.musicxml"};
    for (size_t i = 0; i < 4; i++) {
      snprintf(path, sizeof(path), "materials/items/%s/v%d/%s", theme, version, files[i]);
      all = all && file_exists(path);
    }
  }
  check(all, "each version has its four files");
  snprintf(path, sizeof(path), "materials/items/%s", theme);
  DIR *folder = opendir(path);
  bool clean = folder != NULL;
  struct dirent *entry;
  while (folder && (entry = readdir(folder)) != NULL) {
    clean = clean && strstr(entry->d_name, ".stage-") == NULL;
  }
  if (folder) {
    closedir(folder);
  }
  check(clean, "no staging folder is left");
  free(theme);
}

/* GET and DELETE /api/jobs/<id>: the state of a job, its answer, and the cancel of one that is over. */
static void test_jobs_route(const Motifs *m) {
  int status = 0;
  char slots[600];
  char body[2400];
  char path[400];
  slots_of(m->a, m->b, slots, sizeof(slots));
  theme_body(slots, "", body, sizeof(body));
  char *theme = create_item("theme", "Jobs", body, &status);
  snprintf(path, sizeof(path), "/api/materials/items/%s/compose", theme);
  cJSON *accepted = exchange("POST", path, "{\"seed\":3}", &status);
  const cJSON *job = member(accepted, "job");
  check(status == 202 && ok_true(accepted) && strncmp(string_of(job, "id"), "job-", 4) == 0 &&
            strcmp(string_of(job, "kind"), "theme") == 0 && strcmp(string_of(job, "state"), "queued") == 0,
        "a compose answers 202 with the job's id, kind and state");
  char id[64];
  snprintf(id, sizeof(id), "%s", string_of(job, "id"));
  cJSON_Delete(accepted);
  snprintf(path, sizeof(path), "/api/jobs/%s", id);
  cJSON *state = NULL;
  for (int i = 0; i < 6000; i++) {
    cJSON_Delete(state);
    state = exchange("GET", path, NULL, &status);
    const char *now = string_of(member(state, "job"), "state");
    if (strcmp(now, "queued") != 0 && strcmp(now, "running") != 0) {
      break;
    }
    sleep_ms(50);
  }
  const cJSON *finished = member(state, "job");
  check(status == 200 && strcmp(string_of(finished, "state"), "done") == 0 && number_of(finished, "http_status") == 200 &&
            ok_true(member(finished, "result")) && cJSON_IsObject(member(member(finished, "result"), "item")),
        "a finished job has its state, its status and the answer of the route");
  check(cJSON_IsString(member(finished, "stage")) && cJSON_IsNumber(member(finished, "done")) && cJSON_IsNumber(member(finished, "total")),
        "a job has its stage, done and total");
  cJSON_Delete(state);
  state = exchange("DELETE", path, NULL, &status);
  check(status == 204, "a cancel of a finished job is 204");
  cJSON_Delete(state);
  state = exchange("DELETE", "/api/jobs/job-nothere", NULL, &status);
  check(status == 404, "a cancel of an unknown job is 404");
  cJSON_Delete(state);
  state = exchange("GET", "/api/jobs/job-nothere", NULL, &status);
  check(status == 404 && !ok_true(state), "an unknown job is 404");
  cJSON_Delete(state);
  state = exchange("POST", path, "{}", &status);
  check(status == 405, "a job takes GET and DELETE only");
  cJSON_Delete(state);
  free(theme);
}

/* The real Composer: two eighth-note motives and a small brief of fixture pieces. */
static void test_real_composer(const Motifs *m) {
  int status = 0;
  char slots[600];
  char body[2400];
  char path[400];
  char dir[200];
  char more[600];
  char *first = upload_piece("a.musicxml");
  char *second = upload_piece("b.musicxml");
  slots_of(m->a, m->b, slots, sizeof(slots));
  snprintf(more, sizeof(more),
           ",\"brief\":{\"scopes\":[],\"piece_ids\":[\"%s\",\"%s\"],\"style_ids\":[]},\"style_weight\":\"1/2\"",
           first ? first : "", second ? second : "");
  theme_body(slots, more, body, sizeof(body));
  char *theme = create_item("theme", "Real", body, &status);
  cJSON *answer = compose(theme, "{\"seed\":97}", &status);
  check(status == 200 && ok_true(answer), "the real Composer composes the theme (exit 0)");
  cJSON_Delete(answer);
  version_dir(theme, 2, dir, sizeof(dir));
  static const char *const files[] = {"request.json", "brief.style.json", "plan.json", "theme.mid", "theme.musicxml"};
  bool all = true;
  for (size_t i = 0; i < 5; i++) {
    snprintf(path, sizeof(path), "%s/%s", dir, files[i]);
    char *text = read_file(path, NULL);
    all = all && text && strlen(text) > 0;
    free(text);
  }
  check(all, "the real run wrote the five files");
  snprintf(path, sizeof(path), "/api/materials/items/%s/versions/2/playback?file=theme", theme);
  char midi[700];
  snprintf(midi, sizeof(midi), "%s/theme.mid", dir);
  check_playback_of(path, midi, "the theme's playback is built from its stored MIDI file");
  answer = get_item(theme, &status);
  const cJSON *result = member(theme_of(answer), "result");
  check(number_of(result, "seed") == 97 && cJSON_GetArraySize(member(member(result, "summary"), "themes")) >= 1 &&
            number_of(member(result, "brief"), "piece_count") == 2,
        "the result has the Composer's themes and the brief's two pieces");

  /* The stored files are what they claim to be (R-432). */
  snprintf(path, sizeof(path), "%s/plan.json", dir);
  check(check_plan_file(path), "plan.json is a Composer plan");
  snprintf(path, sizeof(path), "%s/theme.mid", dir);
  check(check_midi_file(path), "theme.mid is a MIDI file");
  snprintf(path, sizeof(path), "%s/theme.musicxml", dir);
  check(check_musicxml_file(path), "theme.musicxml is a MusicXML score");
  cJSON_Delete(answer);
  free(theme);
  free(first);
  free(second);
}

/* The development setting (Composer API 1.9): the request member reaches the Composer, a theme composed with it lists
 * each idea's development with its summary line, and a value the Composer does not know is refused at /development. */
static void test_development(const Motifs *m) {
  int status = 0;
  char slots[600];
  char body[2400];
  char request[2600];
  slots_of(m->a, m->b, slots, sizeof(slots));
  snprintf(body, sizeof(body),
           "{\"uses\":[],\"theme\":{\"form\":{\"into\":\"sentence\",\"hybrid\":null,\"mode\":\"minor\","
           "\"meter\":\"4/4\",\"bars\":8,\"cadence\":\"PAC\",\"first_cadence\":null,\"development\":\"theme\"},"
           "\"slots\":%s}}",
           slots);
  char *theme = create_item("theme", "Developed", body, &status);
  check(theme != NULL, "the theme with a development setting is saved");
  if (theme) {
    cJSON *answer = compose(theme, "{\"seed\":97}", &status);
    check(status == 200 && ok_true(answer), "a theme with development theme composes");
    cJSON_Delete(answer);
    cJSON *sent = last_request();
    check(strcmp(string_of(sent, "development"), "theme") == 0, "the request to the Composer carries the development");
    cJSON_Delete(sent);
    answer = get_item(theme, &status);
    const cJSON *themes = member(member(member(theme_of(answer), "result"), "summary"), "themes");
    const cJSON *developments = member(cJSON_GetArrayItem(themes, 0), "developments");
    bool summaries = cJSON_GetArraySize(developments) > 0;
    bool measured = summaries;
    cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, (cJSON *)developments) {
      summaries = summaries && strlen(string_of(entry, "summary")) > 0;
      /* Composer API 1.10: what was heard against what was intended, each member a value or null. */
      measured = measured && member(entry, "fidelity") && member(entry, "minimum") && member(entry, "heard") &&
                 member(entry, "heard_agrees");
    }
    check(summaries, "the result lists each idea's development with a summary");
    check(measured, "every development item carries fidelity, minimum, heard and heard_agrees");
    cJSON_Delete(answer);
  }
  free(theme);

  snprintf(body, sizeof(body),
           "{\"uses\":[],\"theme\":{\"form\":{\"into\":\"sentence\",\"hybrid\":null,\"mode\":\"minor\","
           "\"meter\":\"4/4\",\"bars\":8,\"cadence\":\"PAC\",\"first_cadence\":null,\"development\":\"wild\"},"
           "\"slots\":%s}}",
           slots);
  snprintf(request, sizeof(request), "{\"type\":\"theme\",\"body\":%s}", body);
  cJSON *answer = exchange("POST", "/api/materials/items/validate", request, &status);
  check((status == 400 || status == 422) && !ok_true(answer) && strcmp(string_of(answer, "path"), "/development") == 0,
        "validate refuses a bad development at /development");
  cJSON_Delete(answer);
}

/* A 2-bar 3/4 basic motif in an 8-bar theme: validate accepts it (the Composer checks the plan before any seeding), refuses a
 * given V in the last bar of a PAC theme at its /chords/<index> path, and compose builds a version. */
static void test_two_bar_theme(void) {
  int status = 0;
  char *motif = create_item_plain(
      "motif", "Two bars",
      "{\"uses\":[],\"motif\":{\"role\":\"melodic\",\"form\":\"full\",\"notes\":\"C4:q Eb4:q G4:q F4:q D4:q G3:q\","
      "\"meter\":\"3/4\",\"key\":\"C minor\",\"pickup\":\"0\",\"features\":[],\"analysis\":null,\"derived\":null}}");
  check(motif != NULL, "the 2-bar 3/4 motif is saved");
  if (!motif) {
    return;
  }
  static const char *const tonic_chords[] = {"I", "IV", "I", "IV", "I", "IV", "V", "I"};
  static const char *const dominant_end[] = {"I", "IV", "I", "IV", "I", "IV", "I", "V"};
  char body[2400];
  char chords[600];
  char request[2800];
  for (int variant = 0; variant < 2; variant++) {
    const char *const *list = variant == 0 ? tonic_chords : dominant_end;
    chords[0] = '\0';
    for (int bar = 0; bar < 8; bar++) {
      char entry[64];
      snprintf(entry, sizeof(entry), "%s{\"bar\":%d,\"text\":\"%s\"}", bar > 0 ? "," : "", bar, list[bar]);
      strcat(chords, entry);
    }
    snprintf(body, sizeof(body),
             "{\"uses\":[],\"theme\":{\"form\":{\"into\":\"sentence\",\"hybrid\":null,\"mode\":\"minor\","
             "\"meter\":\"3/4\",\"bars\":8,\"cadence\":\"PAC\",\"first_cadence\":null},"
             "\"slots\":[{\"slot\":\"basic\",\"item_id\":\"%s\",\"version\":1}],\"chords\":[%s]}}",
             motif, chords);
    snprintf(request, sizeof(request), "{\"type\":\"theme\",\"body\":%s}", body);
    cJSON *answer = exchange("POST", "/api/materials/items/validate", request, &status);
    if (variant == 0) {
      check(status == 200 && ok_true(answer), "validate accepts a 2-bar 3/4 basic motif with 8 one-chord bars");
    } else {
      check((status == 400 || status == 422) && !ok_true(answer) && strncmp(string_of(answer, "path"), "/chords/", 8) == 0,
            "validate refuses a given V in the last bar of a PAC theme at its /chords/<index> path");
    }
    cJSON_Delete(answer);
  }
  snprintf(body, sizeof(body),
           "{\"uses\":[],\"theme\":{\"form\":{\"into\":\"sentence\",\"hybrid\":null,\"mode\":\"minor\","
           "\"meter\":\"3/4\",\"bars\":8,\"cadence\":\"PAC\",\"first_cadence\":null},"
           "\"slots\":[{\"slot\":\"basic\",\"item_id\":\"%s\",\"version\":1}]}}",
           motif);
  char *theme = create_item("theme", "Two bar theme", body, &status);
  check(theme != NULL, "the 2-bar 3/4 theme is saved");
  if (theme) {
    cJSON *answer = compose(theme, "{\"seed\":1}", &status);
    check(status == 200 && ok_true(answer) && current_version(theme) == 2, "compose of the 2-bar 3/4 theme returns a composed version");
    cJSON_Delete(answer);
  }
  free(theme);
  free(motif);
}

int main(void) {
  system("rm -rf materials subjects pieces collections styles");

  struct mg_context *ctx = ursatz_gui_server_start("18773", ".");
  check(ctx != NULL, "server starts");
  if (!ctx) {
    return 1;
  }
  sleep_ms(200);

  int status = 0;
  cJSON *projects = exchange("GET", "/api/materials/projects", NULL, &status);
  snprintf(g_project, sizeof(g_project), "%s", string_of(cJSON_GetArrayItem(member(projects, "projects"), 0), "id"));
  cJSON_Delete(projects);

  Motifs motifs;
  memset(&motifs, 0, sizeof(motifs));
  bool made = make_motifs(&motifs);
  check(made, "the motifs and their forms are saved");
  if (made) {
    test_bodies(&motifs);
    test_compose(&motifs);
    test_files(&motifs);
    test_brief(&motifs);
    test_update_uses(&motifs);
    test_failures(&motifs);
    test_concurrent(&motifs);
    test_jobs_route(&motifs);
    test_real_composer(&motifs);
    test_development(&motifs);
    test_two_bar_theme();
  }

  ursatz_gui_server_stop(ctx);
  if (failures > 0) {
    fprintf(stderr, "%d check(s) failed\n", failures);
    return 1;
  }
  printf("all checks passed\n");
  return 0;
}
