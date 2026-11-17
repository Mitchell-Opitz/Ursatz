/* Confirms the Collections routes end to end (they keep the /api/corpus
 * paths): create with a description, list order, read, rename, delete, bulk
 * add, removal, the cleanup when a piece is deleted, persistence across a
 * restart. Uses the shared HTTP client (tests/support/test_http.h). Runs in its own working directory (collections/ and
 * pieces/ are created there). */

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

#include "gui_check.h"
#include "server.h"
#include "test_http.h"

#define PORT "18767"

/* One request to the server on PORT through the shared client; the whole response (status line, headers, body) is copied
 * into response (caller-sized, NUL-terminated, cut off when it does not fit). body may be NULL for a bodyless request.
 * False when nothing came back. */
static bool exchange_into(const char *method, const char *path, const char *body, char *response,
                          size_t response_capacity) {
  char *received = test_http_exchange(PORT, method, path, NULL, body, body ? strlen(body) : 0);
  bool answered = received && received[0] != '\0';
  snprintf(response, response_capacity, "%s", received ? received : "");
  free(received);
  return answered;
}

static bool has(const char *response, const char *text) {
  return strstr(response, text) != NULL;
}

/* True when `first` appears before `second` in response. */
static bool before(const char *response, const char *first, const char *second) {
  const char *a = strstr(response, first);
  const char *b = strstr(response, second);
  return a && b && a < b;
}

/* Copies the first "id":"<value>" of a response body into out. */
static bool extract_id(const char *response, char *out, size_t out_size) {
  const char *key = strstr(response, "\"id\":\"");
  if (!key) {
    return false;
  }
  key += strlen("\"id\":\"");
  const char *end = strchr(key, '"');
  if (!end || (size_t)(end - key) >= out_size) {
    return false;
  }
  memcpy(out, key, (size_t)(end - key));
  out[end - key] = '\0';
  return true;
}

static void reset_directories(void) {
#ifdef _WIN32
  system("rmdir /s /q collections pieces 2> nul");
#else
  system("rm -rf collections pieces");
#endif
}

/* A piece the routes can see: pieces/<id>/meta.json. */
static void make_piece(const char *id) {
  char path[128];
  snprintf(path, sizeof(path), "pieces/%s", id);
  MKDIR(path);
  snprintf(path, sizeof(path), "pieces/%s/meta.json", id);
  const char *meta = "{\"filename\":\"piece.mid\",\"format\":\"midi\",\"uploaded_at\":\"2026-01-01T00:00:00Z\"}";
  test_write_file(path, meta, strlen(meta));
}

