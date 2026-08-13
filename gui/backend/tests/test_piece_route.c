/* Piece bodies, composing and the files of a version end to end (R-174). A fake Composer (URSATZ_GUI_COMPOSER)
 * records the request file of every run and writes the output files, so the request the route builds can be read
 * back; it also fails with the exit statuses of the real command. The motifs are saved with the real Composer, the
 * themes a piece uses are composed with the fake, and one test composes a piece with the real one. Uses a plain TCP
 * socket for the HTTP requests, same as test_composer_route.c. Runs in its own working directory (materials/,
 * subjects/, pieces/ and fake/ are created there). */

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
  addr.sin_port = htons(18774);
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

static bool write_script(const char *path, const char *text) {
  FILE *file = fopen(path, "wb");
  if (!file) {
    return false;
  }
  bool ok = fwrite(text, 1, strlen(text), file) == strlen(text);
  ok = (fclose(file) == 0) && ok;
  return ok && chmod(path, 0755) == 0;
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
           "{\"uses\":[],\"theme\":{\"form\":{\"into\":\"sentence\",\"hybrid\":null,\"key\":\"D\",\"mode\":\"minor\","
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

static cJSON *compose(const char *id, const char *body, int *status) {
  char path[160];
  snprintf(path, sizeof(path), "/api/materials/items/%s/compose", id);
  return exchange("POST", path, body, status);
}

static void version_dir(const char *id, int version, char *out, size_t size) {
  snprintf(out, size, "materials/items/%s/v%d", id, version);
}

/* The request file the fake Composer saw last. */
static cJSON *last_request(void) {
  return read_json_file("fake/last-request.json");
}

/* True when forms holds the item (with this flag, when flag is not NULL). */
static bool forms_have(const cJSON *forms, const char *item, const char *flag) {
  const cJSON *form = NULL;
  cJSON_ArrayForEach(form, forms) {
    if (strcmp(string_of(form, "item"), item) == 0) {
      return flag == NULL || strcmp(string_of(form, "flag"), flag) == 0;
    }
  }
  return false;
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
  char rhythm[64];
  char accompaniment[64];
  char approved[64];
  char unflagged[64];
  char rejected[64];
  char grandchild[64];
  char unrelated[64];
  char theme[64]; /* a theme composed with the fake: version 2 has a plan.json */
  char draft[64]; /* a theme that was never composed */
} Items;

static bool take(char *out, char *id) {
  bool ok = id != NULL;
  if (id) {
    snprintf(out, 64, "%s", id);
  }
  free(id);
  return ok;
}

/* A piece body: the structure's sections (JSON array text) and the members after the structure (JSON members, may be ""). */
static void piece_body(const char *sections, const char *more, char *out, size_t size) {
  snprintf(out, size,
           "{\"uses\":[],\"piece\":{\"structure\":{\"key\":\"D\",\"mode\":\"minor\",\"meter\":\"4/4\",\"sections\":%s}%s}}",
           sections, more);
}

/* One section as JSON text; fill is JSON text. */
static void section_of(char *out, size_t size, const char *id, const char *part, int beats, const char *fill) {
  snprintf(out, size, "{\"id\":\"%s\",\"part\":\"%s\",\"letter\":\"%c\",\"beats\":%d,\"pickup\":\"0\",\"cadence\":null,\"fill\":%s}",
           id, part, id[0] >= 'a' && id[0] <= 'z' ? id[0] - 32 : id[0], beats, fill);
}

static void motif_fill_of(char *out, size_t size, const char *item, int version, const char *more) {
  snprintf(out, size, "{\"kind\":\"motif\",\"item_id\":\"%s\",\"version\":%d%s}", item, version, more);
}

static void theme_fill_of(char *out, size_t size, const char *item, int version) {
  snprintf(out, size, "{\"kind\":\"theme\",\"item_id\":\"%s\",\"version\":%d}", item, version);
}

/* The status of creating a piece with these sections and members; the new id goes to *id when that is not NULL. */
static int create_piece(const char *name, const char *sections, const char *more, char **id) {
  char body[12000];
  int status = 0;
  piece_body(sections, more, body, sizeof(body));
  char *made = create_item("piece", name, body, &status);
  if (id) {
    *id = made;
  } else {
    free(made);
  }
  return status;
}

static cJSON *piece_of_answer(const cJSON *answer) {
  return (cJSON *)member(member(answer, "body"), "piece");
}

/* Composes the theme item with the fake (version n+1 holds a plan.json with the seed). */
static bool compose_theme(const char *theme, int seed) {
  char body[64];
  int status = 0;
  snprintf(body, sizeof(body), "{\"seed\":%d}", seed);
  cJSON *answer = compose(theme, body, &status);
  cJSON_Delete(answer);
  return status == 200;
}

/* The motifs, their saved forms, and two themes: Theme is composed (version 2), Draft is not. */
static bool make_items(Items *m) {
  bool ok = true;
  char slots[600];
  char body[2400];
  int status = 0;
  ok = take(m->a, create_motif("A", "full", "melodic", "C4:e Eb4:e G4:e Eb4:e C4:e Eb4:e G4:e C5:e", "null")) && ok;
  ok = take(m->b, create_motif("B", "full", "melodic", "G4:e F4:e Eb4:e D4:e F4:e Eb4:e D4:e C4:e", "null")) && ok;
  ok = take(m->rhythm, create_motif("Rhythm", "rhythm", "melodic", "x:q x:e x:e x:h", "null")) && ok;
  ok = take(m->accompaniment, create_motif("Acc", "full", "accompaniment", "C3:q G3:q Eb3:q G3:q", "null")) && ok;
  ok = take(m->approved, create_derived("A up", m->a, "approved", "D4:e F4:e A4:e F4:e D4:e F4:e A4:e D5:e")) && ok;
  ok = take(m->unflagged, create_derived("A up 2", m->a, "unflagged", "Eb4:e G4:e Bb4:e G4:e Eb4:e G4:e Bb4:e Eb5:e")) && ok;
  ok = take(m->rejected, create_derived("A up 3", m->a, "rejected", "F4:e A4:e C5:e A4:e F4:e A4:e C5:e F5:e")) && ok;
  ok = take(m->grandchild,
            create_derived("A up up", m->approved, "unflagged", "Eb4:e G4:e C5:e G4:e Eb4:e G4:e C5:e Eb5:e")) && ok;
  ok = take(m->unrelated, create_motif("C", "full", "melodic", "C4:q Eb4:e G4:e C5:h", "null")) && ok;
  snprintf(slots, sizeof(slots), "[{\"slot\":\"basic\",\"item_id\":\"%s\",\"version\":1}]", m->a);
  snprintf(body, sizeof(body),
           "{\"uses\":[],\"theme\":{\"form\":{\"into\":\"sentence\",\"hybrid\":null,\"key\":\"D\",\"mode\":\"minor\","
           "\"meter\":\"4/4\",\"bars\":8,\"cadence\":\"PAC\",\"first_cadence\":null},\"slots\":%s}}",
           slots);
  ok = take(m->theme, create_item("theme", "Theme", body, &status)) && ok;
  ok = take(m->draft, create_item("theme", "Draft", body, &status)) && ok;
  return ok;
}

/* The bodies the validator takes and refuses, through the create route. */
static void test_bodies(const Items *m) {
  int status = 0;
  char fill[600];
  char fill2[600];
  char one[1200];
  char two[4000];
  char more[600];
  char *id = NULL;

  /* The old placeholders. */
  static const char *const placeholders[] = {"null", "\"intro A B\"", "[]", "[\"A\",\"B\"]", "{\"x\":1}", "{\"sections\":null}"};
  for (size_t i = 0; i < sizeof(placeholders) / sizeof(placeholders[0]); i++) {
    char body[200];
    char what[200];
    snprintf(body, sizeof(body), "{\"uses\":[],\"piece\":{\"structure\":%s}}", placeholders[i]);
    id = create_item("piece", "Placeholder", body, &status);
    snprintf(what, sizeof(what), "the old placeholder with structure %s is still valid", placeholders[i]);
    check(status == 200 && id, what);
    free(id);
  }
  id = create_item("piece", "No structure", "{\"uses\":[],\"piece\":{}}", &status);
  check(status == 200 && id, "an old piece with no structure is valid");
  free(id);

  /* Structure only, then every fill kind. */
  check(create_piece("Structure only", "[{\"id\":\"s0\",\"part\":\"body\",\"beats\":32}]", "", NULL) == 200,
        "a structure-only body with one body section is valid");
  motif_fill_of(fill, sizeof(fill), m->a, 1, ",\"into\":\"hybrid\",\"hybrid\":\"antecedent+continuation\"");
  section_of(one, sizeof(one), "s1", "body", 32, fill);
  theme_fill_of(fill2, sizeof(fill2), m->theme, 2);
  char third[600];
  section_of(third, sizeof(third), "s2", "body", 32, fill2);
  char prog[600];
  char fourth[800];
  snprintf(prog, sizeof(prog), "{\"kind\":\"progression\",\"numerals\":\"i iv V i\",\"cells\":2}");
  section_of(fourth, sizeof(fourth), "s3", "turnaround", 8, prog);
  char intro[300];
  char ending[300];
  section_of(intro, sizeof(intro), "s0", "intro", 8, "{\"kind\":\"empty\"}");
  section_of(ending, sizeof(ending), "end", "ending", 8, "null");
  snprintf(two, sizeof(two), "[%s,%s,%s,%s,%s,%s]", intro, one, third, fourth, ending, "{\"id\":\"s9\",\"part\":\"ending\",\"beats\":4}");
  snprintf(more, sizeof(more),
           ",\"motive\":{\"item_id\":\"%s\",\"version\":1},\"pool\":\"approved\",\"accompaniment\":{\"pattern\":\"alberti\"},"
           "\"without\":[\"motive-rhythm\"],\"style_weight\":\"1/2\",\"tempo\":120,\"check\":true,\"seed\":7,\"result\":null",
           m->b);
  status = create_piece("Every fill", two, more, &id);
  check(status == 200 && id, "every fill kind and every member is valid");
  if (id) {
    char path[160];
    snprintf(path, sizeof(path), "/api/materials/items/%s", id);
    cJSON *answer = exchange("GET", path, NULL, &status);
    const cJSON *uses = member(member(answer, "body"), "uses");
    bool has_motif = false;
    bool has_theme = false;
    bool has_motive = false;
    const cJSON *use = NULL;
    cJSON_ArrayForEach(use, uses) {
      has_motif = has_motif || (strcmp(string_of(use, "item_id"), m->a) == 0 && cJSON_IsObject(member(member(use, "copy"), "motif")));
      has_theme = has_theme || (strcmp(string_of(use, "item_id"), m->theme) == 0 && number_of(use, "version") == 2 &&
                                cJSON_IsObject(member(member(use, "copy"), "theme")));
      has_motive = has_motive || strcmp(string_of(use, "item_id"), m->b) == 0;
    }
    check(cJSON_GetArraySize(uses) == 3 && has_motif && has_theme && has_motive,
          "the uses are the motif fill, the theme fill and the motive, each with a copy");
    cJSON_Delete(answer);
    free(id);
  }
  check(create_piece("Missing motif", "[{\"id\":\"s0\",\"part\":\"body\",\"beats\":8,\"fill\":{\"kind\":\"motif\","
                                         "\"item_id\":\"x\",\"version\":1}}]", "", NULL) == 400,
        "a motif fill of an item that does not exist is refused");

  /* Structures that are refused. */
  static const struct {
    const char *what;
    const char *sections;
  } bad[] = {
      {"no section", "[]"},
      {"no body", "[{\"id\":\"s0\",\"part\":\"intro\",\"beats\":8}]"},
      {"parts out of order", "[{\"id\":\"s0\",\"part\":\"turnaround\",\"beats\":8},{\"id\":\"s1\",\"part\":\"body\",\"beats\":8}]"},
      {"a repeated id", "[{\"id\":\"s0\",\"part\":\"body\",\"beats\":8},{\"id\":\"s0\",\"part\":\"body\",\"beats\":8}]"},
      {"an id that is not sN or end", "[{\"id\":\"x1\",\"part\":\"body\",\"beats\":8}]"},
      {"an id with a path", "[{\"id\":\"s0/../1\",\"part\":\"body\",\"beats\":8}]"},
      {"an unknown part", "[{\"id\":\"s0\",\"part\":\"coda\",\"beats\":8}]"},
      {"no beats", "[{\"id\":\"s0\",\"part\":\"body\"}]"},
      {"zero beats", "[{\"id\":\"s0\",\"part\":\"body\",\"beats\":0}]"},
      {"too many beats", "[{\"id\":\"s0\",\"part\":\"body\",\"beats\":2049}]"},
      {"half a beat", "[{\"id\":\"s0\",\"part\":\"body\",\"beats\":2.5}]"},
      {"a pickup as long as a bar", "[{\"id\":\"s0\",\"part\":\"body\",\"beats\":8,\"pickup\":\"4\"}]"},
      {"a pickup that is no number", "[{\"id\":\"s0\",\"part\":\"body\",\"beats\":8,\"pickup\":\"x\"}]"},
      {"an unknown cadence", "[{\"id\":\"s0\",\"part\":\"body\",\"beats\":8,\"cadence\":\"XX\"}]"},
      {"an unknown fill kind", "[{\"id\":\"s0\",\"part\":\"body\",\"beats\":8,\"fill\":{\"kind\":\"bogus\"}}]"},
      {"a fill that is a string", "[{\"id\":\"s0\",\"part\":\"body\",\"beats\":8,\"fill\":\"empty\"}]"},
      {"a motif fill with no item", "[{\"id\":\"s0\",\"part\":\"body\",\"beats\":8,\"fill\":{\"kind\":\"motif\"}}]"},
      {"a progression with no numerals", "[{\"id\":\"s0\",\"part\":\"body\",\"beats\":8,\"fill\":{\"kind\":\"progression\"}}]"},
      {"a progression with empty numerals",
       "[{\"id\":\"s0\",\"part\":\"body\",\"beats\":8,\"fill\":{\"kind\":\"progression\",\"numerals\":\"\"}}]"},
      {"nine cells", "[{\"id\":\"s0\",\"part\":\"body\",\"beats\":8,\"fill\":{\"kind\":\"progression\",\"numerals\":\"i\",\"cells\":9}}]"},
      {"zero cells", "[{\"id\":\"s0\",\"part\":\"body\",\"beats\":8,\"fill\":{\"kind\":\"progression\",\"numerals\":\"i\",\"cells\":0}}]"},
      {"an unknown into", "[{\"id\":\"s0\",\"part\":\"body\",\"beats\":8,\"fill\":{\"kind\":\"motif\",\"item_id\":\"mot-1\",\"version\":1,\"into\":\"rondo\"}}]"},
  };
  for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
    char what[200];
    snprintf(what, sizeof(what), "%s is refused", bad[i].what);
    check(create_piece("Bad", bad[i].sections, "", NULL) == 400, what);
  }
  /* 65 numerals and 33 sections. */
  char numerals[400] = "";
  for (int i = 0; i < 65; i++) {
    strcat(numerals, i ? " I" : "I");
  }
  snprintf(fill, sizeof(fill), "{\"kind\":\"progression\",\"numerals\":\"%s\"}", numerals);
  section_of(one, sizeof(one), "s0", "body", 8, fill);
  snprintf(two, sizeof(two), "[%s]", one);
  check(create_piece("Bad", two, "", NULL) == 400, "65 numerals are refused");
  char many[8000] = "[";
  for (int i = 0; i < 33; i++) {
    char one_section[200];
    snprintf(one_section, sizeof(one_section), "%s{\"id\":\"s%d\",\"part\":\"body\",\"beats\":4}", i ? "," : "", i);
    strcat(many, one_section);
  }
  strcat(many, "]");
  check(create_piece("Many", many, "", NULL) == 400, "33 sections are refused");
  many[strlen(many) - strlen(",{\"id\":\"s32\",\"part\":\"body\",\"beats\":4}]")] = '\0';
  strcat(many, "]");
  check(create_piece("Many", many, "", NULL) == 200, "32 sections are valid");

  /* Bad fills by what they name. */
  motif_fill_of(fill, sizeof(fill), m->rhythm, 1, "");
  section_of(one, sizeof(one), "s0", "body", 8, fill);
  snprintf(two, sizeof(two), "[%s]", one);
  check(create_piece("Bad", two, "", NULL) == 400, "a motif fill of a rhythm-only motif is refused");
  motif_fill_of(fill, sizeof(fill), m->accompaniment, 1, "");
  section_of(one, sizeof(one), "s0", "body", 8, fill);
  snprintf(two, sizeof(two), "[%s]", one);
  check(create_piece("Bad", two, "", NULL) == 400, "a motif fill of an accompaniment motif is refused");
  motif_fill_of(fill, sizeof(fill), m->theme, 2, "");
  section_of(one, sizeof(one), "s0", "body", 8, fill);
  snprintf(two, sizeof(two), "[%s]", one);
  check(create_piece("Bad", two, "", NULL) == 400, "a motif fill of a theme is refused");
  motif_fill_of(fill, sizeof(fill), m->a, 2, "");
  section_of(one, sizeof(one), "s0", "body", 8, fill);
  snprintf(two, sizeof(two), "[%s]", one);
  check(create_piece("Bad", two, "", NULL) == 400, "a motif fill of a version that does not exist is refused");
  theme_fill_of(fill, sizeof(fill), m->draft, 1);
  section_of(one, sizeof(one), "s0", "body", 8, fill);
  snprintf(two, sizeof(two), "[%s]", one);
  check(create_piece("Bad", two, "", NULL) == 400, "a theme fill of a theme version with no result is refused");
  theme_fill_of(fill, sizeof(fill), m->a, 1);
  section_of(one, sizeof(one), "s0", "body", 8, fill);
  snprintf(two, sizeof(two), "[%s]", one);
  check(create_piece("Bad", two, "", NULL) == 400, "a theme fill of a motif is refused");

  /* Bad members. */
  const char *ok_sections = "[{\"id\":\"s0\",\"part\":\"body\",\"beats\":32}]";
  snprintf(more, sizeof(more), ",\"motive\":{\"item_id\":\"%s\",\"version\":1}", m->rhythm);
  check(create_piece("Bad", ok_sections, more, NULL) == 400, "a motive that is a rhythm-only motif is refused");
  check(create_piece("Bad", ok_sections, ",\"motive\":{\"item_id\":\"mot-x\"}", NULL) == 400, "a motive with no version is refused");
  check(create_piece("Bad", ok_sections, ",\"motive\":null", NULL) == 200, "a null motive is valid");
  check(create_piece("Bad", ok_sections, ",\"pool\":\"all\"", NULL) == 400, "an unknown pool is refused");
  check(create_piece("Bad", ok_sections, ",\"accompaniment\":{\"pattern\":\"motive\",\"item_id\":\"mot-x\",\"version\":1}", NULL) == 400,
        "the accompaniment pattern motive is refused in a piece");
  check(create_piece("Bad", ok_sections, ",\"accompaniment\":{\"pattern\":\"waltz\"}", NULL) == 400, "an unknown pattern is refused");
  check(create_piece("Bad", ok_sections, ",\"tempo\":10", NULL) == 400, "a tempo below 20 is refused");
  check(create_piece("Bad", ok_sections, ",\"seed\":-1", NULL) == 400, "a negative seed is refused");
  check(create_piece("Bad", ok_sections, ",\"check\":1", NULL) == 400, "a check that is not a boolean is refused");
  check(create_piece("Bad", ok_sections, ",\"style_weight\":\"heavy\"", NULL) == 400, "a bad style weight is refused");
  check(create_piece("Bad", ok_sections, ",\"brief\":{\"scopes\":[],\"piece_ids\":[],\"style_ids\":[]}", NULL) == 400,
        "an empty brief is refused");
  check(create_piece("Bad", ok_sections, ",\"result\":3", NULL) == 400, "a result that is not an object is refused");
  id = NULL;
  status = 0;
  {
    char body[400];
    snprintf(body, sizeof(body),
             "{\"uses\":[],\"piece\":{\"structure\":{\"key\":\"Z\",\"mode\":\"minor\",\"meter\":\"4/4\",\"sections\":%s}}}", ok_sections);
    id = create_item("piece", "Bad key", body, &status);
    check(status == 400 && !id, "a key that is not a tonic is refused");
    snprintf(body, sizeof(body),
             "{\"uses\":[],\"piece\":{\"structure\":{\"key\":\"D\",\"mode\":\"minor\",\"meter\":\"5/4\",\"sections\":%s}}}", ok_sections);
    id = create_item("piece", "Bad meter", body, &status);
    check(status == 400 && !id, "a meter of 5/4 is refused");
  }
}

