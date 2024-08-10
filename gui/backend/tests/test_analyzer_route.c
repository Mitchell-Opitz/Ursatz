/* Uploads round-trip through the Analyzer: POST /api/analyzer/pieces stores
 * the source and the Analyzer's analysis document under pieces/<id>/, and
 * the list, detail, rename, delete and MusicXML routes read that store back.
 * The stored document is compared with what the Analyzer CLI writes for the
 * same file. Uses a plain TCP socket for the HTTP requests, same as
 * test_health_route.c. */

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
#include <dirent.h>
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

#include "server.h"

#ifndef URSATZ_GUI_TEST_URSATZ_DIR
#error "URSATZ_GUI_TEST_URSATZ_DIR must name the external/ursatz checkout"
#endif
#ifndef URSATZ_GUI_TEST_ANALYZER_CLI
#error "URSATZ_GUI_TEST_ANALYZER_CLI must name the ursatz-analyzer executable"
#endif

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
  addr.sin_port = htons(18766);
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

static char *http_get(const char *path) {
  return http_exchange("GET", path, NULL, NULL, 0);
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

static char *read_file(const char *path, size_t *out_size) {
  FILE *file = fopen(path, "rb");
  if (!file) {
    return NULL;
  }
  size_t capacity = 65536;
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
        return NULL;
      }
      buffer = grown;
    }
  }
  fclose(file);
  if (buffer) {
    buffer[length] = '\0';
    if (out_size) {
      *out_size = length;
    }
  }
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

static bool is_directory(const char *path) {
#ifdef _WIN32
  DWORD attributes = GetFileAttributesA(path);
  return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
  struct stat st;
  return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

/* The name of the lexicographically first directory in dir_path that holds
 * a score.musicxml. */
static bool first_corpus_folder(const char *dir_path, char *out, size_t out_size) {
#ifdef _WIN32
  (void)dir_path;
  (void)out;
  (void)out_size;
  return false;
#else
  DIR *dir = opendir(dir_path);
  if (!dir) {
    return false;
  }
  bool found = false;
  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    if (entry->d_name[0] == '.') {
      continue;
    }
    char score_path[512];
    snprintf(score_path, sizeof(score_path), "%s/%s/score.musicxml", dir_path, entry->d_name);
    FILE *probe = fopen(score_path, "rb");
    if (!probe) {
      continue;
    }
    fclose(probe);
    if (!found || strcmp(entry->d_name, out) < 0) {
      snprintf(out, out_size, "%s", entry->d_name);
      found = true;
    }
  }
  closedir(dir);
  return found;
#endif
}

/* POSTs the file at path with the given upload name and returns the new
 * piece's id (caller frees), or NULL. *out_response gets the response. */
static char *upload_file(const char *path, const char *encoded_filename, char **out_response) {
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
  if (out_response) {
    *out_response = response;
  } else {
    free(response);
  }
  return id;
}

/* The document `ursatz-analyzer --json` writes for a copy of source_path
 * made in cli_dir. */
static cJSON *analyzer_cli_document(const char *source_path, const char *file_name, const char *cli_dir) {
  MKDIR("cli");
  MKDIR(cli_dir);

  size_t size = 0;
  char *data = read_file(source_path, &size);
  if (!data) {
    return NULL;
  }
  char copy_path[512];
  snprintf(copy_path, sizeof(copy_path), "%s/%s", cli_dir, file_name);
  bool copied = write_file(copy_path, data, size);
  free(data);
  if (!copied) {
    return NULL;
  }

  char command[1536];
  snprintf(command, sizeof(command), "\"%s\" --json \"%s\"", URSATZ_GUI_TEST_ANALYZER_CLI, copy_path);
  if (system(command) != 0) {
    return NULL;
  }

  char stem[256];
  snprintf(stem, sizeof(stem), "%s", file_name);
  char *dot = strrchr(stem, '.');
  if (dot) {
    *dot = '\0';
  }
  char document_path[512];
  snprintf(document_path, sizeof(document_path), "%s/%s.analysis.json", cli_dir, stem);
  char *text = read_file(document_path, NULL);
  cJSON *document = text ? cJSON_Parse(text) : NULL;
  free(text);
  return document;
}

/* metadata.source_file names the input file, which differs between the CLI's
 * copy and the stored source. */
static void drop_source_file(cJSON *document) {
  cJSON *metadata = cJSON_GetObjectItemCaseSensitive(document, "metadata");
  if (metadata) {
    cJSON_DeleteItemFromObjectCaseSensitive(metadata, "source_file");
  }
}

/* True when pieces/<id>/analysis.json equals `expected`, both without
 * metadata.source_file. */
