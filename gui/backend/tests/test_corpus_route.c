/* Confirms the Corpus tool's routes end to end: create, list, add/remove
 * piece refs, and path-param validation. Uses a plain TCP socket for the
 * HTTP requests, same as test_health_route.c/test_analyzer_route.c. */

#include <civetweb.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef SOCKET socket_t;
#define CLOSESOCK closesocket
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
typedef int socket_t;
#define CLOSESOCK close
#define INVALID_SOCKET (-1)
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

  char request[1024];
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

int main(void) {
#ifdef _WIN32
  WSADATA wsa_data;
  WSAStartup(MAKEWORD(2, 2), &wsa_data);
#endif

  remove("data/corpus.db");

  struct mg_context *ctx = ursatz_gui_server_start("18767", ".");
  check(ctx != NULL, "server starts");
  if (!ctx) {
    return 1;
  }

  sleep_ms(200);

  char response[4096];

  /* Create a Corpus. */
  check(http_request("POST", "/api/corpus", "{\"id\":\"test-corpus\",\"name\":\"Test Corpus\"}", response,
                      sizeof(response)),
        "create corpus: got a response");
  check(strstr(response, "\"ok\":true") != NULL, "create corpus succeeds");

  /* Duplicate id fails. */
  check(http_request("POST", "/api/corpus", "{\"id\":\"test-corpus\",\"name\":\"Test Corpus\"}", response,
                      sizeof(response)),
        "duplicate create: got a response");
  check(strstr(response, "\"ok\":false") != NULL, "duplicate create fails");

  /* List includes the new Corpus. */
  check(http_request("GET", "/api/corpus", NULL, response, sizeof(response)), "list corpora: got a response");
  check(strstr(response, "\"ok\":true") != NULL, "list corpora succeeds");
  check(strstr(response, "\"id\":\"test-corpus\"") != NULL, "list corpora includes created corpus");

  /* Create with a name only: the id is generated and returned. */
  check(http_request("POST", "/api/corpus", "{\"name\":\"Unnamed Id Corpus\"}", response, sizeof(response)),
        "name-only create: got a response");
  check(strstr(response, "\"ok\":true") != NULL, "name-only create succeeds");
  check(strstr(response, "\"id\":\"col-") != NULL, "name-only create returns a generated col- id");
  check(strstr(response, "\"name\":\"Unnamed Id Corpus\"") != NULL, "name-only create returns the name");
  check(http_request("GET", "/api/corpus", NULL, response, sizeof(response)), "list after name-only create: got a response");
  check(strstr(response, "\"name\":\"Unnamed Id Corpus\"") != NULL, "name-only corpus is listed under its name");

  /* Invalid corpus id in path is rejected before reaching the service. */
  check(http_request("GET", "/api/corpus/bad!id/pieces", NULL, response, sizeof(response)),
        "invalid corpus id: got a response");
  check(strstr(response, "\"ok\":false") != NULL, "invalid corpus id in path rejected");

  /* Add a piece ref. */
  check(http_request("POST", "/api/corpus/test-corpus/pieces", "{\"piece_id\":\"piece-1\"}", response,
                      sizeof(response)),
        "add piece: got a response");
  check(strstr(response, "\"ok\":true") != NULL, "add piece succeeds");

  /* Adding the same ref again fails, surfaced as-is. */
  check(http_request("POST", "/api/corpus/test-corpus/pieces", "{\"piece_id\":\"piece-1\"}", response,
                      sizeof(response)),
        "duplicate add: got a response");
  check(strstr(response, "\"ok\":false") != NULL, "duplicate add piece fails");

  /* List piece refs includes it. */
  check(http_request("GET", "/api/corpus/test-corpus/pieces", NULL, response, sizeof(response)),
        "list pieces: got a response");
  check(strstr(response, "\"ok\":true") != NULL, "list pieces succeeds");
  check(strstr(response, "\"id\":\"piece-1\"") != NULL, "list pieces includes added piece");

  /* Invalid piece id in path is rejected before reaching the service. */
  check(http_request("DELETE", "/api/corpus/test-corpus/pieces/bad!piece", NULL, response, sizeof(response)),
        "invalid piece id: got a response");
  check(strstr(response, "\"ok\":false") != NULL, "invalid piece id in path rejected");

  /* Remove the piece ref. */
  check(http_request("DELETE", "/api/corpus/test-corpus/pieces/piece-1", NULL, response, sizeof(response)),
        "remove piece: got a response");
  check(strstr(response, "\"ok\":true") != NULL, "remove piece succeeds");

  /* Removing a ref that is no longer a member fails, surfaced as-is. */
  check(http_request("DELETE", "/api/corpus/test-corpus/pieces/piece-1", NULL, response, sizeof(response)),
        "remove non-member: got a response");
  check(strstr(response, "\"ok\":false") != NULL, "remove non-member piece fails");

  ursatz_gui_server_stop(ctx);
  check(true, "server stops cleanly");

  remove("data/corpus.db");

#ifdef _WIN32
  WSACleanup();
#endif

  printf(failures == 0 ? "ALL TESTS PASSED\n" : "TESTS FAILED\n");
  return failures == 0 ? 0 : 1;
}