/* The sections of the piece the compose tests use: intro, a motif body, a theme body, a progression, a turnaround, ending. */
static void full_sections(const Items *m, char *out, size_t size) {
  char fill[400];
  char s[6][700];
  section_of(s[0], sizeof(s[0]), "s0", "intro", 8, "{\"kind\":\"empty\"}");
  motif_fill_of(fill, sizeof(fill), m->a, 1, ",\"into\":\"sentence\"");
  section_of(s[1], sizeof(s[1]), "s1", "body", 32, fill);
  theme_fill_of(fill, sizeof(fill), m->theme, 2);
  section_of(s[2], sizeof(s[2]), "s2", "body", 32, fill);
  section_of(s[3], sizeof(s[3]), "s3", "body", 16, "{\"kind\":\"progression\",\"numerals\":\"i iv V i\",\"cells\":2}");
  section_of(s[4], sizeof(s[4]), "s4", "turnaround", 8, "null");
  section_of(s[5], sizeof(s[5]), "end", "ending", 8, "{\"kind\":\"empty\"}");
  snprintf(out, size, "[%s,%s,%s,%s,%s,%s]", s[0], s[1], s[2], s[3], s[4], s[5]);
}

static const cJSON *section_named(const cJSON *request, const char *id) {
  const cJSON *section = NULL;
  cJSON_ArrayForEach(section, member(request, "sections")) {
    if (strcmp(string_of(section, "id"), id) == 0) {
      return section;
    }
  }
  return NULL;
}