static bool stored_document_equals(const char *id, cJSON *expected) {
  char path[512];
  snprintf(path, sizeof(path), "pieces/%s/analysis.json", id);
  char *text = read_file(path, NULL);
  cJSON *stored = text ? cJSON_Parse(text) : NULL;
  free(text);
  bool equal = false;
  if (stored && expected) {
    drop_source_file(stored);
    drop_source_file(expected);
    equal = cJSON_Compare(stored, expected, true) != 0;
  }
  cJSON_Delete(stored);
  return equal;
}

static bool piece_file_exists(const char *id, const char *name) {
  char path[512];
  snprintf(path, sizeof(path), "pieces/%s/%s", id, name);
  FILE *file = fopen(path, "rb");
  if (!file) {
    return false;
  }
  fclose(file);
  return true;
}

static void seed_piece_meta(const char *id, const char *filename, const char *uploaded_at) {
  char dir[256];
  snprintf(dir, sizeof(dir), "pieces/%s", id);
  MKDIR(dir);
  char path[320];
  snprintf(path, sizeof(path), "%s/meta.json", dir);
  char text[512];
  snprintf(text, sizeof(text),
           "{\"filename\":\"%s\",\"format\":\"midi\",\"uploaded_at\":\"%s\",\"analyzer_version\":\"0.1.0\","
           "\"document_schema_version\":9}",
           filename, uploaded_at);
  write_file(path, text, strlen(text));
}

/* Checks a piece's detail: note_xml_ids has the document's note_count
 * entries, and the first one is expected_first (when non-NULL). */
static void check_detail(const char *id, const char *expected_format, const char *expected_first, const char *label) {
  char path[256];
  snprintf(path, sizeof(path), "/api/analyzer/pieces/%s", id);
  char *response = http_get(path);
  char what[256];

  cJSON *detail = cJSON_Parse(response_body(response));
  snprintf(what, sizeof(what), "%s: detail is valid JSON with ok:true", label);
  check(detail && cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(detail, "ok")), what);
  if (detail) {
    const cJSON *format = cJSON_GetObjectItemCaseSensitive(detail, "format");
    snprintf(what, sizeof(what), "%s: detail format is %s", label, expected_format);
    check(cJSON_IsString(format) && strcmp(format->valuestring, expected_format) == 0, what);

    const cJSON *uploaded_at = cJSON_GetObjectItemCaseSensitive(detail, "uploaded_at");
    snprintf(what, sizeof(what), "%s: detail uploaded_at is ISO 8601 UTC", label);
    check(cJSON_IsString(uploaded_at) && strlen(uploaded_at->valuestring) == 20 &&
              uploaded_at->valuestring[19] == 'Z',
          what);

    const cJSON *document = cJSON_GetObjectItemCaseSensitive(detail, "document");
    const cJSON *metadata = document ? cJSON_GetObjectItemCaseSensitive(document, "metadata") : NULL;
    const cJSON *note_count = metadata ? cJSON_GetObjectItemCaseSensitive(metadata, "note_count") : NULL;
    const cJSON *ids = cJSON_GetObjectItemCaseSensitive(detail, "note_xml_ids");
    snprintf(what, sizeof(what), "%s: the analysis document is inlined", label);
    check(cJSON_IsObject(document) && cJSON_IsNumber(note_count), what);
    snprintf(what, sizeof(what), "%s: note_xml_ids has note_count entries", label);
    check(cJSON_IsArray(ids) && cJSON_IsNumber(note_count) && note_count->valuedouble > 0 &&
              cJSON_GetArraySize(ids) == (int)note_count->valuedouble,
          what);
    if (expected_first) {
      const cJSON *first = cJSON_IsArray(ids) ? cJSON_GetArrayItem(ids, 0) : NULL;
      snprintf(what, sizeof(what), "%s: the first note id is %s", label, expected_first);
      check(cJSON_IsString(first) && strcmp(first->valuestring, expected_first) == 0, what);
    }
  }
  cJSON_Delete(detail);
  free(response);
}