int main(void) {
  printf("-- collection routes\n");
  reset_directories();
  MKDIR("pieces");
  make_piece("piece-1");
  make_piece("piece-2");
  make_piece("piece-3");

  struct mg_context *ctx = ursatz_gui_server_start(PORT, ".");
  check(ctx != NULL, "server starts");
  if (!ctx) {
    return 1;
  }

  test_sleep_ms(200);

  char response[8192];

  /* Create with an id, a name and a description (R-098: the id is still accepted). */
  check(exchange_into("POST", "/api/corpus",
                     "{\"id\":\"test-corpus\",\"name\":\"Test  Corpus \",\"description\":\" First   collection \"}",
                     response, sizeof(response)),
        "create: got a response");
  check(has(response, "\"ok\":true"), "create succeeds");
  check(has(response, "\"name\":\"Test Corpus\""), "create normalizes the name");
  check(has(response, "\"description\":\"First collection\""), "create stores the description");
  check(has(response, "\"piece_count\":0"), "create starts with no pieces");

  /* Duplicate id fails. */
  check(exchange_into("POST", "/api/corpus", "{\"id\":\"test-corpus\",\"name\":\"Again\"}", response, sizeof(response)),
        "duplicate create: got a response");
  check(has(response, "\"ok\":false"), "duplicate create fails");

  /* An empty name and a bad id fail. */
  check(exchange_into("POST", "/api/corpus", "{\"name\":\"   \"}", response, sizeof(response)),
        "empty name: got a response");
  check(has(response, "\"ok\":false") && has(response, "invalid name"), "empty name fails");
  check(exchange_into("POST", "/api/corpus", "{\"id\":\"bad!id\",\"name\":\"x\"}", response, sizeof(response)),
        "bad id: got a response");
  check(has(response, "\"ok\":false") && has(response, "invalid id"), "bad id fails");

  /* Name only: the id is generated and returned. */
  check(exchange_into("POST", "/api/corpus", "{\"name\":\"beta collection\"}", response, sizeof(response)),
        "name-only create: got a response");
  check(has(response, "\"ok\":true"), "name-only create succeeds");
  check(has(response, "\"id\":\"col-"), "name-only create returns a generated col- id");
  char second_id[64] = "";
  check(extract_id(response, second_id, sizeof(second_id)), "name-only create: id read");

  check(exchange_into("POST", "/api/corpus", "{\"name\":\"Alpha collection\",\"description\":\"d\"}", response,
                     sizeof(response)),
        "third create: got a response");
  check(has(response, "\"ok\":true"), "third create succeeds");

  /* A quote in a name round-trips as valid JSON. */
  check(exchange_into("POST", "/api/corpus", "{\"name\":\"The \\\"Quoted\\\" one\"}", response, sizeof(response)),
        "quoted name: got a response");
  check(has(response, "\"name\":\"The \\\"Quoted\\\" one\""), "quote in a name is escaped in the answer");

  /* List: by name, case-insensitive. */
  check(exchange_into("GET", "/api/corpus", NULL, response, sizeof(response)), "list: got a response");
  check(has(response, "\"ok\":true") && has(response, "\"id\":\"test-corpus\""), "list includes the created collection");
  check(has(response, "\"updated_at\":\"") && has(response, "\"piece_count\":0"), "list carries counts and dates");
  check(before(response, "Alpha collection", "beta collection") && before(response, "beta collection", "Test Corpus"),
        "list is sorted by name, case-insensitively");
  check(has(response, "The \\\"Quoted\\\" one"), "list escapes a quote in a name");

  /* Invalid collection id in the path is rejected before reaching the service. */
  check(exchange_into("GET", "/api/corpus/bad!id/pieces", NULL, response, sizeof(response)),
        "invalid corpus id: got a response");
  check(has(response, "\"ok\":false"), "invalid corpus id in path rejected");

  /* Read one. */
  check(exchange_into("GET", "/api/corpus/test-corpus", NULL, response, sizeof(response)), "get: got a response");
  check(has(response, "\"ok\":true") && has(response, "\"pieces\":[]") && has(response, "\"created_at\":\""),
        "get returns the collection with its piece ids");
  check(exchange_into("GET", "/api/corpus/no-such", NULL, response, sizeof(response)), "get unknown: got a response");
  check(test_http_status(response) == 404 && has(response, "\"ok\":false"), "get unknown collection is 404");

  /* Rename and describe. */
  check(exchange_into("PATCH", "/api/corpus/test-corpus", "{\"name\":\"Renamed   One\"}", response, sizeof(response)),
        "rename: got a response");
  check(has(response, "\"ok\":true") && has(response, "\"name\":\"Renamed One\""), "rename normalizes and answers");
  check(has(response, "\"description\":\"First collection\""), "rename leaves the description");
  check(exchange_into("PATCH", "/api/corpus/test-corpus", "{\"description\":\"\"}", response, sizeof(response)),
        "clear description: got a response");
  check(has(response, "\"ok\":true") && has(response, "\"description\":\"\""), "an empty description clears it");
  check(exchange_into("PATCH", "/api/corpus/test-corpus", "{\"name\":\"  \"}", response, sizeof(response)),
        "empty rename: got a response");
  check(has(response, "\"ok\":false") && has(response, "invalid name"), "an empty name is rejected");
  check(exchange_into("PATCH", "/api/corpus/test-corpus", "{}", response, sizeof(response)),
        "empty patch: got a response");
  check(has(response, "\"ok\":false") && has(response, "nothing to update"), "nothing to update");
  check(exchange_into("PATCH", "/api/corpus/no-such", "{\"name\":\"x\"}", response, sizeof(response)),
        "rename unknown: got a response");
  check(test_http_status(response) == 404, "rename unknown collection is 404");

  /* Add one piece, then several. */
  check(exchange_into("POST", "/api/corpus/test-corpus/pieces", "{\"piece_id\":\"piece-1\"}", response,
                     sizeof(response)),
        "add piece: got a response");
  check(has(response, "\"ok\":true") && has(response, "\"added\":1") && has(response, "\"already\":0"),
        "add piece succeeds");
  check(exchange_into("POST", "/api/corpus/test-corpus/pieces",
                     "{\"piece_ids\":[\"piece-1\",\"piece-2\",\"piece-3\",\"piece-2\"]}", response, sizeof(response)),
        "bulk add: got a response");
  check(has(response, "\"ok\":true") && has(response, "\"added\":2") && has(response, "\"already\":2"),
        "bulk add counts added and already-members (a repeat in the request too)");

  /* An unknown piece is a 400 naming it. */
  check(exchange_into("POST", "/api/corpus/test-corpus/pieces", "{\"piece_ids\":[\"piece-3\",\"no-such-piece\"]}",
                     response, sizeof(response)),
        "unknown piece: got a response");
  check(test_http_status(response) == 400 && has(response, "unknown piece: no-such-piece"), "unknown piece is 400 and named");
  check(exchange_into("POST", "/api/corpus/no-such/pieces", "{\"piece_id\":\"piece-1\"}", response, sizeof(response)),
        "add to unknown collection: got a response");
  check(test_http_status(response) == 404, "adding to an unknown collection is 404");
  check(exchange_into("POST", "/api/corpus/test-corpus/pieces", "{\"piece_id\":\"bad!piece\"}", response,
                     sizeof(response)),
        "bad piece id: got a response");
  check(has(response, "\"ok\":false") && has(response, "invalid piece_id"), "bad piece id rejected");

  /* List the members: ids in add order. */
  check(exchange_into("GET", "/api/corpus/test-corpus/pieces", NULL, response, sizeof(response)),
        "list pieces: got a response");
  check(has(response, "\"ok\":true") && before(response, "\"id\":\"piece-1\"", "\"id\":\"piece-2\"") &&
            before(response, "\"id\":\"piece-2\"", "\"id\":\"piece-3\""),
        "list pieces returns the members in add order");
  check(exchange_into("GET", "/api/corpus/test-corpus", NULL, response, sizeof(response)), "get after add: got a response");
  check(has(response, "\"piece_count\":3") && has(response, "[\"piece-1\",\"piece-2\",\"piece-3\"]"),
        "get returns the piece ids");

  /* Invalid piece id in the path is rejected before reaching the service. */
  check(exchange_into("DELETE", "/api/corpus/test-corpus/pieces/bad!piece", NULL, response, sizeof(response)),
        "invalid piece id: got a response");
  check(has(response, "\"ok\":false"), "invalid piece id in path rejected");

  /* Remove a member; removing it again fails. */
  check(exchange_into("DELETE", "/api/corpus/test-corpus/pieces/piece-1", NULL, response, sizeof(response)),
        "remove piece: got a response");
  check(has(response, "\"ok\":true"), "remove piece succeeds");
  check(exchange_into("DELETE", "/api/corpus/test-corpus/pieces/piece-1", NULL, response, sizeof(response)),
        "remove non-member: got a response");
  check(has(response, "\"ok\":false"), "remove non-member piece fails");

  /* Deleting a piece removes it from every collection that holds it. */
  char path[128];
  snprintf(path, sizeof(path), "/api/corpus/%s/pieces", second_id);
  check(exchange_into("POST", path, "{\"piece_ids\":[\"piece-2\",\"piece-3\"]}", response, sizeof(response)),
        "second collection: add got a response");
  check(has(response, "\"added\":2"), "second collection holds piece-2 and piece-3");
  check(exchange_into("DELETE", "/api/analyzer/pieces/piece-2", NULL, response, sizeof(response)),
        "delete piece: got a response");
  check(has(response, "\"ok\":true"), "delete piece succeeds");
  check(exchange_into("GET", "/api/corpus/test-corpus/pieces", NULL, response, sizeof(response)),
        "first collection after delete: got a response");
  check(!has(response, "piece-2") && has(response, "\"id\":\"piece-3\""),
        "the deleted piece left the first collection");
  check(exchange_into("GET", path, NULL, response, sizeof(response)), "second collection after delete: got a response");
  check(!has(response, "piece-2") && has(response, "\"id\":\"piece-3\""),
        "the deleted piece left the second collection");

  /* Collections survive a restart. */
  ursatz_gui_server_stop(ctx);
  ctx = ursatz_gui_server_start(PORT, ".");
  check(ctx != NULL, "server restarts");
  if (!ctx) {
    return 1;
  }
  test_sleep_ms(200);
  check(exchange_into("GET", "/api/corpus/test-corpus", NULL, response, sizeof(response)),
        "after restart: got a response");
  check(has(response, "\"name\":\"Renamed One\"") && has(response, "[\"piece-3\"]"),
        "a collection is read back after a restart");

  /* Delete a collection. Its pieces stay. */
  check(exchange_into("DELETE", "/api/corpus/test-corpus", NULL, response, sizeof(response)), "delete: got a response");
  check(has(response, "\"ok\":true"), "delete collection succeeds");
  check(exchange_into("GET", "/api/corpus/test-corpus", NULL, response, sizeof(response)),
        "get deleted: got a response");
  check(test_http_status(response) == 404, "a deleted collection is 404");
  check(exchange_into("DELETE", "/api/corpus/test-corpus", NULL, response, sizeof(response)),
        "delete again: got a response");
  check(test_http_status(response) == 404, "deleting a deleted collection is 404");
  check(exchange_into("GET", "/api/corpus", NULL, response, sizeof(response)), "list after delete: got a response");
  check(!has(response, "\"id\":\"test-corpus\""), "the deleted collection is not listed");
  check(test_file_exists("pieces/piece-3/meta.json"), "deleting a collection leaves its pieces");

  ursatz_gui_server_stop(ctx);
  check(true, "server stops cleanly");

  reset_directories();

  return test_finish("corpus_route");
}
