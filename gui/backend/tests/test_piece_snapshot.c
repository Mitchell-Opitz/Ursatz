/* The replies for a piece's detail (note ids), score (MusicXML and its meta) and playback, compared with the
 * snapshot in fixtures/pieces taken before the Analyzer produced them (B7). JSON is compared as values, MusicXML
 * byte for byte. Set URSATZ_GUI_SNAPSHOT_CAPTURE=1 to rewrite the snapshot from the current replies. */

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
#include <unistd.h>
typedef int socket_t;
#define CLOSESOCK close
#define INVALID_SOCKET (-1)
#define MKDIR(path) mkdir(path, 0755)
#endif

#include <cJSON.h>

#include "server.h"

#ifndef URSATZ_GUI_TEST_URSATZ_DIR
#error "URSATZ_GUI_TEST_URSATZ_DIR must name the external/ursatz checkout"
#endif
#ifndef URSATZ_GUI_TEST_PIECE_FIXTURES
#error "URSATZ_GUI_TEST_PIECE_FIXTURES must name fixtures/pieces"
#endif

#define PORT 18775

static int failures = 0;
static bool capture = false;

static void check(bool cond, const char *what) {
  if (!cond) {
    fprintf(stderr, "FAIL: %s\n", what);
    failures++;
  } else {
    printf("PASS: %s\n", what);
  }
}

static char *http_exchange(const char *method, const char *path, const char *extra_headers, const void *body,
                           size_t body_len) {
  socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock == INVALID_SOCKET) {
    return NULL;
  }
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(PORT);
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

static const char *response_body(const char *response) {
  const char *split = response ? strstr(response, "\r\n\r\n") : NULL;
  return split ? split + 4 : "";
}

static bool response_has_status(const char *response, int status) {
  char needle[32];
  snprintf(needle, sizeof(needle), "HTTP/1.1 %d", status);
  return response && strncmp(response, needle, strlen(needle)) == 0;
}

static char *read_file(const char *path, size_t *out_size) {
  FILE *file = fopen(path, "rb");
  if (!file) {
    return NULL;
  }
  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  fseek(file, 0, SEEK_SET);
  char *buffer = size >= 0 ? malloc((size_t)size + 1) : NULL;
  if (buffer) {
    size_t n = fread(buffer, 1, (size_t)size, file);
    buffer[n] = '\0';
    if (out_size) {
      *out_size = n;
    }
  }
  fclose(file);
  return buffer;
}

static bool write_file(const char *path, const void *data, size_t size) {
  FILE *file = fopen(path, "wb");
  if (!file) {
    return false;
  }
  bool ok = fwrite(data, 1, size, file) == size;
  return (fclose(file) == 0) && ok;
}

static char *upload_file(const char *path, const char *encoded_filename) {
  size_t size = 0;
  char *data = read_file(path, &size);
  if (!data) {
    return NULL;
  }
  char headers[512];
  snprintf(headers, sizeof(headers), "X-Filename: %s\r\n", encoded_filename);
  char *response = http_exchange("POST", "/api/analyzer/pieces", headers, data, size);
  free(data);
  char *id = NULL;
  cJSON *json = cJSON_Parse(response_body(response));
  const cJSON *id_item = json ? cJSON_GetObjectItemCaseSensitive(json, "id") : NULL;
  if (cJSON_IsString(id_item)) {
    id = strdup(id_item->valuestring);
  }
  cJSON_Delete(json);
  free(response);
  return id;
}

/* GET route for the piece. The body is returned (caller frees) when the status is 200. */
static char *get_body(const char *id, const char *suffix) {
  char path[256];
  snprintf(path, sizeof(path), "/api/analyzer/pieces/%s%s", id, suffix);
  char *response = http_exchange("GET", path, NULL, NULL, 0);
  char *body = response_has_status(response, 200) ? strdup(response_body(response)) : NULL;
  free(response);
  return body;
}