static void check_score_meter(const char *id, bool expect_defaulted, const char *label) {
  char path[256];
  char what[256];
  snprintf(path, sizeof(path), "/api/analyzer/pieces/%s/musicxml", id);
  char *xml = http_get(path);
  snprintf(what, sizeof(what), "%s: the MusicXML export is 200 OK", label);
  check(response_has_status(xml, 200), what);
  snprintf(what, sizeof(what), "%s: the export %s ursatz:meter-defaulted", label,
           expect_defaulted ? "carries" : "has no");
  check((strstr(response_body(xml), "ursatz:meter-defaulted") != NULL) == expect_defaulted, what);
  if (!expect_defaulted) {
    snprintf(what, sizeof(what), "%s: the export keeps the file's 4/4 (<beats>4</beats>)", label);
    check(strstr(response_body(xml), "<beats>4</beats>") != NULL, what);
  }
  free(xml);

  snprintf(path, sizeof(path), "/api/analyzer/pieces/%s/musicxml/meta", id);
  char *meta = http_get(path);
  snprintf(what, sizeof(what), "%s: musicxml/meta reports meter_defaulted:%s", label,
           expect_defaulted ? "true" : "false");
  check(strstr(response_body(meta), expect_defaulted ? "\"meter_defaulted\":true" : "\"meter_defaulted\":false") !=
            NULL,
        what);
  check(strstr(response_body(meta), "cosmetic_meter") == NULL, "musicxml/meta no longer has cosmetic_meter");
  free(meta);
}