/* True when the file at path holds text. */
static bool file_has(const char *path, const char *text) {
  char *content = read_file(path, NULL);
  bool found = content && strstr(content, text);
  free(content);
  return found;
}

static void test_compose(const Items *m) {
  int status = 0;
  char sections[5000];
  char more[600];
  char dir[200];
  char path[400];

  full_sections(m, sections, sizeof(sections));
  snprintf(more, sizeof(more),
           ",\"motive\":{\"item_id\":\"%s\",\"version\":1},\"pool\":\"approved-and-unflagged\",\"check\":true,\"tempo\":96",
           m->a);
  char *piece = NULL;
  check(create_piece("Composed", sections, more, &piece) == 200 && piece, "the piece to compose is saved");
  if (!piece) {
    return;
  }

  cJSON *answer = compose(piece, "{\"seed\":97}", &status);
  check(status == 200 && ok_true(answer), "compose answers 200");
  check(number_of(member(answer, "item"), "current_version") == 2, "compose makes version 2");
  cJSON_Delete(answer);

  version_dir(piece, 2, dir, sizeof(dir));
  static const char *const files[] = {"request.json",  "plan.json",  "ending-plan.json", "loop.mid",
                                      "loop.musicxml", "ending.mid", "ending.musicxml",  "theme-s2.plan.json"};
  bool all = true;
  for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); i++) {
    snprintf(path, sizeof(path), "%s/%s", dir, files[i]);
    all = all && file_exists(path);
  }
  check(all, "the version folder holds the request, both plans, both MIDI and MusicXML files and the theme plan");
  snprintf(path, sizeof(path), "%s/brief.style.json", dir);
  check(!file_exists(path), "a piece without a brief has no style file");

  answer = get_item(piece, &status);
  const cJSON *result = member(piece_of_answer(answer), "result");
  check(number_of(result, "seed") == 97, "the result keeps the seed");
  check(cJSON_IsObject(member(result, "summary")), "the result keeps the command's summary");
  check(strcmp(string_of(member(result, "loop"), "start"), "2") == 0 && strcmp(string_of(member(result, "loop"), "end"), "10") == 0,
        "the result keeps the loop region");
  check(number_of(member(result, "brief"), "piece_count") == 0 && cJSON_IsArray(member(member(result, "brief"), "skipped")),
        "the result's brief is empty without one");
  const cJSON *names = member(result, "files");
  check(cJSON_GetArraySize(names) == 8 && strcmp(cJSON_GetArrayItem(names, 0)->valuestring, "request.json") == 0 &&
            strcmp(cJSON_GetArrayItem(names, 7)->valuestring, "theme-s2.plan.json") == 0,
        "the result names the seven files and the theme plan");
  const cJSON *versions = member(member(answer, "item"), "versions");
  check(strcmp(string_of(cJSON_GetArrayItem(versions, 1), "note"), "composed seed 97") == 0, "the note is the seed");
  check(cJSON_GetArraySize(member(member(answer, "body"), "uses")) == 2, "the new version keeps its uses");
  cJSON_Delete(answer);

  snprintf(path, sizeof(path), "/api/materials/items/%s/versions/1", piece);
  answer = exchange("GET", path, NULL, &status);
  const cJSON *first_result = member(piece_of_answer(answer), "result");
  check(status == 200 && (!first_result || cJSON_IsNull(first_result)), "version 1 has no result");
  cJSON_Delete(answer);

  /* The request. */
  cJSON *request = last_request();
  check(strcmp(string_of(request, "schema"), "ursatz-piece-request") == 0 && number_of(request, "schema_version") == 1,
        "the request has the piece request schema");
  check(strcmp(string_of(request, "key"), "D") == 0 && strcmp(string_of(request, "mode"), "minor") == 0 &&
            strcmp(string_of(request, "meter"), "4/4") == 0 && number_of(request, "tempo") == 96,
        "the request has the brief");
  check(strcmp(string_of(member(request, "motive"), "degrees"), kDegreesA) == 0 &&
            strcmp(string_of(member(request, "motive"), "role"), "melodic") == 0,
        "the motive is the copy's degrees and role");
  check(cJSON_GetArraySize(member(request, "sections")) == 6, "the request has the six sections");
  const cJSON *s0 = section_named(request, "s0");
  check(strcmp(string_of(s0, "part"), "intro") == 0 && number_of(s0, "beats") == 8 && strcmp(string_of(s0, "pickup"), "0") == 0 &&
            cJSON_IsNull(member(s0, "cadence")) && strcmp(string_of(member(s0, "fill"), "kind"), "empty") == 0,
        "an empty section has its part, beats, pickup, no cadence and an empty fill");
  const cJSON *s1 = section_named(request, "s1");
  const cJSON *fill = member(s1, "fill");
  const cJSON *forms = member(fill, "forms");
  check(strcmp(string_of(fill, "kind"), "motif") == 0 && strcmp(string_of(member(fill, "motive"), "degrees"), kDegreesA) == 0 &&
            strcmp(string_of(fill, "into"), "sentence") == 0 && !member(fill, "hybrid"),
        "a motif fill has the motif's degrees and its into");
  check(forms_have(forms, m->approved, "approved") && forms_have(forms, m->unflagged, "unflagged") &&
            forms_have(forms, m->grandchild, "unflagged"),
        "a motif fill has the approved and unflagged forms, down the derived chain");
  check(!forms_have(forms, m->rejected, NULL) && !forms_have(forms, m->unrelated, NULL),
        "a motif fill has no rejected form and no form of another motif");
  const cJSON *s2 = section_named(request, "s2");
  const char *plan = string_of(member(s2, "fill"), "plan");
  const char *suffix = "/v2/theme-s2.plan.json";
  check(strcmp(string_of(member(s2, "fill"), "kind"), "theme") == 0 && plan[0] == '/' && strlen(plan) > strlen(suffix) &&
            strcmp(plan + strlen(plan) - strlen(suffix), suffix) == 0,
        "a theme fill names the copy of the plan by an absolute path in the version folder");
  check(file_has(plan, "\"seed\": 21"), "the copy is the plan of the theme version (composed with seed 21)");
  const cJSON *s3 = section_named(request, "s3");
  check(strcmp(string_of(member(s3, "fill"), "kind"), "progression") == 0 &&
            strcmp(string_of(member(s3, "fill"), "numerals"), "i iv V i") == 0 && number_of(member(s3, "fill"), "cells") == 2,
        "a progression fill passes its numerals and cells");
  check(strcmp(string_of(member(section_named(request, "end"), "fill"), "kind"), "empty") == 0 &&
            strcmp(string_of(section_named(request, "end"), "part"), "ending") == 0,
        "the ending is a section like the others");
  check(cJSON_IsNull(member(section_named(request, "s4"), "fill")) == false &&
            strcmp(string_of(member(section_named(request, "s4"), "fill"), "kind"), "empty") == 0,
        "a null fill is written as an empty one");
  check(cJSON_IsNull(member(request, "accompaniment")) && cJSON_IsNull(member(request, "voicing")) && !member(request, "style"),
        "the request has no accompaniment, no voicing and no style");
  cJSON_Delete(request);
  char *argv_log = read_file("fake/argv.log", NULL);
  const char *last_run = argv_log ? strrchr(argv_log, '\n') : NULL;
  if (argv_log && last_run && last_run > argv_log) {
    argv_log[last_run - argv_log] = '\0';
    last_run = strrchr(argv_log, '\n');
    last_run = last_run ? last_run + 1 : argv_log;
  } else {
    last_run = "";
  }
  check(strncmp(last_run, "piece ", 6) == 0 && strstr(last_run, "--check") && strstr(last_run, "--seed 97") &&
            strstr(last_run, "--format json") && strstr(last_run, "--ending-out") && strstr(last_run, "/plan.json") &&
            strstr(last_run, "loop.mid") && strstr(last_run, "loop.musicxml") && strstr(last_run, "--ending-midi") &&
            strstr(last_run, "ending.mid") && strstr(last_run, "--ending-musicxml") && strstr(last_run, "ending.musicxml"),
        "the Composer got piece, the eight outputs, --check and the seed");
  free(argv_log);

  /* The body's seed is used when the call has none; the call's wins. */
  free(compose(piece, "{}", &status));
  check(status == 200, "a compose with no seed runs");
  free(piece);

  snprintf(more, sizeof(more), ",\"seed\":41");
  create_piece("Body seed", sections, more, &piece);
  answer = compose(piece, "{}", &status);
  check(status == 200 && strcmp(string_of(cJSON_GetArrayItem(member(member(answer, "item"), "versions"), 1), "note"),
                                 "composed seed 41") == 0,
        "the body's seed is used when the call has none");
  cJSON_Delete(answer);
  answer = compose(piece, "{\"seed\":42}", &status);
  check(status == 200 && strcmp(string_of(cJSON_GetArrayItem(member(member(answer, "item"), "versions"), 2), "note"),
                                 "composed seed 42") == 0,
        "the call's seed wins over the body's");
  cJSON_Delete(answer);
  free(piece);

  /* The pool setting, a hybrid fill, an accompaniment pattern and a pickup. */
  char fill_text[500];
  char s[2][900];
  char two[2000];
  motif_fill_of(fill_text, sizeof(fill_text), m->a, 1, ",\"into\":\"hybrid\",\"hybrid\":\"antecedent+continuation\"");
  section_of(s[0], sizeof(s[0]), "s0", "body", 32, fill_text);
  snprintf(s[1], sizeof(s[1]), "{\"id\":\"s1\",\"part\":\"turnaround\",\"beats\":8,\"pickup\":\"1/2\",\"cadence\":\"HC\",\"fill\":null}");
  snprintf(two, sizeof(two), "[%s,%s]", s[0], s[1]);
  create_piece("Pool none", two, ",\"pool\":\"none\",\"accompaniment\":{\"pattern\":\"alberti\"}", &piece);
  free(compose(piece, "{}", &status));
  request = last_request();
  fill = member(section_named(request, "s0"), "fill");
  check(status == 200 && cJSON_GetArraySize(member(fill, "forms")) == 0, "the none pool is empty");
  check(strcmp(string_of(fill, "into"), "hybrid") == 0 && strcmp(string_of(fill, "hybrid"), "antecedent+continuation") == 0,
        "a hybrid fill passes its hybrid");
  check(strcmp(string_of(member(request, "accompaniment"), "pattern"), "alberti") == 0, "an accompaniment pattern passes");
  check(strcmp(string_of(section_named(request, "s1"), "pickup"), "1/2") == 0 &&
            strcmp(string_of(section_named(request, "s1"), "cadence"), "HC") == 0,
        "a pickup and a cadence pass");
  check(cJSON_IsNull(member(request, "motive")), "no motive is a null motive");
  cJSON_Delete(request);
  free(piece);
  create_piece("Pool approved", two, ",\"pool\":\"approved\"", &piece);
  free(compose(piece, "{}", &status));
  request = last_request();
  forms = member(member(section_named(request, "s0"), "fill"), "forms");
  check(status == 200 && forms_have(forms, m->approved, NULL) && !forms_have(forms, m->unflagged, NULL) &&
            !forms_have(forms, m->grandchild, NULL),
        "the approved pool leaves out the unflagged forms");
  cJSON_Delete(request);
  free(piece);
}

