/* Confirms the Collections routes end to end (they keep the /api/corpus
 * paths): create with a description, list order, read, rename, delete, bulk
 * add, removal, the cleanup when a piece is deleted, persistence across a
 * restart, and the one-time import of the old data/corpus.db. Uses a plain
 * TCP socket for the HTTP requests, same as test_health_route.c and
 * test_analyzer_route.c. Runs in its own working directory (collections/,
 * pieces/ and data/ are created there). */

#include <civetweb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <corpus/corpus_api.h>

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

#include "server.h"

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

/* Sends one HTTP request to 127.0.0.1:18767 and reads the full response
 * into response (caller-sized, NUL-terminated on success). body may be
 * NULL for a bodyless request. */
static bool http_request(const char *method, const char *path, const char *body, char *response,
                          size_t response_capacity) {
  socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock == INVALID_SOCKET) {
    return false;
  }

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(18767);
  inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

  if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
    CLOSESOCK(sock);
    return false;
  }

  char request[4096];
  size_t body_len = body ? strlen(body) : 0;
  snprintf(request, sizeof(request),
           "%s %s HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\nContent-Length: %zu\r\n\r\n%s", method, path,
           body_len, body ? body : "");
  send(sock, request, (int)strlen(request), 0);

  memset(response, 0, response_capacity);
  size_t total = 0;
  for (;;) {
    int n = recv(sock, response + total, (int)(response_capacity - 1 - total), 0);
    if (n <= 0) {
      break;
    }
    total += (size_t)n;
    if (total >= response_capacity - 1) {
      break;
    }
  }

  CLOSESOCK(sock);
  return total > 0;
}

static bool has(const char *response, const char *text) {
  return strstr(response, text) != NULL;
}

static bool has_status(const char *response, int status) {
  char line[32];
  snprintf(line, sizeof(line), "HTTP/1.1 %d", status);
  return strncmp(response, line, strlen(line)) == 0;
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
  system("rmdir /s /q collections pieces data 2> nul");
#else
  system("rm -rf collections pieces data");
#endif
}

/* A piece the routes can see: pieces/<id>/meta.json. */
static void make_piece(const char *id) {
  char path[128];
  snprintf(path, sizeof(path), "pieces/%s", id);
  MKDIR(path);
  snprintf(path, sizeof(path), "pieces/%s/meta.json", id);
  FILE *file = fopen(path, "wb");
  if (file) {
    fputs("{\"filename\":\"piece.mid\",\"format\":\"midi\",\"uploaded_at\":\"2026-01-01T00:00:00Z\"}", file);
    fclose(file);
  }
}

static bool file_exists(const char *path) {
  FILE *file = fopen(path, "rb");
  if (file) {
    fclose(file);
    return true;
  }
  return false;
}

static void test_import(void) {
  printf("-- import from the old corpus store\n");
  reset_directories();
  MKDIR("data");
  MKDIR("pieces");
  make_piece("piece-a");

  CorpusStoreHandle *handle = NULL;
  check(corpus_api_open("data/corpus.db", &handle) == CORPUS_API_OK, "old corpus.db opens");
  if (!handle) {
    return;
  }
  check(corpus_api_create_corpus(handle, "old-one", "Old One") == CORPUS_API_OK, "old corpus created");
  check(corpus_api_create_corpus(handle, "old-two", "Old Two") == CORPUS_API_OK, "second old corpus created");
  check(corpus_api_add_piece_ref(handle, "old-one", "piece-a") == CORPUS_API_OK, "existing piece ref added");
  check(corpus_api_add_piece_ref(handle, "old-one", "piece-gone") == CORPUS_API_OK, "missing piece ref added");
  corpus_api_close(handle);

  struct mg_context *ctx = ursatz_gui_server_start("18767", ".");
  check(ctx != NULL, "server starts and imports");
  if (!ctx) {
    return;
  }
  sleep_ms(200);

  char response[8192];
  check(http_request("GET", "/api/corpus", NULL, response, sizeof(response)), "import: list got a response");
  check(has(response, "\"id\":\"old-one\"") && has(response, "\"name\":\"Old One\""), "import: first corpus listed");
  check(has(response, "\"id\":\"old-two\""), "import: second corpus listed");
  check(http_request("GET", "/api/corpus/old-one/pieces", NULL, response, sizeof(response)),
        "import: pieces got a response");
  check(has(response, "\"id\":\"piece-a\""), "import: existing piece kept");
  check(!has(response, "piece-gone"), "import: missing piece dropped");
  check(file_exists("collections/.imported-from-corpus-db"), "import: marker file written");
  check(file_exists("data/corpus.db"), "import: old corpus.db left in place");

  ursatz_gui_server_stop(ctx);
  reset_directories();
}