int main(void) {
#ifdef _WIN32
  WSADATA wsa_data;
  WSAStartup(MAKEWORD(2, 2), &wsa_data);
#endif

  /* Fresh state: this test's working directory is its own. */
#ifdef _WIN32
  system("rmdir /s /q pieces cli 2> nul");
#else
  system("rm -rf pieces cli");
#endif
  MKDIR("pieces");

  const char *fixtures = URSATZ_GUI_TEST_URSATZ_DIR "/tests/fixtures/framework-v1-corpus";
  char mozart_path[512];
  char bach_path[512];
  snprintf(mozart_path, sizeof(mozart_path), "%s/Mozart 545.mid", fixtures);
  snprintf(bach_path, sizeof(bach_path), "%s/Bach 114.mid", fixtures);

  char folder[256] = "";
  bool have_folder = first_corpus_folder(URSATZ_GUI_TEST_URSATZ_DIR "/tests/accuracy/corpus", folder, sizeof(folder));
  check(have_folder, "found the first accuracy corpus folder with a score.musicxml");
  char score_path[768];
  snprintf(score_path, sizeof(score_path), URSATZ_GUI_TEST_URSATZ_DIR "/tests/accuracy/corpus/%s/score.musicxml",
           folder);

  struct mg_context *ctx = ursatz_gui_server_start("18766", ".");
  check(ctx != NULL, "server starts");
  if (!ctx) {
    return 1;
  }
  sleep_ms(200);

  /* A MIDI upload. */
  char *response = NULL;
  char *mozart_id = upload_file(mozart_path, "Mozart%20545.mid", &response);
  check(mozart_id != NULL && response_has_status(response, 200), "Mozart 545.mid uploads with 200 OK");
  free(response);
  check(mozart_id && piece_file_exists(mozart_id, "source.mid"), "the MIDI upload is stored as source.mid");
  check(mozart_id && piece_file_exists(mozart_id, "analysis.json") && piece_file_exists(mozart_id, "meta.json"),
        "analysis.json and meta.json are stored next to the source");
  if (mozart_id) {
    char path[512];
    snprintf(path, sizeof(path), "pieces/%s/source.mid", mozart_id);
    size_t stored_size = 0, original_size = 0;
    char *stored = read_file(path, &stored_size);
    char *original = read_file(mozart_path, &original_size);
    check(stored && original && stored_size == original_size && memcmp(stored, original, stored_size) == 0,
          "the stored source is the upload's bytes, unchanged");
    free(stored);
    free(original);

    cJSON *expected = analyzer_cli_document(mozart_path, "Mozart 545.mid", "cli/mozart");
    check(expected != NULL, "the Analyzer CLI writes a document for Mozart 545.mid");
    check(stored_document_equals(mozart_id, expected), "the stored MIDI analysis equals the Analyzer CLI's document");
    cJSON_Delete(expected);
    check_detail(mozart_id, "midi", "midi-note-0", "Mozart 545");
    check_score_meter(mozart_id, false, "Mozart 545");
  }

  /* A MusicXML upload. */
  char *score_id = have_folder ? upload_file(score_path, "score.musicxml", &response) : NULL;
  check(score_id != NULL && response_has_status(response, 200), "a MusicXML score uploads with 200 OK");
  free(response);
  check(score_id && piece_file_exists(score_id, "source.musicxml"), "the MusicXML upload is stored as source.musicxml");
  if (score_id) {
    cJSON *expected = analyzer_cli_document(score_path, "score.musicxml", "cli/score");
    check(expected != NULL, "the Analyzer CLI writes a document for the MusicXML score");
    check(stored_document_equals(score_id, expected),
          "the stored MusicXML analysis equals the Analyzer CLI's document");
    cJSON_Delete(expected);
    check_detail(score_id, "musicxml", NULL, "MusicXML score");
  }

  /* A MIDI file declaring 4/4 for a 3/4 piece keeps the file's meter. */
  char *bach_id = upload_file(bach_path, "Bach%20114.mid", NULL);
  check(bach_id != NULL, "Bach 114.mid uploads");
  if (bach_id) {
    check_score_meter(bach_id, false, "Bach 114");
  }

  /* A MIDI file without a time signature gets a default 4/4, flagged. */
  static const unsigned char kNoMeterMidi[] = {'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, 0x01, 0xE0, 'M', 'T',
                                               'r', 'k', 0, 0, 0, 13, 0x00, 0x90, 0x3C, 0x40, 0x83, 0x60, 0x80,
                                               0x3C, 0x40, 0x00, 0xFF, 0x2F, 0x00};
  write_file("no-meter.mid", kNoMeterMidi, sizeof(kNoMeterMidi));
  char *no_meter_id = upload_file("no-meter.mid", "no-meter.mid", NULL);
  check(no_meter_id != NULL, "a MIDI file with no time signature uploads");
  if (no_meter_id) {
    check_score_meter(no_meter_id, true, "no time signature");
  }

  /* Bodies the Analyzer cannot read. */
  response = http_exchange("POST", "/api/analyzer/pieces", NULL, "just some text", 14);
  check(response_has_status(response, 415), "a .txt body gives 415");
  check(strstr(response_body(response), "\"error\": \"unsupported format\"") != NULL,
        "the 415 body is {\"error\": \"unsupported format\"}");
  free(response);
  response = http_exchange("POST", "/api/analyzer/pieces", NULL, "MThd\0\0\0\6", 8);
  check(response_has_status(response, 422), "a truncated MIDI file gives 422");
  free(response);

  /* The list: newest first, then id; anything that is not a piece directory
     with a meta.json is left out. */
  seed_piece_meta("seed-newest", "newest.mid", "2099-01-01T00:00:00Z");
  seed_piece_meta("seed-oldest", "oldest.mid", "2001-01-01T00:00:00Z");
  write_file("pieces/old-piece.db", "old", 3);
  char *list = http_get("/api/analyzer/pieces");
  check(response_has_status(list, 200), "the list is 200 OK");
  const char *list_body = response_body(list);
  const char *newest = strstr(list_body, "\"id\":\"seed-newest\"");
  const char *oldest = strstr(list_body, "\"id\":\"seed-oldest\"");
  const char *uploaded = mozart_id ? strstr(list_body, mozart_id) : NULL;
  check(newest && oldest && uploaded && newest < uploaded && uploaded < oldest,
        "the list is ordered by uploaded_at, newest first");
  check(strstr(list_body, "old-piece") == NULL, "an old <id>.db file in pieces/ is not listed");
  check(strstr(list_body, "\"filename\":\"Mozart 545.mid\",\"format\":\"midi\",\"uploaded_at\":\"") != NULL,
        "a list entry has id, filename, format and uploaded_at");
  free(list);

  /* Rename. */
  if (mozart_id) {
    char path[256];
    snprintf(path, sizeof(path), "/api/analyzer/pieces/%s", mozart_id);
    const char *body = "{\"filename\":\"Renamed.mid\"}";
    response = http_exchange("PATCH", path, "Content-Type: application/json\r\n", body, strlen(body));
    check(response_has_status(response, 200) && strstr(response_body(response), "\"filename\":\"Renamed.mid\"") != NULL,
          "PATCH renames a piece");
    free(response);
    list = http_get("/api/analyzer/pieces");
    check(strstr(response_body(list), "\"filename\":\"Renamed.mid\"") != NULL, "the list shows the new name");
    free(list);
    check(piece_file_exists(mozart_id, "source.mid") && piece_file_exists(mozart_id, "analysis.json"),
          "renaming leaves the source and the analysis alone");

    /* Delete. */
    char dir[256];
    snprintf(dir, sizeof(dir), "pieces/%s", mozart_id);
    response = http_exchange("DELETE", path, NULL, NULL, 0);
    check(response_has_status(response, 200), "DELETE removes a piece");
    free(response);
    check(!is_directory(dir), "deleting a piece removes its directory");
    response = http_get(path);
    check(strstr(response_body(response), "\"ok\":false") != NULL, "a deleted piece is not found");
    free(response);
  }

  free(mozart_id);
  free(score_id);
  free(bach_id);
  free(no_meter_id);
  ursatz_gui_server_stop(ctx);
  return failures == 0 ? 0 : 1;
}