static void test_failures(const Items *m, const char *stuck, const char *usage) {
  int status = 0;
  char sections[5000];
  char dir[200];
  full_sections(m, sections, sizeof(sections));
  char *piece = NULL;
  create_piece("Fails", sections, "", &piece);
  check(piece != NULL, "the piece for the failing runs is saved");
  if (!piece) {
    return;
  }
  version_dir(piece, 2, dir, sizeof(dir));

  setenv("URSATZ_GUI_COMPOSER", stuck, 1);
  cJSON *answer = compose(piece, "{\"seed\":1}", &status);
  check(status == 422 && !ok_true(answer), "a stuck run is 422");
  check(strstr(string_of(answer, "error"), "no seed works") != NULL, "the answer carries the Composer's message");
  cJSON_Delete(answer);
  check(current_version(piece) == 1 && !dir_exists(dir), "a stuck run writes no version and leaves no folder, no theme copy either");

  setenv("URSATZ_GUI_COMPOSER", usage, 1);
  answer = compose(piece, "{\"seed\":1}", &status);
  check(status == 400 && strstr(string_of(answer, "error"), "does not read") != NULL, "a usage error is 400");
  cJSON_Delete(answer);
  check(current_version(piece) == 1 && !dir_exists(dir), "a usage error writes no version and leaves no folder");

  setenv("URSATZ_GUI_COMPOSER", "/nonexistent/ursatz-composer", 1);
  answer = compose(piece, "{\"seed\":1}", &status);
  check(status == 503, "a missing Composer is 503");
  cJSON_Delete(answer);
  check(current_version(piece) == 1 && !dir_exists(dir), "a missing Composer writes no version and leaves no folder");
  free(piece);

  /* A piece that cannot be composed is 400 before the Composer runs. */
  char *placeholder = create_item("piece", "Placeholder", "{\"uses\":[],\"piece\":{\"structure\":null}}", &status);
  answer = compose(placeholder, "{}", &status);
  check(status == 400 && strstr(string_of(answer, "error"), "structure") != NULL, "a placeholder piece is 400");
  cJSON_Delete(answer);
  free(placeholder);
  create_piece("Style", "[{\"id\":\"s0\",\"part\":\"body\",\"beats\":32}]", "", &piece);
  {
    char body[600];
    snprintf(body, sizeof(body),
             "{\"uses\":[],\"piece\":{\"structure\":{\"key\":\"D\",\"mode\":\"style\",\"meter\":\"4/4\","
             "\"sections\":[{\"id\":\"s0\",\"part\":\"body\",\"beats\":32}]}}}");
    char *styled = create_item("piece", "Style without a brief", body, &status);
    answer = compose(styled, "{}", &status);
    check(status == 400 && strstr(string_of(answer, "error"), "needs a brief") != NULL, "the mode style needs a brief");
    cJSON_Delete(answer);
    free(styled);
  }
  free(piece);
  answer = compose(m->a, "{}", &status);
  cJSON_Delete(answer);
  check(status == 400, "a motif is not composed");
  answer = compose("pce-0123456789abcdef", "{}", &status);
  check(status == 404, "a piece that does not exist is 404");
  cJSON_Delete(answer);
  answer = compose(m->theme, "{\"seed\":\"x\"}", &status);
  check(status == 400, "a seed that is no number is 400");
  cJSON_Delete(answer);
}

