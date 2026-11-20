/* GET /api/events (R-648): a raw socket reads the stream (text/event-stream, no Connection: close) while a subject is made and an import runs as a job;
 * the events come in the form "event: <name>\ndata: <json>\n\n"; a ninth stream is refused with 503; the server stops while streams are open.
 * Runs in its own working directory. */

#include <arpa/inet.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "server.h"
#include "support/server_support.h"
#include "test_http.h"
#include "ursatz_check.h"

#ifndef URSATZ_GUI_TEST_LINK_FIXTURES
#error "URSATZ_GUI_TEST_LINK_FIXTURES must name the link fixtures directory"
#endif

#define PORT "18774"

static int open_stream(void) {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof addr);
  addr.sin_family = AF_INET;
  addr.sin_port = htons((unsigned short)atoi(PORT));
  inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
  if (sock < 0 || connect(sock, (struct sockaddr *)&addr, sizeof addr) != 0) {
    return -1;
  }
  const char *request = "GET /api/events HTTP/1.1\r\nHost: 127.0.0.1\r\nAccept: text/event-stream\r\n\r\n";
  send(sock, request, strlen(request), 0);
  return sock;
}

/* Reads from sock into buffer until it holds text, up to timeout_ms; true when it does. */
static int read_until(int sock, char *buffer, size_t size, const char *text, int timeout_ms) {
  size_t length = strlen(buffer);
  for (int waited = 0; waited < timeout_ms; waited += 100) {
    if (strstr(buffer, text)) {
      return 1;
    }
    struct pollfd fd = {sock, POLLIN, 0};
    if (poll(&fd, 1, 100) > 0) {
      ssize_t n = recv(sock, buffer + length, size - length - 1, 0);
      if (n <= 0) {
        break;
      }
      length += (size_t)n;
      buffer[length] = '\0';
    }
  }
  return strstr(buffer, text) != NULL;
}

int main(void) {
  struct mg_context *ctx = test_gui_server_start(PORT);
  if (!ctx) {
    return 1;
  }
  int streams[8];
  char buffers[8][8192];
  for (int i = 0; i < 8; i++) {
    streams[i] = open_stream();
    buffers[i][0] = '\0';
  }
  check(streams[0] >= 0 && read_until(streams[0], buffers[0], sizeof(buffers[0]), ": connected", 3000), "the stream opens and says it is connected");
  check(strstr(buffers[0], "HTTP/1.1 200") && strstr(buffers[0], "Content-Type: text/event-stream") && strstr(buffers[0], "Cache-Control: no-cache") &&
            !strstr(buffers[0], "Connection: close"),
        "it is text/event-stream, not cached and not closed");
  for (int i = 1; i < 8; i++) {
    read_until(streams[i], buffers[i], sizeof(buffers[i]), ": connected", 3000);
  }
  int ninth = open_stream();
  char refused[1024] = "";
  read_until(ninth, refused, sizeof(refused), "503", 3000);
  check(strstr(refused, "HTTP/1.1 503") != NULL, "a ninth stream is refused with 503");
  close(ninth);

  int status = 0;
  cJSON *made = test_http_json(PORT, "POST", "/api/subjects", "{\"name\":\"Anna\",\"kind\":\"idea\"}", &status);
  cJSON_Delete(made);
  check(status == 200 && read_until(streams[0], buffers[0], sizeof(buffers[0]), "event: subject-changed\ndata: {\"subject_id\":", 3000),
        "making a subject is an event on the stream");
  check(read_until(streams[7], buffers[7], sizeof(buffers[7]), "event: subject-changed", 3000), "every stream gets it");

  char path[600];
  snprintf(path, sizeof(path), "%s/a.musicxml", URSATZ_GUI_TEST_LINK_FIXTURES);
  char *id = test_http_upload(PORT, path, "a.musicxml", NULL);
  check(id != NULL && read_until(streams[0], buffers[0], sizeof(buffers[0]), "event: job-done\ndata: {\"id\":\"job-", 5000), "an import that runs as a job ends with job-done");
  check(strstr(buffers[0], "event: library-changed") != NULL, "and the piece it made is a library-changed");
  free(id);

  /* The server stops while the streams are open: it must not wait for them. */
  test_gui_server_stop(ctx);
  char rest[512] = "";
  int closed = 0;
  for (int i = 0; i < 20 && !closed; i++) {
    struct pollfd fd = {streams[0], POLLIN, 0};
    if (poll(&fd, 1, 200) > 0) {
      char chunk[512];
      closed = recv(streams[0], chunk, sizeof(chunk), 0) <= 0;
    }
  }
  (void)rest;
  check(closed, "the streams end when the server stops");
  for (int i = 0; i < 8; i++) {
    close(streams[i]);
  }
  return test_finish("gui_events_route");
}
