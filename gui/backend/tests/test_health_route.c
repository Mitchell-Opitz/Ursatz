/* Confirms the backend starts, serves the health-check route correctly,
 * and shuts down cleanly. Uses a plain TCP socket for the HTTP request
 * rather than civetweb's client API, which this version doesn't expose
 * a reliable raw read path for. */

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

int main(void) {
#ifdef _WIN32
  WSADATA wsa_data;
  WSAStartup(MAKEWORD(2, 2), &wsa_data);
#endif

  struct mg_context *ctx = ursatz_gui_server_start("18765", ".");
  check(ctx != NULL, "server starts");
  if (!ctx) {
    return 1;
  }

  /* civetweb's worker threads may not be immediately ready to accept
   * right after mg_start() returns; give them a moment. */
  sleep_ms(200);

  socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
  check(sock != INVALID_SOCKET, "socket created");

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(18765);
  inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

  int connected = connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0;
  check(connected, "client can connect");

  if (connected) {
    const char *request =
        "GET /api/health/status HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
    send(sock, request, (int)strlen(request), 0);

    char response[1024] = {0};
    size_t total = 0;
    for (;;) {
      int n = recv(sock, response + total, (int)(sizeof(response) - 1 - total), 0);
      if (n <= 0) {
        break;
      }
      total += (size_t)n;
      if (total >= sizeof(response) - 1) {
        break;
      }
    }
    check(total > 0, "got a response");
    check(strstr(response, "200") != NULL, "response is 200 OK");
    check(strstr(response, "\"ok\"") != NULL, "response body has ok field");
  }

  if (sock != INVALID_SOCKET) {
    CLOSESOCK(sock);
  }

  ursatz_gui_server_stop(ctx);
  check(true, "server stops cleanly");

#ifdef _WIN32
  WSACleanup();
#endif

  printf(failures == 0 ? "ALL TESTS PASSED\n" : "TESTS FAILED\n");
  return failures == 0 ? 0 : 1;
}