/* The files route serves the piece names and the theme copies, and nothing else. */
static void test_files(const Items *m) {
  int status = 0;
  char sections[5000];
  char path[400];
  full_sections(m, sections, sizeof(sections));
  char *piece = NULL;
  create_piece("Files", sections, "", &piece);
  cJSON *answer = compose(piece, "{\"seed\":4}", &status);
  cJSON_Delete(answer);
  check(status == 200, "the piece for the files is composed");

  static const char *const names[] = {"request.json",  "plan.json",  "ending-plan.json", "loop.mid",
                                      "loop.musicxml", "ending.mid", "ending.musicxml",  "theme-s2.plan.json"};
  static const char *const types[] = {"application/json", "application/json", "application/json", "audio/midi",
                                      "application/xml",  "audio/midi",       "application/xml",  "application/json"};
  for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
    snprintf(path, sizeof(path), "/api/materials/items/%s/versions/2/files/%s", piece, names[i]);
    char *response = http_exchange("GET", path, NULL, NULL, 0, NULL);
    char type[128];
    content_type_of(response, type, sizeof(type));
    char what[160];
    snprintf(what, sizeof(what), "%s is served as %s", names[i], types[i]);
    check(response_status(response) == 200 && strcmp(type, types[i]) == 0 && strlen(response_body(response)) > 0, what);
    free(response);
  }
  snprintf(path, sizeof(path), "/api/materials/items/%s/versions/2/files/loop.mid", piece);
  size_t size = 0;
  char *response = http_exchange("GET", path, NULL, NULL, 0, &size);
  check(size - (size_t)(response_body(response) - response) == 5, "the MIDI file is served whole (the fake writes five bytes)");
  free(response);

  static const char *const refused[] = {"item.json",         "brief.style.json",   "..%2Fitem.json",
                                        "v2.json",           "loop.MID",           "theme-s2.plan.jsonx",
                                        "theme-.plan.json",  "theme-x2.plan.json", "theme-s2%2Fx.plan.json",
                                        "theme-s1234567890123.plan.json", "ending.plan.json", "loop.mid.bak"};
  for (size_t i = 0; i < sizeof(refused) / sizeof(refused[0]); i++) {
    snprintf(path, sizeof(path), "/api/materials/items/%s/versions/2/files/%s", piece, refused[i]);
    response = http_exchange("GET", path, NULL, NULL, 0, NULL);
    char what[160];
    snprintf(what, sizeof(what), "%s is not served", refused[i]);
    check(response_status(response) == 404, what);
    free(response);
  }
  snprintf(path, sizeof(path), "/api/materials/items/%s/versions/2/files/theme-s0.plan.json", piece);
  response = http_exchange("GET", path, NULL, NULL, 0, NULL);
  check(response_status(response) == 404, "a theme copy the version does not hold is 404");
  free(response);
  snprintf(path, sizeof(path), "/api/materials/items/%s/versions/1/files/plan.json", piece);
  response = http_exchange("GET", path, NULL, NULL, 0, NULL);
  check(response_status(response) == 404, "a version without files is 404");
  free(response);
  snprintf(path, sizeof(path), "/api/materials/items/%s/versions/2/files/plan.json", piece);
  response = http_exchange("POST", path, NULL, "{}", 2, NULL);
  check(response_status(response) == 405, "the files route takes GET only");
  free(response);

  /* A theme version's own files still work beside a piece's. */
  snprintf(path, sizeof(path), "/api/materials/items/%s/versions/2/files/theme.mid", m->theme);
  response = http_exchange("GET", path, NULL, NULL, 0, NULL);
  check(response_status(response) == 200, "a theme version's files are still served");
  free(response);

  /* Deleting the item removes its version folders. */
  char dir[200];
  snprintf(dir, sizeof(dir), "materials/items/%s", piece);
  check(dir_exists(dir), "the item directory exists before the delete");
  snprintf(path, sizeof(path), "/api/materials/items/%s", piece);
  answer = exchange("DELETE", path, NULL, &status);
  cJSON_Delete(answer);
  check(status == 200 && !dir_exists(dir), "deleting the item removes its directory and the version folders");
  free(piece);
}

/* A brief of Library pieces is built into the version folder; the style, weight and without reach the request. */
static void test_brief(const Items *m) {
  int status = 0;
  char sections[5000];
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
  full_sections(m, sections, sizeof(sections));
  snprintf(more, sizeof(more),
           ",\"brief\":{\"scopes\":[],\"piece_ids\":[\"%s\",\"%s\",\"piece-0000000000000000\"],\"style_ids\":[]},"
           "\"style_weight\":\"1/4\",\"without\":[\"motive-rhythm\",\"motive-degrees\"],\"tempo\":132",
           first, second);
  char *piece = NULL;
  check(create_piece("Briefed", sections, more, &piece) == 200 && piece, "a piece with a brief is saved");
  cJSON *answer = compose(piece, "{\"seed\":2}", &status);
  check(status == 200 && ok_true(answer), "a piece with a brief composes");
  cJSON_Delete(answer);

  version_dir(piece, 2, dir, sizeof(dir));
  snprintf(path, sizeof(path), "%s/brief.style.json", dir);
  check(file_exists(path), "the brief is built into the version folder");
  answer = get_item(piece, &status);
  const cJSON *result = member(piece_of_answer(answer), "result");
  const cJSON *brief = member(result, "brief");
  check(number_of(brief, "piece_count") == 2, "the result counts the pieces used");
  const cJSON *skipped = member(brief, "skipped");
  check(cJSON_GetArraySize(skipped) == 1 && strcmp(string_of(cJSON_GetArrayItem(skipped, 0), "id"), "piece-0000000000000000") == 0,
        "the result lists the piece that was left out");
  check(cJSON_GetArraySize(member(result, "files")) == 9 && strcmp(cJSON_GetArrayItem(member(result, "files"), 1)->valuestring,
                                                                   "brief.style.json") == 0,
        "the result names the style file among the nine files");
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
  snprintf(path, sizeof(path), "/api/materials/items/%s/versions/2/files/brief.style.json", piece);
  char *response = http_exchange("GET", path, NULL, NULL, 0, NULL);
  cJSON *style_json = cJSON_Parse(response_body(response));
  check(response_status(response) == 200 && style_json, "the style file is served");
  cJSON_Delete(style_json);
  free(response);
  free(piece);

  /* A brief with nothing the Composer can use is 422 and leaves nothing. */
  snprintf(more, sizeof(more), ",\"brief\":{\"scopes\":[],\"piece_ids\":[\"piece-0000000000000000\"],\"style_ids\":[]}");
  create_piece("Empty brief", sections, more, &piece);
  answer = compose(piece, "{}", &status);
  check(status == 422, "a brief with no usable piece is 422");
  cJSON_Delete(answer);
  version_dir(piece, 2, dir, sizeof(dir));
  check(current_version(piece) == 1 && !dir_exists(dir), "an unusable brief writes no version and leaves no folder");
  free(piece);
  free(first);
  free(second);
}

/* update_uses moves an updated motif and an updated theme to their current versions before the compose. */
static void test_update_uses(const Items *m) {
  int status = 0;
  char sections[5000];
  char path[400];
  char dir[200];
  full_sections(m, sections, sizeof(sections));
  char *piece = NULL;
  create_piece("Updating", sections, "", &piece);

  /* A new version of motif A, analyzed by the real Composer, and a new composed version of the theme. */
  const char *fake = getenv("URSATZ_GUI_COMPOSER");
  char fake_path[600];
  snprintf(fake_path, sizeof(fake_path), "%s", fake ? fake : "");
  unsetenv("URSATZ_GUI_COMPOSER");
  snprintf(path, sizeof(path), "/api/materials/items/%s/versions", m->a);
  cJSON *answer = exchange("POST", path,
                           "{\"note\":\"changed\",\"body\":{\"uses\":[],\"motif\":{\"role\":\"melodic\",\"form\":\"full\","
                           "\"notes\":\"C4:e F4:e G4:e F4:e C4:e F4:e G4:e C5:e\",\"meter\":\"4/4\",\"key\":\"C minor\","
                           "\"pickup\":\"0\",\"features\":[],\"analysis\":null,\"derived\":null}}}",
                           &status);
  check(status == 200 && number_of(member(answer, "item"), "current_version") == 2, "motif A has a second version");
  cJSON_Delete(answer);
  setenv("URSATZ_GUI_COMPOSER", fake_path, 1);
  check(compose_theme(m->theme, 22) && current_version(m->theme) == 3, "the theme has a third version, composed with seed 22");

  answer = get_item(piece, &status);
  bool motif_updated = false;
  bool theme_updated = false;
  const cJSON *entry = NULL;
  cJSON_ArrayForEach(entry, member(answer, "uses_status")) {
    motif_updated = motif_updated || (strcmp(string_of(entry, "item_id"), m->a) == 0 && cJSON_IsTrue(member(entry, "updated")));
    theme_updated = theme_updated || (strcmp(string_of(entry, "item_id"), m->theme) == 0 && cJSON_IsTrue(member(entry, "updated")));
  }
  check(motif_updated && theme_updated, "the piece reports its uses of the motif and the theme as updated");
  cJSON_Delete(answer);

  /* Without update_uses the compose reads the copies the piece has. */
  answer = compose(piece, "{}", &status);
  cJSON_Delete(answer);
  cJSON *request = last_request();
  const cJSON *s1 = section_named(request, "s1");
  const cJSON *s2 = section_named(request, "s2");
  check(status == 200 && current_version(piece) == 2 && strcmp(string_of(member(member(s1, "fill"), "motive"), "degrees"), kDegreesA) == 0 &&
            file_has(string_of(member(s2, "fill"), "plan"), "\"seed\": 21"),
        "a compose without update_uses reads the copies it has");
  cJSON_Delete(request);

  /* With it the uses move first (a version each), then the compose runs on the new copies. */
  answer = compose(piece, "{\"update_uses\":true}", &status);
  check(status == 200 && number_of(member(answer, "item"), "current_version") == 5,
        "update_uses writes the two updates and the composed version");
  cJSON_Delete(answer);
  request = last_request();
  s1 = section_named(request, "s1");
  s2 = section_named(request, "s2");
  check(strcmp(string_of(member(member(s1, "fill"), "motive"), "degrees"), "1:e 4:e 5:e 4:e 1:e 4:e 5:e 1':e") == 0,
        "the motif fill is built from the updated copy");
  check(file_has(string_of(member(s2, "fill"), "plan"), "\"seed\": 22"), "the theme fill's plan is copied from the updated theme version");
  cJSON_Delete(request);
  answer = get_item(piece, &status);
  const cJSON *sections_json = member(member(piece_of_answer(answer), "structure"), "sections");
  const cJSON *uses = member(member(answer, "body"), "uses");
  check(number_of(member(cJSON_GetArrayItem(sections_json, 1), "fill"), "version") == 2 &&
            number_of(member(cJSON_GetArrayItem(sections_json, 2), "fill"), "version") == 3,
        "the motif fill names version 2 of A and the theme fill version 3 of the theme");
  bool uses_ok = cJSON_GetArraySize(uses) == 2;
  cJSON_ArrayForEach(entry, uses) {
    uses_ok = uses_ok && number_of(entry, "version") == (strcmp(string_of(entry, "item_id"), m->theme) == 0 ? 3 : 2);
  }
  check(uses_ok, "the uses name the current versions");
  const cJSON *versions = member(member(answer, "item"), "versions");
  check(strncmp(string_of(cJSON_GetArrayItem(versions, 2), "note"), "updated ", 8) == 0 &&
            strncmp(string_of(cJSON_GetArrayItem(versions, 3), "note"), "updated ", 8) == 0,
        "each update has its own version and note");
  cJSON_Delete(answer);
  version_dir(piece, 5, dir, sizeof(dir));
  snprintf(path, sizeof(path), "%s/theme-s2.plan.json", dir);
  check(file_exists(path), "the composed version holds the updated theme's plan");

  /* Nothing to update: the compose runs once. */
  answer = compose(piece, "{\"update_uses\":true}", &status);
  check(status == 200 && number_of(member(answer, "item"), "current_version") == 6,
        "update_uses with nothing updated writes only the composed version");
  cJSON_Delete(answer);
  free(piece);
}