int main(void) {
#ifdef _WIN32
  WSADATA wsa_data;
  WSAStartup(MAKEWORD(2, 2), &wsa_data);
#endif

  test_import();

  printf("-- collection routes\n");
  reset_directories();
  MKDIR("pieces");
  make_piece("piece-1");
  make_piece("piece-2");
  make_piece("piece-3");

  struct mg_context *ctx = ursatz_gui_server_start("18767", ".");
  check(ctx != NULL, "server starts");
  if (!ctx) {
    return 1;
  }

  sleep_ms(200);

  char response[8192];

  /* Create with an id, a name and a description (R-098: the id is still accepted). */
  check(http_request("POST", "/api/corpus",
                     "{\"id\":\"test-corpus\",\"name\":\"Test  Corpus \",\"description\":\" First   collection \"}",
                     response, sizeof(response)),
        "create: got a response");
  check(has(response, "\"ok\":true"), "create succeeds");
  check(has(response, "\"name\":\"Test Corpus\""), "create normalizes the name");
  check(has(response, "\"description\":\"First collection\""), "create stores the description");
  check(has(response, "\"piece_count\":0"), "create starts with no pieces");

  /* Duplicate id fails. */
  check(http_request("POST", "/api/corpus", "{\"id\":\"test-corpus\",\"name\":\"Again\"}", response, sizeof(response)),
        "duplicate create: got a response");
  check(has(response, "\"ok\":false"), "duplicate create fails");

  /* An empty name and a bad id fail. */
  check(http_request("POST", "/api/corpus", "{\"name\":\"   \"}", response, sizeof(response)),
        "empty name: got a response");
  check(has(response, "\"ok\":false") && has(response, "invalid name"), "empty name fails");
  check(http_request("POST", "/api/corpus", "{\"id\":\"bad!id\",\"name\":\"x\"}", response, sizeof(response)),
        "bad id: got a response");
  check(has(response, "\"ok\":false") && has(response, "invalid id"), "bad id fails");

  /* Name only: the id is generated and returned. */
  check(http_request("POST", "/api/corpus", "{\"name\":\"beta collection\"}", response, sizeof(response)),
        "name-only create: got a response");
  check(has(response, "\"ok\":true"), "name-only create succeeds");
  check(has(response, "\"id\":\"col-"), "name-only create returns a generated col- id");
  char second_id[64] = "";
  check(extract_id(response, second_id, sizeof(second_id)), "name-only create: id read");

  check(http_request("POST", "/api/corpus", "{\"name\":\"Alpha collection\",\"description\":\"d\"}", response,
                     sizeof(response)),
        "third create: got a response");
  check(has(response, "\"ok\":true"), "third create succeeds");

  /* A quote in a name round-trips as valid JSON. */
  check(http_request("POST", "/api/corpus", "{\"name\":\"The \\\"Quoted\\\" one\"}", response, sizeof(response)),
        "quoted name: got a response");
  check(has(response, "\"name\":\"The \\\"Quoted\\\" one\""), "quote in a name is escaped in the answer");

  /* List: by name, case-insensitive. */
  check(http_request("GET", "/api/corpus", NULL, response, sizeof(response)), "list: got a response");
  check(has(response, "\"ok\":true") && has(response, "\"id\":\"test-corpus\""), "list includes the created collection");
  check(has(response, "\"updated_at\":\"") && has(response, "\"piece_count\":0"), "list carries counts and dates");
  check(before(response, "Alpha collection", "beta collection") && before(response, "beta collection", "Test Corpus"),
        "list is sorted by name, case-insensitively");
  check(has(response, "The \\\"Quoted\\\" one"), "list escapes a quote in a name");

  /* Invalid collection id in the path is rejected before reaching the service. */
  check(http_request("GET", "/api/corpus/bad!id/pieces", NULL, response, sizeof(response)),
        "invalid corpus id: got a response");
  check(has(response, "\"ok\":false"), "invalid corpus id in path rejected");

  /* Read one. */
  check(http_request("GET", "/api/corpus/test-corpus", NULL, response, sizeof(response)), "get: got a response");
  check(has(response, "\"ok\":true") && has(response, "\"pieces\":[]") && has(response, "\"created_at\":\""),
        "get returns the collection with its piece ids");
  check(http_request("GET", "/api/corpus/no-such", NULL, response, sizeof(response)), "get unknown: got a response");
  check(has_status(response, 404) && has(response, "\"ok\":false"), "get unknown collection is 404");

  /* Rename and describe. */
  check(http_request("PATCH", "/api/corpus/test-corpus", "{\"name\":\"Renamed   One\"}", response, sizeof(response)),
        "rename: got a response");
  check(has(response, "\"ok\":true") && has(response, "\"name\":\"Renamed One\""), "rename normalizes and answers");
  check(has(response, "\"description\":\"First collection\""), "rename leaves the description");
  check(http_request("PATCH", "/api/corpus/test-corpus", "{\"description\":\"\"}", response, sizeof(response)),
        "clear description: got a response");
  check(has(response, "\"ok\":true") && has(response, "\"description\":\"\""), "an empty description clears it");
  check(http_request("PATCH", "/api/corpus/test-corpus", "{\"name\":\"  \"}", response, sizeof(response)),
        "empty rename: got a response");
  check(has(response, "\"ok\":false") && has(response, "invalid name"), "an empty name is rejected");
  check(http_request("PATCH", "/api/corpus/test-corpus", "{}", response, sizeof(response)),
        "empty patch: got a response");
  check(has(response, "\"ok\":false") && has(response, "nothing to update"), "nothing to update");
  check(http_request("PATCH", "/api/corpus/no-such", "{\"name\":\"x\"}", response, sizeof(response)),
        "rename unknown: got a response");
  check(has_status(response, 404), "rename unknown collection is 404");

  /* Add one piece, then several. */
  check(http_request("POST", "/api/corpus/test-corpus/pieces", "{\"piece_id\":\"piece-1\"}", response,
                     sizeof(response)),
        "add piece: got a response");
  check(has(response, "\"ok\":true") && has(response, "\"added\":1") && has(response, "\"already\":0"),
        "add piece succeeds");
  check(http_request("POST", "/api/corpus/test-corpus/pieces",
                     "{\"piece_ids\":[\"piece-1\",\"piece-2\",\"piece-3\",\"piece-2\"]}", response, sizeof(response)),
        "bulk add: got a response");
  check(has(response, "\"ok\":true") && has(response, "\"added\":2") && has(response, "\"already\":2"),
        "bulk add counts added and already-members (a repeat in the request too)");

  /* An unknown piece is a 400 naming it. */
  check(http_request("POST", "/api/corpus/test-corpus/pieces", "{\"piece_ids\":[\"piece-3\",\"no-such-piece\"]}",
                     response, sizeof(response)),
        "unknown piece: got a response");
  check(has_status(response, 400) && has(response, "unknown piece: no-such-piece"), "unknown piece is 400 and named");
  check(http_request("POST", "/api/corpus/no-such/pieces", "{\"piece_id\":\"piece-1\"}", response, sizeof(response)),
        "add to unknown collection: got a response");
  check(has_status(response, 404), "adding to an unknown collection is 404");
  check(http_request("POST", "/api/corpus/test-corpus/pieces", "{\"piece_id\":\"bad!piece\"}", response,
                     sizeof(response)),
        "bad piece id: got a response");
  check(has(response, "\"ok\":false") && has(response, "invalid piece_id"), "bad piece id rejected");

  /* List the members: ids in add order. */
  check(http_request("GET", "/api/corpus/test-corpus/pieces", NULL, response, sizeof(response)),
        "list pieces: got a response");
  check(has(response, "\"ok\":true") && before(response, "\"id\":\"piece-1\"", "\"id\":\"piece-2\"") &&
            before(response, "\"id\":\"piece-2\"", "\"id\":\"piece-3\""),
        "list pieces returns the members in add order");
  check(http_request("GET", "/api/corpus/test-corpus", NULL, response, sizeof(response)), "get after add: got a response");
  check(has(response, "\"piece_count\":3") && has(response, "[\"piece-1\",\"piece-2\",\"piece-3\"]"),
        "get returns the piece ids");

  /* Invalid piece id in the path is rejected before reaching the service. */
  check(http_request("DELETE", "/api/corpus/test-corpus/pieces/bad!piece", NULL, response, sizeof(response)),
        "invalid piece id: got a response");
  check(has(response, "\"ok\":false"), "invalid piece id in path rejected");

  /* Remove a member; removing it again fails. */
  check(http_request("DELETE", "/api/corpus/test-corpus/pieces/piece-1", NULL, response, sizeof(response)),
        "remove piece: got a response");
  check(has(response, "\"ok\":true"), "remove piece succeeds");
  check(http_request("DELETE", "/api/corpus/test-corpus/pieces/piece-1", NULL, response, sizeof(response)),
        "remove non-member: got a response");
  check(has(response, "\"ok\":false"), "remove non-member piece fails");

  /* Deleting a piece removes it from every collection that holds it. */
  char path[128];
  snprintf(path, sizeof(path), "/api/corpus/%s/pieces", second_id);
  check(http_request("POST", path, "{\"piece_ids\":[\"piece-2\",\"piece-3\"]}", response, sizeof(response)),
        "second collection: add got a response");
  check(has(response, "\"added\":2"), "second collection holds piece-2 and piece-3");
  check(http_request("DELETE", "/api/analyzer/pieces/piece-2", NULL, response, sizeof(response)),
        "delete piece: got a response");
  check(has(response, "\"ok\":true"), "delete piece succeeds");
  check(http_request("GET", "/api/corpus/test-corpus/pieces", NULL, response, sizeof(response)),
        "first collection after delete: got a response");
  check(!has(response, "piece-2") && has(response, "\"id\":\"piece-3\""),
        "the deleted piece left the first collection");
  check(http_request("GET", path, NULL, response, sizeof(response)), "second collection after delete: got a response");
  check(!has(response, "piece-2") && has(response, "\"id\":\"piece-3\""),
        "the deleted piece left the second collection");

  /* Collections survive a restart. */
  ursatz_gui_server_stop(ctx);
  ctx = ursatz_gui_server_start("18767", ".");
  check(ctx != NULL, "server restarts");
  if (!ctx) {
    return 1;
  }
  sleep_ms(200);
  check(http_request("GET", "/api/corpus/test-corpus", NULL, response, sizeof(response)),
        "after restart: got a response");
  check(has(response, "\"name\":\"Renamed One\"") && has(response, "[\"piece-3\"]"),
        "a collection is read back after a restart");

  /* Delete a collection. Its pieces stay. */
  check(http_request("DELETE", "/api/corpus/test-corpus", NULL, response, sizeof(response)), "delete: got a response");
  check(has(response, "\"ok\":true"), "delete collection succeeds");
  check(http_request("GET", "/api/corpus/test-corpus", NULL, response, sizeof(response)),
        "get deleted: got a response");
  check(has_status(response, 404), "a deleted collection is 404");
  check(http_request("DELETE", "/api/corpus/test-corpus", NULL, response, sizeof(response)),
        "delete again: got a response");
  check(has_status(response, 404), "deleting a deleted collection is 404");
  check(http_request("GET", "/api/corpus", NULL, response, sizeof(response)), "list after delete: got a response");
  check(!has(response, "\"id\":\"test-corpus\""), "the deleted collection is not listed");
  check(file_exists("pieces/piece-3/meta.json"), "deleting a collection leaves its pieces");

  ursatz_gui_server_stop(ctx);
  check(true, "server stops cleanly");

  reset_directories();

#ifdef _WIN32
  WSACleanup();
#endif

  printf(failures == 0 ? "ALL TESTS PASSED\n" : "TESTS FAILED\n");
  return failures == 0 ? 0 : 1;
}