/* Compares text with fixtures/pieces/<name>/<file>, or writes it in capture mode. JSON is compared as values. */
static void compare(const char *name, const char *file, const char *text, bool json) {
  char path[768];
  char what[256];
  snprintf(path, sizeof(path), "%s/%s", URSATZ_GUI_TEST_PIECE_FIXTURES, name);
  if (capture) {
    MKDIR(path);
  }
  snprintf(path, sizeof(path), "%s/%s/%s", URSATZ_GUI_TEST_PIECE_FIXTURES, name, file);
  snprintf(what, sizeof(what), "%s: %s equals the snapshot", name, file);
  if (!text) {
    check(false, what);
    return;
  }
  if (capture) {
    check(write_file(path, text, strlen(text)), what);
    return;
  }
  char *expected = read_file(path, NULL);
  bool same = false;
  if (expected && json) {
    cJSON *a = cJSON_Parse(expected);
    cJSON *b = cJSON_Parse(text);
    same = a && b && cJSON_Compare(a, b, true);
    cJSON_Delete(a);
    cJSON_Delete(b);
  } else if (expected) {
    same = strcmp(expected, text) == 0;
  }
  free(expected);
  check(same, what);
}

static void snapshot_piece(const char *name, const char *source_path, const char *encoded_filename) {
  char *id = upload_file(source_path, encoded_filename);
  char what[256];
  snprintf(what, sizeof(what), "%s uploads", name);
  check(id != NULL, what);
  if (!id) {
    return;
  }

  char *detail_text = get_body(id, "");
  cJSON *detail = detail_text ? cJSON_Parse(detail_text) : NULL;
  cJSON *ids = detail ? cJSON_DetachItemFromObjectCaseSensitive(detail, "note_xml_ids") : NULL;
  char *ids_text = ids ? cJSON_PrintUnformatted(ids) : NULL;
  compare(name, "note_xml_ids.json", ids_text, true);
  free(ids_text);
  cJSON_Delete(ids);
  cJSON_Delete(detail);
  free(detail_text);

  char *score = get_body(id, "/musicxml");
  compare(name, "score.musicxml", score, false);
  free(score);
  char *meta = get_body(id, "/musicxml/meta");
  compare(name, "score-meta.json", meta, true);
  free(meta);
  char *playback = get_body(id, "/playback");
  compare(name, "playback.json", playback, true);
  free(playback);
  free(id);
}

int main(void) {
#ifdef _WIN32
  WSADATA wsa_data;
  WSAStartup(MAKEWORD(2, 2), &wsa_data);
  system("rmdir /s /q pieces 2> nul");
#else
  system("rm -rf pieces");
#endif
  MKDIR("pieces");
  const char *capture_env = getenv("URSATZ_GUI_SNAPSHOT_CAPTURE");
  capture = capture_env && capture_env[0] == '1';
  if (capture) {
    MKDIR(URSATZ_GUI_TEST_PIECE_FIXTURES);
  }

  struct mg_context *ctx = ursatz_gui_server_start("18775", ".");
  check(ctx != NULL, "server starts");
  if (!ctx) {
    return 1;
  }

  const char *corpus = URSATZ_GUI_TEST_URSATZ_DIR "/tests/fixtures/framework-v1-corpus";
  char path[512];
  snprintf(path, sizeof(path), "%s/Bach 114.mid", corpus);
  snapshot_piece("bach-114", path, "Bach%20114.mid");

  /* A MIDI file without a time signature: the meter is defaulted. */
  static const unsigned char kNoMeterMidi[] = {'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, 0x01, 0xE0, 'M', 'T',
                                               'r', 'k', 0, 0, 0, 13, 0x00, 0x90, 0x3C, 0x40, 0x83, 0x60, 0x80,
                                               0x3C, 0x40, 0x00, 0xFF, 0x2F, 0x00};
  write_file("no-meter.mid", kNoMeterMidi, sizeof(kNoMeterMidi));
  snapshot_piece("no-meter", "no-meter.mid", "no-meter.mid");

  snapshot_piece("chords-backup", URSATZ_GUI_TEST_URSATZ_DIR "/tests/fixtures/musicxml/chords_backup.musicxml",
                 "chords_backup.musicxml");

  ursatz_gui_server_stop(ctx);
  return failures == 0 ? 0 : 1;
}