/* The last line the fake Composer logged, newly allocated ("" when there is none). */
static char *last_argv(void) {
  char *log = read_file("fake/argv.log", NULL);
  if (!log) {
    return copy_string("");
  }
  size_t length = strlen(log);
  while (length > 0 && log[length - 1] == '\n') {
    log[--length] = '\0';
  }
  const char *line = strrchr(log, '\n');
  char *copy = copy_string(line ? line + 1 : log);
  free(log);
  return copy;
}

static cJSON *post_json(const char *id, const char *tail, const char *body, int *status) {
  char path[300];
  snprintf(path, sizeof(path), "/api/materials/items/%s/%s", id, tail);
  return exchange("POST", path, body, status);
}

/* The status of a POST whose answer is not needed. */
static int post_status(const char *id, const char *tail, const char *body) {
  int status = 0;
  cJSON_Delete(post_json(id, tail, body, &status));
  return status;
}

static bool version_files_exist(const char *id, int version, bool with_theme) {
  static const char *const files[] = {"request.json",  "plan.json",  "ending-plan.json", "loop.mid",
                                      "loop.musicxml", "ending.mid", "ending.musicxml",  "theme-s2.plan.json"};
  char dir[200];
  char path[400];
  bool all = true;
  version_dir(id, version, dir, sizeof(dir));
  for (size_t i = 0; i < (with_theme ? 8 : 7); i++) {
    snprintf(path, sizeof(path), "%s/%s", dir, files[i]);
    all = all && file_exists(path);
  }
  return all;
}

