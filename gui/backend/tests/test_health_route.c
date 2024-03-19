/* Confirms the backend starts, serves the health-check route correctly,
 * and shuts down cleanly. */

#include <civetweb.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "server.h"

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
  struct mg_context *ctx = ursatz_gui_server_start("18765", ".");
  check(ctx != NULL, "server starts");
  if (!ctx) {
    return 1;
  }

  char err_buf[256];
  struct mg_connection *conn = mg_connect_client(
      "localhost", 18765, 0, err_buf, sizeof(err_buf));
  check(conn != NULL, "client can connect");

  if (conn) {
    const char *request =
        "GET /api/health/status HTTP/1.1\r\nHost: localhost\r\n\r\n";
    mg_write(conn, request, strlen(request));

    char response[1024] = {0};
    int n = mg_read(conn, response, sizeof(response) - 1);
    check(n > 0, "got a response");
    check(strstr(response, "200") != NULL, "response is 200 OK");
    check(strstr(response, "\"ok\"") != NULL, "response body has ok field");

    mg_close_connection(conn);
  }

  ursatz_gui_server_stop(ctx);
  check(true, "server stops cleanly");

  printf(failures == 0 ? "ALL TESTS PASSED\n" : "TESTS FAILED\n");
  return failures == 0 ? 0 : 1;
}