/* Regenerate, variations and choose on a composed piece (R-173). */
static void test_regenerate(const Items *m, const char *ok_script, const char *stuck_script) {
  int status = 0;
  char sections[5000];
  char path[400];
  char dir[200];
  full_sections(m, sections, sizeof(sections));
  char *piece = NULL;
  create_piece("Regenerated", sections, ",\"check\":true", &piece);
  cJSON *answer = compose(piece, "{\"seed\":3}", &status);
  cJSON_Delete(answer);
  check(piece && status == 200 && current_version(piece) == 2, "the piece to regenerate is composed (version 2)");
  if (!piece) {
    return;
  }

  /* Regenerate a lane of a motif section, with a seed. */
  answer = post_json(piece, "versions/2/regenerate", "{\"section_id\":\"s1\",\"lane\":\"melody\",\"seed\":9}", &status);
  check(status == 200 && ok_true(answer) && number_of(member(answer, "item"), "current_version") == 3,
        "regenerate answers 200 and makes version 3");
  const cJSON *versions = member(member(answer, "item"), "versions");
  check(strcmp(string_of(cJSON_GetArrayItem(versions, 2), "note"), "regenerated s1 melody seed 9") == 0, "the note names the section, the lane and the seed");
  cJSON_Delete(answer);
  check(version_files_exist(piece, 3, true), "the new version holds the eight files");
  char *argv = last_argv();
  check(strncmp(argv, "piece --from ", 13) == 0 && strstr(argv, "/v2/plan.json --request ") && strstr(argv, "/v3/request.json") &&
            strstr(argv, "--regenerate s1") && strstr(argv, "--lane melody") && strstr(argv, "--seed 9") && strstr(argv, "--check") &&
            strstr(argv, "--ending-out") && strstr(argv, "ending.musicxml"),
        "the Composer got the plan of version 2, the request of version 3, the section, the lane, the seed and the eight outputs");
  free(argv);
  cJSON *request = last_request();
  const char *plan = string_of(member(section_named(request, "s2"), "fill"), "plan");
  const char *suffix = "/v3/theme-s2.plan.json";
  check(strlen(plan) > strlen(suffix) && strcmp(plan + strlen(plan) - strlen(suffix), suffix) == 0 && file_exists(plan),
        "the new request names the copy of the theme plan in its own folder");
  cJSON_Delete(request);
  answer = get_item(piece, &status);
  const cJSON *result = member(piece_of_answer(answer), "result");
  check(number_of(result, "seed") == 9 && cJSON_IsObject(member(result, "summary")) && cJSON_GetArraySize(member(result, "files")) == 8 &&
            cJSON_GetArraySize(member(member(answer, "body"), "uses")) == 2 &&
            cJSON_GetArraySize(member(member(piece_of_answer(answer), "structure"), "sections")) == 6,
        "the new version keeps the structure and the uses and holds the new result");
  cJSON_Delete(answer);
  snprintf(path, sizeof(path), "/api/materials/items/%s/versions/2", piece);
  answer = exchange("GET", path, NULL, &status);
  check(number_of(member(piece_of_answer(answer), "result"), "seed") == 3, "the version it came from is unchanged");
  cJSON_Delete(answer);

  /* No lane and no seed: the whole section, and the Composer is left to choose. */
  answer = post_json(piece, "versions/3/regenerate", "{\"section_id\":\"s0\"}", &status);
  versions = member(member(answer, "item"), "versions");
  check(status == 200 && strcmp(string_of(cJSON_GetArrayItem(versions, 3), "note"), "regenerated s0 all seed 5") == 0,
        "a regenerate with no lane writes the whole section (note lane all)");
  cJSON_Delete(answer);
  argv = last_argv();
  check(strstr(argv, "/v3/plan.json") && strstr(argv, "--regenerate s0") && !strstr(argv, "--lane") && !strstr(argv, "--seed"),
        "no lane and no seed are passed on");
  free(argv);

  /* A section filled with a theme takes harmony and bass only. */
  check(post_status(piece, "versions/4/regenerate", "{\"section_id\":\"s2\",\"lane\":\"melody\"}") == 400, "a theme section's melody is refused");
  check(post_status(piece, "versions/4/regenerate", "{\"section_id\":\"s2\",\"lane\":\"all\"}") == 400, "a theme section's lane all is refused");
  check(post_status(piece, "versions/4/regenerate", "{\"section_id\":\"s2\"}") == 400, "a theme section with no lane is refused");
  check(current_version(piece) == 4, "the refused calls wrote no version");
  answer = post_json(piece, "versions/4/regenerate", "{\"section_id\":\"s2\",\"lane\":\"harmony\"}", &status);
  check(status == 200 && number_of(member(answer, "item"), "current_version") == 5, "a theme section's harmony is regenerated");
  cJSON_Delete(answer);
  check(post_status(piece, "versions/5/regenerate", "{\"section_id\":\"s2\",\"lane\":\"bass\"}") == 200, "a theme section's bass is regenerated");

  /* What is refused. */
  int before = current_version(piece);
  check(post_status(piece, "versions/6/regenerate", "{\"section_id\":\"s9\"}") == 400, "a section the piece does not have is 400");
  check(post_status(piece, "versions/6/regenerate", "{\"section_id\":\"end\"}") == 400, "an ending section is 400");
  check(post_status(piece, "versions/6/regenerate", "{\"section_id\":\"s1\",\"lane\":\"tuba\"}") == 400, "an unknown lane is 400");
  check(post_status(piece, "versions/6/regenerate", "{\"lane\":\"melody\"}") == 400, "no section id is 400");
  check(post_status(piece, "versions/6/regenerate", "{\"section_id\":\"s1\",\"seed\":-1}") == 400, "a bad seed is 400");
  check(post_status(piece, "versions/1/regenerate", "{\"section_id\":\"s1\"}") == 400, "a version that was never composed is 400");
  check(post_status(piece, "versions/99/regenerate", "{\"section_id\":\"s1\"}") == 404, "a version that does not exist is 404");
  check(post_status(piece, "versions/x/regenerate", "{\"section_id\":\"s1\"}") == 404, "a version that is no number is 404");
  check(post_status("pce-0123456789abcdef", "versions/1/regenerate", "{\"section_id\":\"s1\"}") == 404, "a piece that does not exist is 404");
  check(post_status(m->theme, "versions/2/regenerate", "{\"section_id\":\"s1\"}") == 400, "a theme version is 400 for regenerate");
  check(post_status(m->theme, "versions/2/variations", "{\"section_id\":\"s1\",\"lane\":\"melody\",\"count\":2}") == 400,
        "a theme version is 400 for variations");
  check(post_status(m->theme, "versions/2/variations/1/choose", "{\"index\":1}") == 400, "a theme version is 400 for choose");
  check(post_status(m->a, "versions/1/regenerate", "{\"section_id\":\"s1\"}") == 400, "a motif is 400 for regenerate");
  check(current_version(piece) == before, "the refused calls wrote no version");

  /* A stuck run writes no version and leaves no folder. */
  setenv("URSATZ_GUI_COMPOSER", stuck_script, 1);
  answer = post_json(piece, "versions/6/regenerate", "{\"section_id\":\"s1\",\"lane\":\"melody\"}", &status);
  check(status == 422 && strstr(string_of(answer, "error"), "no seed works") != NULL, "a stuck regenerate is 422 with the Composer's message");
  cJSON_Delete(answer);
  version_dir(piece, before + 1, dir, sizeof(dir));
  check(current_version(piece) == before && !dir_exists(dir), "a stuck regenerate writes no version and leaves no folder");
  answer = post_json(piece, "versions/6/variations", "{\"section_id\":\"s1\",\"lane\":\"melody\",\"count\":2}", &status);
  check(status == 422, "stuck variations are 422");
  cJSON_Delete(answer);
  snprintf(path, sizeof(path), "materials/items/%s/v6/variations-1", piece);
  check(!dir_exists(path), "stuck variations leave no set folder");
  setenv("URSATZ_GUI_COMPOSER", ok_script, 1);

  /* Variations: a set of three, in the folder of the version they were made from. */
  answer = post_json(piece, "versions/6/variations", "{\"section_id\":\"s1\",\"lane\":\"melody\",\"count\":3,\"seed\":20}", &status);
  check(status == 200 && ok_true(answer) && number_of(answer, "set") == 1, "variations answer 200 with set 1");
  const cJSON *list = member(answer, "variations");
  check(cJSON_GetArraySize(list) == 3 && number_of(cJSON_GetArrayItem(list, 0), "index") == 1 &&
            number_of(cJSON_GetArrayItem(list, 0), "seed") == 20 && number_of(cJSON_GetArrayItem(list, 1), "seed") == 21 &&
            number_of(cJSON_GetArrayItem(list, 2), "seed") == 22,
        "three variations with distinct seeds");
  const cJSON *second = cJSON_GetArrayItem(list, 1);
  check(number_of(member(second, "score"), "numerator") == 2 && number_of(member(second, "seam"), "held") == 4 &&
            strcmp(string_of(second, "heard"), "IAC") == 0,
        "each variation has its score, seam and cadence heard");
  cJSON_Delete(answer);
  check(current_version(piece) == before, "variations write no version");
  argv = last_argv();
  check(strstr(argv, "--variations s1") && strstr(argv, "--lane melody") && strstr(argv, "--count 3") && strstr(argv, "/v6/variations-1") &&
            strstr(argv, "/v6/request.json") && strstr(argv, "--seed 20") && strstr(argv, "--check"),
        "the Composer got the section, the lane, the count, the set folder and the seed");
  free(argv);
  static const char *const set_files[] = {"result.json",       "variation-1.json", "variation-1.mid", "variation-1.musicxml",
                                          "variation-3.json", "variation-3.mid",  "variation-3.musicxml"};
  static const char *const set_types[] = {"application/json", "application/json", "audio/midi",       "application/xml",
                                          "application/json", "audio/midi",       "application/xml"};
  for (size_t i = 0; i < sizeof(set_files) / sizeof(set_files[0]); i++) {
    snprintf(path, sizeof(path), "/api/materials/items/%s/versions/6/variations/1/%s", piece, set_files[i]);
    char *response = http_exchange("GET", path, NULL, NULL, 0, NULL);
    char type[128];
    char what[160];
    content_type_of(response, type, sizeof(type));
    snprintf(what, sizeof(what), "%s of the set is served as %s", set_files[i], set_types[i]);
    check(response_status(response) == 200 && strcmp(type, set_types[i]) == 0 && strlen(response_body(response)) > 0, what);
    free(response);
  }
  static const char *const refused[] = {"item.json", "variation-123.json", "variation-1.mid.bak", "variation-.json", "..%2Frequest.json", "variation-1.MID"};
  for (size_t i = 0; i < sizeof(refused) / sizeof(refused[0]); i++) {
    char what[160];
    snprintf(path, sizeof(path), "/api/materials/items/%s/versions/6/variations/1/%s", piece, refused[i]);
    char *response = http_exchange("GET", path, NULL, NULL, 0, NULL);
    snprintf(what, sizeof(what), "%s of a set is not served", refused[i]);
    check(response_status(response) == 404, what);
    free(response);
  }
  snprintf(path, sizeof(path), "/api/materials/items/%s/versions/6/variations/2/result.json", piece);
  char *response = http_exchange("GET", path, NULL, NULL, 0, NULL);
  check(response_status(response) == 404, "a set that does not exist is 404");
  free(response);
  snprintf(path, sizeof(path), "/api/materials/items/%s/versions/5/variations/1/result.json", piece);
  response = http_exchange("GET", path, NULL, NULL, 0, NULL);
  check(response_status(response) == 404, "a set is found in the version that holds it only");
  free(response);
  snprintf(path, sizeof(path), "/api/materials/items/%s/versions/6/variations/1/result.json", piece);
  response = http_exchange("POST", path, NULL, "{}", 2, NULL);
  check(response_status(response) == 405, "a set file takes GET only");
  free(response);

  /* The next set is numbered across the versions. */
  answer = post_json(piece, "versions/5/variations", "{\"section_id\":\"s0\",\"lane\":\"bass\",\"count\":2}", &status);
  check(status == 200 && number_of(answer, "set") == 2 && cJSON_GetArraySize(member(answer, "variations")) == 2,
        "a set made in another version is numbered after every set of the item");
  cJSON_Delete(answer);
  argv = last_argv();
  check(strstr(argv, "/v5/variations-2") && !strstr(argv, "--seed"), "the second set is in the folder of version 5, with no seed passed");
  free(argv);
  check(post_status(piece, "versions/6/variations", "{\"section_id\":\"s1\",\"lane\":\"melody\",\"count\":9}") == 400, "nine variations are refused");
  check(post_status(piece, "versions/6/variations", "{\"section_id\":\"s1\",\"lane\":\"melody\",\"count\":0}") == 400, "no variations are refused");
  check(post_status(piece, "versions/6/variations", "{\"section_id\":\"s1\",\"count\":2}") == 400, "variations with no lane are refused");
  check(post_status(piece, "versions/6/variations", "{\"section_id\":\"s2\",\"lane\":\"melody\",\"count\":2}") == 400,
        "a theme section's melody variations are refused");
  check(post_status(piece, "versions/6/variations", "{\"section_id\":\"s2\",\"lane\":\"harmony\",\"count\":2}") == 200,
        "harmonize a theme section: its harmony variations are made");
  check(post_status(piece, "versions/6/variations", "{\"section_id\":\"s7\",\"lane\":\"melody\",\"count\":2}") == 400, "variations of an unknown section are refused");
  check(post_status(piece, "versions/6/variations", "{\"section_id\":\"s1\",\"lane\":\"melody\",\"count\":2,\"seed\":1.5}") == 400,
        "variations with a bad seed are refused");

  /* Choose: the variation's plan becomes the next version, and the ending is built from it. */
  answer = post_json(piece, "versions/6/variations/1/choose", "{\"index\":2}", &status);
  check(status == 200 && ok_true(answer) && number_of(member(answer, "item"), "current_version") == before + 1,
        "choose answers 200 and writes the next version");
  versions = member(member(answer, "item"), "versions");
  check(strcmp(string_of(cJSON_GetArrayItem(versions, before), "note"), "chose variation 2 seed 21") == 0, "the note names the variation and its seed");
  cJSON_Delete(answer);
  check(version_files_exist(piece, before + 1, true), "the chosen version holds the eight files");
  version_dir(piece, before + 1, dir, sizeof(dir));
  snprintf(path, sizeof(path), "%s/plan.json", dir);
  check(file_has(path, "variations-1/variation-2.json"), "the Composer built the version from the chosen plan");
  argv = last_argv();
  check(strstr(argv, "--from ") && strstr(argv, "/v6/variations-1/variation-2.json") && strstr(argv, "--regenerate none") && !strstr(argv, "--lane") &&
            strstr(argv, "/request.json"),
        "the Composer got the chosen plan and --regenerate none");
  free(argv);
  answer = get_item(piece, &status);
  result = member(piece_of_answer(answer), "result");
  check(number_of(result, "seed") == 21 && number_of(member(result, "summary"), "seed") == 21 &&
            cJSON_IsObject(member(member(member(result, "summary"), "versions"), "loop")) &&
            cJSON_IsObject(member(member(member(result, "summary"), "versions"), "ending")),
        "the result holds the variation's seed and both versions");
  const cJSON *loop = member(member(member(result, "summary"), "versions"), "loop");
  check(cJSON_GetArraySize(member(loop, "sections")) == 1 && number_of(member(loop, "score"), "numerator") == 2 &&
            number_of(member(loop, "seam"), "held") == 4,
        "the loop version in the result is the variation's");
  cJSON_Delete(answer);
  check(post_status(piece, "versions/6/variations/1/choose", "{\"index\":9}") == 404, "a variation the set does not have is 404");
  check(post_status(piece, "versions/6/variations/9/choose", "{\"index\":1}") == 404, "a set that does not exist is 404");
  check(post_status(piece, "versions/6/variations/1/choose", "{\"index\":\"x\"}") == 400, "an index that is no number is 400");
  check(post_status(piece, "versions/6/variations/1/choose", "{}") == 400, "no index is 400");
  check(post_status(piece, "versions/3/variations/1/choose", "{\"index\":1}") == 404, "a set is chosen from the version that holds it only");
  check(current_version(piece) == before + 1, "the refused chooses wrote no version");

  /* Deleting the item removes the variation sets with it. */
  snprintf(dir, sizeof(dir), "materials/items/%s", piece);
  snprintf(path, sizeof(path), "/api/materials/items/%s", piece);
  answer = exchange("DELETE", path, NULL, &status);
  cJSON_Delete(answer);
  check(status == 200 && !dir_exists(dir), "deleting the item removes its variation sets");
  free(piece);
}

/* The real Composer: a structure of empty fills (the Composer's own structure-only example). */
static void test_real_composer(void) {
  int status = 0;
  char path[400];
  char dir[200];
  unsetenv("URSATZ_GUI_COMPOSER");
  char *piece = NULL;
  int created = create_piece("Real",
                             "[{\"id\":\"s0\",\"part\":\"intro\",\"letter\":\"I\",\"beats\":8,\"pickup\":\"0\",\"cadence\":null,\"fill\":null},"
                             "{\"id\":\"s1\",\"part\":\"body\",\"letter\":\"A\",\"beats\":32,\"pickup\":\"0\",\"cadence\":null,\"fill\":null},"
                             "{\"id\":\"s2\",\"part\":\"body\",\"letter\":\"B\",\"beats\":6,\"pickup\":\"0\",\"cadence\":null,\"fill\":null},"
                             "{\"id\":\"s3\",\"part\":\"turnaround\",\"letter\":\"T\",\"beats\":4,\"pickup\":\"0\",\"cadence\":null,\"fill\":null}]",
                             "", &piece);
  check(created == 200 && piece, "the piece for the real Composer is saved");
  cJSON *answer = compose(piece, "{\"seed\":1}", &status);
  check(status == 200 && ok_true(answer), "the real Composer composes the piece (exit 0)");
  cJSON_Delete(answer);
  version_dir(piece, 2, dir, sizeof(dir));
  static const char *const files[] = {"request.json",  "plan.json",  "ending-plan.json", "loop.mid",
                                      "loop.musicxml", "ending.mid", "ending.musicxml"};
  bool all = true;
  for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); i++) {
    snprintf(path, sizeof(path), "%s/%s", dir, files[i]);
    char *text = read_file(path, NULL);
    all = all && text && strlen(text) > 0;
    free(text);
  }
  check(all, "the real run wrote the seven files");
  answer = get_item(piece, &status);
  const cJSON *result = member(piece_of_answer(answer), "result");
  check(number_of(result, "seed") >= 0 && cJSON_IsObject(member(result, "loop")) && cJSON_IsObject(member(result, "summary")),
        "the result has the seed, the loop region and the Composer's summary");
  cJSON_Delete(answer);

  /* Regenerate, variations and choose with the real Composer. */
  answer = post_json(piece, "versions/2/regenerate", "{\"section_id\":\"s1\",\"lane\":\"melody\",\"seed\":1}", &status);
  check(status == 200 && number_of(member(answer, "item"), "current_version") == 3 && version_files_exist(piece, 3, false),
        "the real Composer regenerates a section (version 3 holds the seven files)");
  cJSON_Delete(answer);
  answer = post_json(piece, "versions/3/variations", "{\"section_id\":\"s1\",\"lane\":\"melody\",\"count\":2,\"seed\":1}", &status);
  int made = cJSON_GetArraySize(member(answer, "variations"));
  check(status == 200 && number_of(answer, "set") == 1 && made >= 1 && cJSON_IsObject(member(cJSON_GetArrayItem(member(answer, "variations"), 0), "score")),
        "the real Composer makes variations with their scores");
  cJSON_Delete(answer);
  snprintf(path, sizeof(path), "/api/materials/items/%s/versions/3/variations/1/variation-1.mid", piece);
  char *response = http_exchange("GET", path, NULL, NULL, 0, NULL);
  check(response_status(response) == 200 && strncmp(response_body(response), "MThd", 4) == 0, "a variation's MIDI file is served");
  free(response);
  answer = post_json(piece, "versions/3/variations/1/choose", "{\"index\":1}", &status);
  check(status == 200 && number_of(member(answer, "item"), "current_version") == 4 && version_files_exist(piece, 4, false),
        "the real Composer builds the ending version of a chosen variation (version 4 holds the seven files)");
  cJSON_Delete(answer);
  free(piece);
}

int main(void) {
  system("rm -rf materials subjects pieces collections styles fake");
  mkdir("fake", 0755);

  char cwd[512];
  if (!getcwd(cwd, sizeof(cwd))) {
    return 1;
  }
  char ok_script[600];
  char stuck_script[600];
  char usage_script[600];
  snprintf(ok_script, sizeof(ok_script), "%s/fake/ok.sh", cwd);
  snprintf(stuck_script, sizeof(stuck_script), "%s/fake/stuck.sh", cwd);
  snprintf(usage_script, sizeof(usage_script), "%s/fake/usage.sh", cwd);
  char script[4600];
  snprintf(script, sizeof(script),
           "#!/bin/sh\n"
           "[ \"$2\" = \"--from\" ] || cp \"$2\" \"%s/fake/last-request.json\"\n"
           "echo \"$@\" >> \"%s/fake/argv.log\"\n"
           "seed=5\n"
           "while [ $# -gt 0 ]; do\n"
           "  case \"$1\" in\n"
           "    --from) from=\"$2\"; shift ;;\n"
           "    --request) cp \"$2\" \"%s/fake/last-request.json\"; shift ;;\n"
           "    --variations) section=\"$2\"; shift ;;\n"
           "    --count) count=\"$2\"; shift ;;\n"
           "    --out-dir) outdir=\"$2\"; shift ;;\n"
           "    --out) out=\"$2\"; shift ;;\n"
           "    --ending-out) ending_out=\"$2\"; shift ;;\n"
           "    --midi) midi=\"$2\"; shift ;;\n"
           "    --musicxml) xml=\"$2\"; shift ;;\n"
           "    --ending-midi) ending_midi=\"$2\"; shift ;;\n"
           "    --ending-musicxml) ending_xml=\"$2\"; shift ;;\n"
           "    --seed) seed=\"$2\"; shift ;;\n"
           "  esac\n"
           "  shift\n"
           "done\n"
           "if [ -n \"$outdir\" ]; then\n"
           "  mkdir -p \"$outdir\"\n"
           "  i=1; s=$seed; entries=''; sep=''\n"
           "  while [ $i -le $count ]; do\n"
           "    echo \"{\\\"variation\\\": $i}\" > \"$outdir/variation-$i.json\"\n"
           "    printf 'MThd\\n' > \"$outdir/variation-$i.mid\"\n"
           "    echo '<score-partwise/>' > \"$outdir/variation-$i.musicxml\"\n"
           "    entries=\"$entries$sep{\\\"index\\\":$i,\\\"seed\\\":$s,\\\"midi\\\":\\\"m\\\",\\\"musicxml\\\":\\\"x\\\",\\\"loop\\\":{\\\"sections\\\":[{\\\"id\\\":\\\"$section\\\",\\\"heard\\\":\\\"IAC\\\"}],\\\"score\\\":{\\\"numerator\\\":$i,\\\"denominator\\\":7},\\\"seam\\\":{\\\"held\\\":4}}}\"\n"
           "    sep=,; i=$((i+1)); s=$((s+1))\n"
           "  done\n"
           "  echo \"{\\\"schema\\\":\\\"ursatz-piece-variations\\\",\\\"variations\\\":[$entries]}\" | tee \"$outdir/result.json\"\n"
           "  exit 0\n"
           "fi\n"
           "echo \"{\\\"seed\\\": $seed, \\\"from\\\": \\\"$from\\\"}\" > \"$out\"\n"
           "[ -n \"$ending_out\" ] && echo '{}' > \"$ending_out\"\n"
           "printf 'MThd\\n' > \"$midi\"\n"
           "echo '<score-partwise/>' > \"$xml\"\n"
           "[ -n \"$ending_midi\" ] && printf 'MThd\\n' > \"$ending_midi\"\n"
           "[ -n \"$ending_xml\" ] && echo '<score-partwise/>' > \"$ending_xml\"\n"
           "echo \"{\\\"seed\\\": $seed, \\\"themes\\\": [{\\\"seed\\\": $seed}], \\\"versions\\\": {\\\"loop\\\": {}, \\\"ending\\\": {}}, \\\"loop\\\": {\\\"start\\\": \\\"2\\\", \\\"end\\\": \\\"10\\\"}}\"\n",
           cwd, cwd, cwd);
  check(write_script(ok_script, script), "the recording fake Composer is written");
  check(write_script(stuck_script, "#!/bin/sh\necho 'no seed works' >&2\nexit 2\n"), "the stuck fake Composer is written");
  check(write_script(usage_script, "#!/bin/sh\necho 'the request does not read' >&2\nexit 1\n"),
        "the usage-error fake Composer is written");

  struct mg_context *ctx = ursatz_gui_server_start("18774", ".");
  check(ctx != NULL, "server starts");
  if (!ctx) {
    return 1;
  }
  sleep_ms(200);

  int status = 0;
  cJSON *projects = exchange("GET", "/api/materials/projects", NULL, &status);
  snprintf(g_project, sizeof(g_project), "%s", string_of(cJSON_GetArrayItem(member(projects, "projects"), 0), "id"));
  cJSON_Delete(projects);

  /* The motifs are saved with the real Composer; the themes are composed with the fake. */
  unsetenv("URSATZ_GUI_COMPOSER");
  Items items;
  memset(&items, 0, sizeof(items));
  bool made = make_items(&items);
  check(made, "the motifs, their forms and the themes are saved");
  if (made) {
    setenv("URSATZ_GUI_COMPOSER", ok_script, 1);
    check(compose_theme(items.theme, 21) && current_version(items.theme) == 2, "the theme is composed (version 2, seed 21)");
    test_bodies(&items);
    test_compose(&items);
    test_files(&items);
    test_brief(&items);
    test_update_uses(&items);
    test_regenerate(&items, ok_script, stuck_script);
    test_failures(&items, stuck_script, usage_script);
    test_real_composer();
  }

  ursatz_gui_server_stop(ctx);
  if (failures > 0) {
    fprintf(stderr, "%d check(s) failed\n", failures);
    return 1;
  }
  printf("all checks passed\n");
  return 0;
}
