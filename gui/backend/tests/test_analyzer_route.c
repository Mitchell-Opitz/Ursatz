/* Uploads round-trip through the Analyzer: POST /api/analyzer/pieces stores
 * the source and the Analyzer's analysis document under pieces/<id>/, and
 * the list, detail, rename, delete and MusicXML routes read that store back.
 * The stored document is compared with what the Analyzer CLI writes for the
 * same file. Uses a plain TCP socket for the HTTP requests, same as
 * test_health_route.c. */

#include <civetweb.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define strcasecmp _stricmp
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef SOCKET socket_t;
#define CLOSESOCK closesocket
#define MKDIR(path) _mkdir(path)
#else
#include <arpa/inet.h>
#include <dirent.h>
#include <strings.h>
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
#include "service/subject_service.h"

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

/* The score route draws a piece: 200, application/xml, a body that opens with
 * an XML declaration. */
static void check_score_drawn(const char *id, const char *label) {
  char path[256];
  char what[256];
  snprintf(path, sizeof(path), "/api/analyzer/pieces/%s/musicxml", id);
  char *xml = http_get(path);
  snprintf(what, sizeof(what), "%s: the score is 200 OK", label);
  check(response_has_status(xml, 200), what);
  snprintf(what, sizeof(what), "%s: the score is application/xml", label);
  check(xml && strstr(xml, "Content-Type: application/xml\r\n") != NULL, what);
  snprintf(what, sizeof(what), "%s: the score body starts with <?xml", label);
  check(strncmp(response_body(xml), "<?xml", 5) == 0, what);
  free(xml);
}

static double rational_value(const cJSON *pair) {
  const cJSON *num = cJSON_GetArrayItem(pair, 0);
  const cJSON *den = cJSON_GetArrayItem(pair, 1);
  return (cJSON_IsNumber(num) && cJSON_IsNumber(den) && den->valuedouble > 0) ? num->valuedouble / den->valuedouble
                                                                              : -1.0;
}

/* The playback route lists every note of the piece with its time, pitch and
 * part, and the tempo. */
static void check_playback(const char *id, const char *label) {
  char path[256];
  char what[256];
  snprintf(path, sizeof(path), "/api/analyzer/pieces/%s", id);
  char *detail_response = http_get(path);
  cJSON *detail = cJSON_Parse(response_body(detail_response));
  const cJSON *document = detail ? cJSON_GetObjectItemCaseSensitive(detail, "document") : NULL;
  const cJSON *metadata = document ? cJSON_GetObjectItemCaseSensitive(document, "metadata") : NULL;
  const cJSON *note_count = metadata ? cJSON_GetObjectItemCaseSensitive(metadata, "note_count") : NULL;

  snprintf(path, sizeof(path), "/api/analyzer/pieces/%s/playback", id);
  char *response = http_get(path);
  snprintf(what, sizeof(what), "%s: playback is 200 OK", label);
  check(response_has_status(response, 200), what);
  snprintf(what, sizeof(what), "%s: playback is application/json", label);
  check(response && strstr(response, "Content-Type: application/json\r\n") != NULL, what);

  cJSON *playback = cJSON_Parse(response_body(response));
  snprintf(what, sizeof(what), "%s: playback is valid JSON with ok:true", label);
  check(playback && cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(playback, "ok")), what);
  if (playback) {
    const cJSON *notes = cJSON_GetObjectItemCaseSensitive(playback, "notes");
    const cJSON *tempo = cJSON_GetObjectItemCaseSensitive(playback, "tempo");
    const cJSON *parts = cJSON_GetObjectItemCaseSensitive(playback, "parts");
    const cJSON *end = cJSON_GetObjectItemCaseSensitive(playback, "end");
    const cJSON *tempo_source = cJSON_GetObjectItemCaseSensitive(playback, "tempo_source");

    snprintf(what, sizeof(what), "%s: playback notes has the document's note_count entries", label);
    check(cJSON_IsArray(notes) && cJSON_IsNumber(note_count) && cJSON_GetArraySize(notes) == (int)note_count->valuedouble,
          what);

    snprintf(what, sizeof(what), "%s: playback tempo_source is file, score or default", label);
    check(cJSON_IsString(tempo_source) && (strcmp(tempo_source->valuestring, "file") == 0 ||
                                           strcmp(tempo_source->valuestring, "score") == 0 ||
                                           strcmp(tempo_source->valuestring, "default") == 0),
          what);

    const cJSON *first_tempo = cJSON_GetArrayItem(tempo, 0);
    snprintf(what, sizeof(what), "%s: playback tempo starts at [0,1] with qpm above 0", label);
    check(cJSON_IsArray(tempo) && cJSON_GetArraySize(tempo) > 0 && first_tempo &&
              rational_value(cJSON_GetObjectItemCaseSensitive(first_tempo, "at")) == 0.0 &&
              cJSON_GetObjectItemCaseSensitive(first_tempo, "qpm") &&
              cJSON_GetObjectItemCaseSensitive(first_tempo, "qpm")->valuedouble > 0.0,
          what);

    bool tempo_ascending = cJSON_IsArray(tempo);
    double previous_at = -1.0;
    const cJSON *region = NULL;
    cJSON_ArrayForEach(region, tempo) {
      double at = rational_value(cJSON_GetObjectItemCaseSensitive(region, "at"));
      tempo_ascending = tempo_ascending && at > previous_at;
      previous_at = at;
    }
    snprintf(what, sizeof(what), "%s: playback tempo regions are strictly ascending", label);
    check(tempo_ascending, what);

    bool indexes_match = cJSON_IsArray(notes);
    bool parts_listed = cJSON_IsArray(notes) && cJSON_IsArray(parts);
    bool end_covers = cJSON_IsArray(notes) && cJSON_IsArray(end);
    double end_value = cJSON_IsArray(end) ? rational_value(end) : -1.0;
    int index = 0;
    const cJSON *note = NULL;
    cJSON_ArrayForEach(note, notes) {
      const cJSON *i = cJSON_GetObjectItemCaseSensitive(note, "i");
      const cJSON *part = cJSON_GetObjectItemCaseSensitive(note, "part");
      const cJSON *pitch = cJSON_GetObjectItemCaseSensitive(note, "pitch");
      double on = rational_value(cJSON_GetObjectItemCaseSensitive(note, "on"));
      double len = rational_value(cJSON_GetObjectItemCaseSensitive(note, "len"));
      indexes_match = indexes_match && cJSON_IsNumber(i) && (int)i->valuedouble == index;
      indexes_match = indexes_match && cJSON_IsNumber(pitch) && pitch->valuedouble >= 0 && pitch->valuedouble <= 127;
      end_covers = end_covers && on >= 0.0 && len > 0.0 && end_value + 1e-9 >= on + len;
      bool listed = false;
      const cJSON *entry = NULL;
      cJSON_ArrayForEach(entry, parts) {
        const cJSON *entry_index = cJSON_GetObjectItemCaseSensitive(entry, "index");
        listed = listed || (cJSON_IsNumber(entry_index) && cJSON_IsNumber(part) &&
                            entry_index->valuedouble == part->valuedouble);
      }
      parts_listed = parts_listed && listed;
      index++;
    }
    snprintf(what, sizeof(what), "%s: playback notes[k].i == k and pitches are MIDI numbers", label);
    check(indexes_match, what);
    snprintf(what, sizeof(what), "%s: every playback note's part is listed in parts", label);
    check(parts_listed, what);
    snprintf(what, sizeof(what), "%s: playback end is at least every on + len", label);
    check(end_covers, what);
  }
  cJSON_Delete(playback);
  cJSON_Delete(detail);
  free(response);
  free(detail_response);
}

/* PATCHes a piece with a JSON body; returns the whole response. */
static char *patch_piece(const char *id, const char *body) {
  char path[256];
  snprintf(path, sizeof(path), "/api/analyzer/pieces/%s", id);
  return http_exchange("PATCH", path, "Content-Type: application/json\r\n", body, strlen(body));
}

/* The parsed body of GET path (caller deletes). */
static cJSON *get_json(const char *path) {
  char *response = http_get(path);
  cJSON *json = response ? cJSON_Parse(response_body(response)) : NULL;
  free(response);
  return json;
}

/* The number of non-overlapping occurrences of needle in text. */
static int count_occurrences(const char *text, const char *needle) {
  int count = 0;
  size_t length = strlen(needle);
  for (const char *at = text ? strstr(text, needle) : NULL; at; at = strstr(at + length, needle)) {
    count++;
  }
  return count;
}

/* The score is drawn with notated values: parts, staves, instruments, beams,
 * the piece title, and the composer once it is set. */
static void check_notated_score(const char *piano_id, const char *two_channel_id) {
  char path[256];
  snprintf(path, sizeof(path), "/api/analyzer/pieces/%s/musicxml", two_channel_id);
  char *xml = http_get(path);
  check(response_has_status(xml, 200), "the two-channel score is 200 OK");
  check(count_occurrences(response_body(xml), "<score-part ") == 2, "a two-channel MIDI upload gives two parts");
  check(strstr(response_body(xml), "<midi-program>41</midi-program>") != NULL,
        "the second part carries its MIDI program");
  check(strstr(response_body(xml), "<work-title>two-channel</work-title>") != NULL,
        "the score title is the filename without its extension");
  check(strstr(response_body(xml), "type=\"composer\"") == NULL, "a piece with no composer writes none");
  free(xml);

  snprintf(path, sizeof(path), "/api/analyzer/pieces/%s/musicxml", piano_id);
  xml = http_get(path);
  check(response_has_status(xml, 200), "the piano score is 200 OK");
  check(strstr(response_body(xml), "<staves>2</staves>") != NULL, "a piano upload has two staves");
  check(strstr(response_body(xml), "<type>") != NULL, "the score notes carry a <type>");
  check(strstr(response_body(xml), "<beam number=\"1\">begin</beam>") != NULL, "the score notes carry <beam>");
  check(strstr(response_body(xml), "<work-title>Bach 114</work-title>") != NULL,
        "the piano score title is the piece title");
  free(xml);

  char *response = patch_piece(piano_id, "{\"composer\":\"J. S. Bach\"}");
  check(response_has_status(response, 200), "PATCH sets the composer");
  free(response);
  snprintf(path, sizeof(path), "/api/analyzer/pieces/%s/musicxml", piano_id);
  xml = http_get(path);
  check(strstr(response_body(xml), "type=\"composer\">J. S. Bach</creator>") != NULL,
        "the composer is written after a PATCH");
  free(xml);
}

static const cJSON *piece_in_list(const cJSON *list, const char *id);

/* How long playback takes, in seconds: each tempo region's span up to the end,
 * at its quarter notes per minute. -1 when the route does not answer. */
static double playback_seconds(const char *id) {
  char path[256];
  snprintf(path, sizeof(path), "/api/analyzer/pieces/%s/playback", id);
  cJSON *playback = get_json(path);
  const cJSON *tempo = playback ? cJSON_GetObjectItemCaseSensitive(playback, "tempo") : NULL;
  const cJSON *end = playback ? cJSON_GetObjectItemCaseSensitive(playback, "end") : NULL;
  double seconds = -1.0;
  if (cJSON_IsArray(tempo) && cJSON_IsArray(end)) {
    seconds = 0.0;
    double end_value = rational_value(end);
    int count = cJSON_GetArraySize(tempo);
    for (int i = 0; i < count; i++) {
      const cJSON *region = cJSON_GetArrayItem(tempo, i);
      double from = rational_value(cJSON_GetObjectItemCaseSensitive(region, "at"));
      double to = i + 1 < count ? rational_value(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(tempo, i + 1), "at"))
                                : end_value;
      double qpm = cJSON_GetObjectItemCaseSensitive(region, "qpm")->valuedouble;
      if (to > end_value) {
        to = end_value;
      }
      if (to > from && qpm > 0.0) {
        seconds += (to - from) * 4.0 / qpm * 60.0;
      }
    }
  }
  cJSON_Delete(playback);
  return seconds;
}

/* The document's meter.source for a piece ("" when absent). */
static void document_meter_source(const char *id, char *out, size_t out_size) {
  char path[256];
  snprintf(path, sizeof(path), "/api/analyzer/pieces/%s", id);
  cJSON *detail = get_json(path);
  const cJSON *document = detail ? cJSON_GetObjectItemCaseSensitive(detail, "document") : NULL;
  const cJSON *meter = document ? cJSON_GetObjectItemCaseSensitive(document, "meter") : NULL;
  const cJSON *source = meter ? cJSON_GetObjectItemCaseSensitive(meter, "source") : NULL;
  snprintf(out, out_size, "%s", cJSON_IsString(source) ? source->valuestring : "");
  cJSON_Delete(detail);
}

/* The number of <measure> elements in a piece's score. */
static int score_measure_count(const char *id, const char *beats_text) {
  char path[256];
  snprintf(path, sizeof(path), "/api/analyzer/pieces/%s/musicxml", id);
  char *xml = http_get(path);
  int count = count_occurrences(response_body(xml), "<measure ");
  if (beats_text) {
    check(strstr(response_body(xml), beats_text) != NULL, beats_text);
  }
  free(xml);
  return count;
}

/* The notation override: a 3/4 reading with doubled values redraws, analyzes
 * and plays the piece, clearing it restores the file's reading, and values the
 * Analyzer does not read are 400. */
static void check_notation_override(const char *id) {
  char path[256];
  snprintf(path, sizeof(path), "/api/analyzer/pieces/%s", id);

  int measures_before = score_measure_count(id, "<beats>4</beats>");
  double seconds_before = playback_seconds(id);
  char source[32];
  document_meter_source(id, source, sizeof(source));
  check(strcmp(source, "override") != 0, "a piece with no override has a meter source other than override");
  cJSON *detail = get_json(path);
  check(detail && cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(detail, "notation")),
        "the piece detail shows notation null without an override");
  cJSON_Delete(detail);

  char *response = patch_piece(id, "{\"notation\":{\"meter\":\"3/4\",\"value_scale\":\"2\",\"pickup\":\"\"}}");
  check(response_has_status(response, 200), "PATCH sets a notation override");
  cJSON *answer = cJSON_Parse(response_body(response));
  const cJSON *piece = answer ? cJSON_GetObjectItemCaseSensitive(answer, "piece") : NULL;
  const cJSON *notation = piece ? cJSON_GetObjectItemCaseSensitive(piece, "notation") : NULL;
  const cJSON *meter = notation ? cJSON_GetObjectItemCaseSensitive(notation, "meter") : NULL;
  const cJSON *scale = notation ? cJSON_GetObjectItemCaseSensitive(notation, "value_scale") : NULL;
  check(cJSON_IsString(meter) && strcmp(meter->valuestring, "3/4") == 0 && cJSON_IsString(scale) &&
            strcmp(scale->valuestring, "2") == 0,
        "the PATCH answer includes the piece's notation");
  cJSON_Delete(answer);
  free(response);

  int measures_after = score_measure_count(id, "<beats>3</beats>");
  check(measures_before > 0 && measures_after > 2 * measures_before,
        "a 3/4 reading with doubled values more than doubles the measure count");
  document_meter_source(id, source, sizeof(source));
  check(strcmp(source, "override") == 0, "the analysis reports the meter source override");
  double seconds_after = playback_seconds(id);
  check(seconds_before > 0.0 && seconds_after > 0.0 && fabs(seconds_after - seconds_before) < seconds_before * 0.001,
        "playback takes the same total seconds under the override");
  cJSON *list = get_json("/api/analyzer/pieces");
  const cJSON *listed = list ? piece_in_list(list, id) : NULL;
  const cJSON *listed_notation = listed ? cJSON_GetObjectItemCaseSensitive(listed, "notation") : NULL;
  check(cJSON_IsObject(listed_notation), "the piece list includes the notation");
  cJSON_Delete(list);
  detail = get_json(path);
  check(detail && cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(detail, "notation")),
        "the piece detail includes the notation");
  cJSON_Delete(detail);

  response = patch_piece(id, "{\"notation\":null}");
  check(response_has_status(response, 200), "PATCH clears the notation override");
  free(response);
  check(score_measure_count(id, "<beats>4</beats>") == measures_before, "clearing the override restores the measure count");
  document_meter_source(id, source, sizeof(source));
  check(strcmp(source, "override") != 0, "clearing the override restores the meter source");
  check(fabs(playback_seconds(id) - seconds_before) < seconds_before * 0.001, "clearing the override restores playback");

  static const char *const kInvalid[] = {
      "{\"notation\":{\"meter\":\"3/0\"}}",       "{\"notation\":{\"value_scale\":\"3\"}}",
      "{\"notation\":{\"pickup\":\"x\"}}",         "{\"notation\":{\"meter\":4}}",
      "{\"notation\":\"3/4\"}",                     "{\"notation\":{\"pickup\":\"1/1\"}}",
  };
  for (size_t i = 0; i < sizeof(kInvalid) / sizeof(kInvalid[0]); i++) {
    response = patch_piece(id, kInvalid[i]);
    char what[96];
    snprintf(what, sizeof(what), "%s gives 400 invalid notation", kInvalid[i]);
    check(response_has_status(response, 400) && strstr(response_body(response), "invalid notation") != NULL, what);
    free(response);
  }
  detail = get_json(path);
  check(detail && cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(detail, "notation")),
        "a refused notation leaves the piece without an override");
  cJSON_Delete(detail);
}

static const cJSON *piece_in_list(const cJSON *list, const char *id) {
  const cJSON *pieces = cJSON_GetObjectItemCaseSensitive(list, "pieces");
  const cJSON *entry = NULL;
  cJSON_ArrayForEach(entry, pieces) {
    const cJSON *entry_id = cJSON_GetObjectItemCaseSensitive(entry, "id");
    if (cJSON_IsString(entry_id) && strcmp(entry_id->valuestring, id) == 0) {
      return entry;
    }
  }
  return NULL;
}

static int list_size(const cJSON *list) {
  return cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(list, "pieces"));
}

/* True when the entry's string member equals value. */
static bool entry_string_is(const cJSON *entry, const char *key, const char *value) {
  const cJSON *item = entry ? cJSON_GetObjectItemCaseSensitive(entry, key) : NULL;
  return cJSON_IsString(item) && strcmp(item->valuestring, value) == 0;
}

/* True when the entry's tags are exactly the given ones, in order. */
static bool entry_tags_are(const cJSON *entry, const char *const *tags, int count) {
  const cJSON *array = entry ? cJSON_GetObjectItemCaseSensitive(entry, "tags") : NULL;
  if (!cJSON_IsArray(array) || cJSON_GetArraySize(array) != count) {
    return false;
  }
  for (int i = 0; i < count; i++) {
    const cJSON *tag = cJSON_GetArrayItem(array, i);
    if (!cJSON_IsString(tag) || strcmp(tag->valuestring, tags[i]) != 0) {
      return false;
    }
  }
  return true;
}

/* The count of the group with this value (case-insensitive) in groups[key], -1 when absent. */
static int group_count(const cJSON *groups, const char *key, const char *value) {
  const cJSON *entry = NULL;
  cJSON_ArrayForEach(entry, cJSON_GetObjectItemCaseSensitive(groups, key)) {
    const cJSON *entry_value = cJSON_GetObjectItemCaseSensitive(entry, "value");
    if (cJSON_IsString(entry_value) && strcasecmp(entry_value->valuestring, value) == 0) {
      return (int)cJSON_GetObjectItemCaseSensitive(entry, "count")->valuedouble;
    }
  }
  return -1;
}

static bool group_shows(const cJSON *groups, const char *key, const char *value) {
  const cJSON *entry = NULL;
  cJSON_ArrayForEach(entry, cJSON_GetObjectItemCaseSensitive(groups, key)) {
    if (entry_string_is(entry, "value", value)) {
      return true;
    }
  }
  return false;
}

/* Composer, source and tags on pieces: edit, normalize, list, filter, group. a, b and c get a
 * composer; plain is never patched and stands for a piece from before the metadata existed. */
static void check_metadata(const char *a, const char *b, const char *c, const char *plain) {
  char *response = patch_piece(a, "{\"composer\":\"John  Field \"}");
  cJSON *answer = cJSON_Parse(response_body(response));
  check(response_has_status(response, 200) && cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(answer, "ok")) &&
            entry_string_is(cJSON_GetObjectItemCaseSensitive(answer, "piece"), "composer", "John Field"),
        "PATCH composer \"John  Field \" answers the piece with \"John Field\"");
  cJSON_Delete(answer);
  free(response);
  cJSON *list = get_json("/api/analyzer/pieces");
  check(entry_string_is(piece_in_list(list, a), "composer", "John Field"), "the list reads the composer back normalized");
  check(entry_string_is(piece_in_list(list, a), "filename", "Mozart 545.mid"), "setting the composer keeps the filename");
  int total = list_size(list);
  cJSON_Delete(list);

  free(patch_piece(b, "{\"composer\":\"john field\"}"));
  free(patch_piece(c, "{\"composer\":\"John Field\",\"source\":\"Nocturnes\"}"));

  /* Tags: replace, then add and remove. */
  response = patch_piece(a, "{\"tags\":[\"Whimsical\",\" Slow \"]}");
  answer = cJSON_Parse(response_body(response));
  const char *replaced[] = {"whimsical", "slow"};
  check(entry_tags_are(cJSON_GetObjectItemCaseSensitive(answer, "piece"), replaced, 2), "tags replace all and are lowercased");
  cJSON_Delete(answer);
  free(response);
  free(patch_piece(b, "{\"add_tags\":[\"slow\"]}"));

  list = get_json("/api/analyzer/pieces?tag=SLOW");
  check(list_size(list) == 2 && piece_in_list(list, a) && piece_in_list(list, b), "filter by tag matches case-insensitively");
  cJSON_Delete(list);
  list = get_json("/api/analyzer/pieces?composer=JOHN%20FIELD");
  check(list_size(list) == 3 && piece_in_list(list, a) && piece_in_list(list, b) && piece_in_list(list, c),
        "filter by composer matches case-insensitively");
  cJSON_Delete(list);
  list = get_json("/api/analyzer/pieces?composer=john%20field&tag=whimsical");
  check(list_size(list) == 1 && piece_in_list(list, a), "composer and tag filters combine");
  cJSON_Delete(list);
  list = get_json("/api/analyzer/pieces?composer=John%20Field&source=nocturnes");
  check(list_size(list) == 1 && piece_in_list(list, c), "composer and source filters combine");
  cJSON_Delete(list);

  response = patch_piece(a, "{\"add_tags\":[\"romantic\",\"SLOW\"],\"remove_tags\":[\"Whimsical\",\"never-had-it\"]}");
  answer = cJSON_Parse(response_body(response));
  const char *edited[] = {"slow", "romantic"};
  check(entry_tags_are(cJSON_GetObjectItemCaseSensitive(answer, "piece"), edited, 2),
        "add_tags skips a duplicate, remove_tags drops one and ignores an absent one");
  cJSON_Delete(answer);
  free(response);

  /* A quote and a backslash in a value round-trip. */
  free(patch_piece(c, "{\"source\":\"The \\\"Nocturnes\\\" \\\\ No. 1\"}"));
  list = get_json("/api/analyzer/pieces");
  check(entry_string_is(piece_in_list(list, c), "source", "The \"Nocturnes\" \\ No. 1"), "a quote and a backslash round-trip");
  cJSON_Delete(list);

  /* Invalid metadata is 400 and writes nothing. */
  response = patch_piece(a, "{\"composer\":\"Changed\",\"add_tags\":[\"a,b\"]}");
  check(response_has_status(response, 400) && strstr(response_body(response), "\"ok\":false") &&
            strstr(response_body(response), "invalid tag"),
        "a tag with a comma is 400 \"invalid tag\"");
  free(response);
  char long_composer[200];
  strcpy(long_composer, "{\"composer\":\"");
  memset(long_composer + strlen(long_composer), 'x', 121);
  strcpy(long_composer + strlen(long_composer) + 0, "\"}");
  response = patch_piece(a, long_composer);
  check(response_has_status(response, 400) && strstr(response_body(response), "composer is too long"),
        "a composer over 120 bytes is 400 \"composer is too long\"");
  free(response);
  char many[1024] = "{\"tags\":[";
  for (int i = 0; i < 33; i++) {
    char tag[24];
    snprintf(tag, sizeof(tag), "%s\"t%d\"", i ? "," : "", i);
    strcat(many, tag);
  }
  strcat(many, "]}");
  response = patch_piece(a, many);
  check(response_has_status(response, 400) && strstr(response_body(response), "too many tags"), "33 tags is 400 \"too many tags\"");
  free(response);
  list = get_json("/api/analyzer/pieces");
  const cJSON *entry = piece_in_list(list, a);
  check(entry_string_is(entry, "composer", "John Field") && entry_tags_are(entry, edited, 2),
        "a rejected PATCH changes nothing");
  cJSON_Delete(list);

  response = patch_piece(a, "{}");
  check(strstr(response_body(response), "\"ok\":false") && strstr(response_body(response), "nothing to update"),
        "an empty body says \"nothing to update\"");
  free(response);
  response = patch_piece(a, "{\"unknown\":\"member\"}");
  check(strstr(response_body(response), "nothing to update"), "unknown members are ignored");
  free(response);
  response = patch_piece("no-such-piece", "{\"composer\":\"X\"}");
  check(response_has_status(response, 404), "PATCH of an unknown piece is 404");
  free(response);

  /* Pieces from before the metadata existed list as unassigned. */
  list = get_json("/api/analyzer/pieces");
  entry = piece_in_list(list, plain);
  const char *none[1] = {NULL};
  check(entry_string_is(entry, "composer", "") && entry_string_is(entry, "source", "") && entry_tags_are(entry, none, 0),
        "a piece without the new members lists as unassigned and untagged");
  cJSON_Delete(list);
  list = get_json("/api/analyzer/pieces?unassigned=composer");
  check(piece_in_list(list, plain) && !piece_in_list(list, a) && !piece_in_list(list, b), "unassigned=composer lists the pieces with no composer");
  cJSON_Delete(list);
  list = get_json("/api/analyzer/pieces?unassigned=composer&unassigned=source&untagged=1");
  check(piece_in_list(list, plain) && !piece_in_list(list, c), "unassigned may repeat and combines with untagged=1");
  cJSON_Delete(list);
  list = get_json("/api/analyzer/pieces?untagged=1");
  check(piece_in_list(list, plain) && !piece_in_list(list, a) && !piece_in_list(list, b) && piece_in_list(list, c),
        "untagged=1 lists the pieces with no tags");
  cJSON_Delete(list);

  /* Groups. */
  cJSON *groups = get_json("/api/analyzer/groups");
  check(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(groups, "ok")), "GET groups answers ok");
  check(group_count(groups, "composers", "john field") == 3 && group_shows(groups, "composers", "John Field") &&
            !group_shows(groups, "composers", "john field"),
        "groups count a composer case-insensitively and show the most used spelling");
  check(group_count(groups, "sources", "the \"nocturnes\" \\ no. 1") == 1, "groups list the sources");
  check(group_count(groups, "tags", "slow") == 2 && group_count(groups, "tags", "romantic") == 1, "groups list the tags with counts");
  const cJSON *unassigned = cJSON_GetObjectItemCaseSensitive(groups, "unassigned");
  const cJSON *total_item = cJSON_GetObjectItemCaseSensitive(groups, "total");
  check(cJSON_IsNumber(total_item) && (int)total_item->valuedouble == total, "groups total counts every piece");
  check((int)cJSON_GetObjectItemCaseSensitive(unassigned, "composer")->valuedouble == total - 3,
        "groups count the pieces without a composer");
  check((int)cJSON_GetObjectItemCaseSensitive(groups, "untagged")->valuedouble == total - 2, "groups count the untagged pieces");
  cJSON_Delete(groups);
  response = http_exchange("DELETE", "/api/analyzer/groups", NULL, NULL, 0);
  check(response_has_status(response, 405), "groups only answers GET");
  free(response);
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

  /* Notated values: two MIDI channels give two named parts, a piano upload
     two staves with beams, and the title and composer reach the score. */
  static const unsigned char kTwoChannelMidi[] = {'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, 0x01, 0xE0, 'M', 'T',
                                                  'r', 'k', 0, 0, 0, 27, 0x00, 0xC0, 0x00, 0x00, 0xC1, 0x28, 0x00,
                                                  0x90, 0x3C, 0x40, 0x00, 0x91, 0x40, 0x40, 0x83, 0x60, 0x80, 0x3C,
                                                  0x40, 0x00, 0x81, 0x40, 0x40, 0x00, 0xFF, 0x2F, 0x00};
  write_file("two-channel.mid", kTwoChannelMidi, sizeof(kTwoChannelMidi));
  char *two_channel_id = upload_file("two-channel.mid", "two-channel.mid", NULL);
  check(two_channel_id != NULL, "a two-channel MIDI file uploads");
  if (two_channel_id && bach_id) {
    check_notated_score(bach_id, two_channel_id);
  }
  free(two_channel_id);

  /* The notation override: set, apply everywhere, clear. */
  if (bach_id) {
    check_notation_override(bach_id);
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

  /* Pieces with chords and overlapping notes draw (the score export writes
     chords and extra voices). */
  const char *chord_pieces[] = {"Clementi 36.mid", "Field 1.mid"};
  const char *chord_names[] = {"Clementi%2036.mid", "Field%201.mid"};
  for (int i = 0; i < 2; i++) {
    char piece_path[512];
    snprintf(piece_path, sizeof(piece_path), "%s/%s", fixtures, chord_pieces[i]);
    char *piece_id = upload_file(piece_path, chord_names[i], NULL);
    char what[256];
    snprintf(what, sizeof(what), "%s uploads", chord_pieces[i]);
    check(piece_id != NULL, what);
    if (piece_id) {
      check_score_drawn(piece_id, chord_pieces[i]);
      check_playback(piece_id, chord_pieces[i]);
    }
    free(piece_id);
  }
  char *chords_id = upload_file(URSATZ_GUI_TEST_URSATZ_DIR "/tests/fixtures/musicxml/chords_backup.musicxml",
                                "chords_backup.musicxml", NULL);
  check(chords_id != NULL, "chords_backup.musicxml uploads");
  if (chords_id) {
    check_score_drawn(chords_id, "chords_backup.musicxml");
    check_playback(chords_id, "chords_backup.musicxml");

    /* A piece whose stored source is gone: the score routes answer 422 with
       a JSON message, not a 200 the viewer would hand to OSMD. */
    char source_path[512];
    snprintf(source_path, sizeof(source_path), "pieces/%s/source.musicxml", chords_id);
    check(remove(source_path) == 0, "the chord piece's stored source can be deleted");
    char route[256];
    snprintf(route, sizeof(route), "/api/analyzer/pieces/%s/musicxml", chords_id);
    char *missing_source = http_get(route);
    check(response_has_status(missing_source, 422), "a score with a missing source file gives 422");
    check(strstr(missing_source, "Content-Type: application/json\r\n") != NULL,
          "the missing-source score error is application/json");
    check(strstr(response_body(missing_source), "\"message\":\"could not import the stored source\"") != NULL,
          "the missing-source score error carries its message");
    free(missing_source);
    snprintf(route, sizeof(route), "/api/analyzer/pieces/%s/musicxml/meta", chords_id);
    missing_source = http_get(route);
    check(response_has_status(missing_source, 422), "musicxml/meta with a missing source file gives 422");
    free(missing_source);
    snprintf(route, sizeof(route), "/api/analyzer/pieces/%s/playback", chords_id);
    missing_source = http_get(route);
    check(response_has_status(missing_source, 422), "playback with a missing source file gives 422");
    check(strstr(response_body(missing_source), "\"message\":\"could not import the stored source\"") != NULL,
          "the missing-source playback error carries its message");
    free(missing_source);
  }
  free(chords_id);

  char *unknown_playback = http_get("/api/analyzer/pieces/no-such-piece/playback");
  check(response_has_status(unknown_playback, 404), "playback for an unknown id gives 404");
  free(unknown_playback);

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

  /* Re-run the analysis: the document is rebuilt from the stored source and
     the versions in meta.json are rewritten; the rest of meta.json stays. */
  if (mozart_id) {
    char analysis_path[512];
    char meta_path[512];
    snprintf(analysis_path, sizeof(analysis_path), "pieces/%s/analysis.json", mozart_id);
    snprintf(meta_path, sizeof(meta_path), "pieces/%s/meta.json", mozart_id);
    char *document_before = read_file(analysis_path, NULL);
    char *meta_text = read_file(meta_path, NULL);
    cJSON *meta = meta_text ? cJSON_Parse(meta_text) : NULL;
    free(meta_text);
    const cJSON *uploaded_before = meta ? cJSON_GetObjectItemCaseSensitive(meta, "uploaded_at") : NULL;
    char uploaded_at_before[32] = "";
    if (cJSON_IsString(uploaded_before)) {
      snprintf(uploaded_at_before, sizeof(uploaded_at_before), "%s", uploaded_before->valuestring);
    }
    if (meta) {
      cJSON_ReplaceItemInObjectCaseSensitive(meta, "analyzer_version", cJSON_CreateString("old"));
      cJSON_ReplaceItemInObjectCaseSensitive(meta, "document_schema_version", cJSON_CreateNumber(1));
      char *stale_meta = cJSON_Print(meta);
      write_file(meta_path, stale_meta, strlen(stale_meta));
      free(stale_meta);
      cJSON_Delete(meta);
    }
    write_file(analysis_path, "{}", 2);

    char path[256];
    snprintf(path, sizeof(path), "/api/analyzer/pieces/%s/reanalyze", mozart_id);
    response = http_exchange("POST", path, NULL, NULL, 0);
    check(response_has_status(response, 200) && strstr(response_body(response), "\"ok\":true") != NULL,
          "POST reanalyze is 200 OK");
    free(response);

    char *document_after = read_file(analysis_path, NULL);
    check(document_before && document_after && strcmp(document_before, document_after) == 0,
          "re-running the analysis rebuilds the same document for the same Analyzer");
    free(document_before);
    free(document_after);

    meta_text = read_file(meta_path, NULL);
    meta = meta_text ? cJSON_Parse(meta_text) : NULL;
    free(meta_text);
    const cJSON *version = meta ? cJSON_GetObjectItemCaseSensitive(meta, "analyzer_version") : NULL;
    const cJSON *schema = meta ? cJSON_GetObjectItemCaseSensitive(meta, "document_schema_version") : NULL;
    const cJSON *uploaded_after = meta ? cJSON_GetObjectItemCaseSensitive(meta, "uploaded_at") : NULL;
    const cJSON *filename = meta ? cJSON_GetObjectItemCaseSensitive(meta, "filename") : NULL;
    check(cJSON_IsString(version) && strcmp(version->valuestring, "old") != 0 && cJSON_IsNumber(schema) &&
              schema->valuedouble == 9,
          "re-running the analysis rewrites the versions in meta.json");
    check(cJSON_IsString(uploaded_after) && strcmp(uploaded_after->valuestring, uploaded_at_before) == 0 &&
              cJSON_IsString(filename) && strcmp(filename->valuestring, "Mozart 545.mid") == 0,
          "re-running the analysis keeps the filename and uploaded_at");
    cJSON_Delete(meta);
  }
  response = http_exchange("POST", "/api/analyzer/pieces/no-such-piece/reanalyze", NULL, NULL, 0);
  check(response_has_status(response, 404), "reanalyze of an unknown id gives 404");
  free(response);

  if (mozart_id && bach_id && no_meter_id && score_id) {
    check_metadata(mozart_id, bach_id, no_meter_id, score_id);
  }

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

    /* Delete. A subject that linked the piece forgets it. */
    cJSON *subject = NULL;
    check(ursatz_gui_subject_create("Delete test", "idea", "", &subject) == URSATZ_GUI_SUBJECT_OK,
          "a subject is created");
    char *subject_id = subject ? strdup(cJSON_GetObjectItemCaseSensitive(subject, "id")->valuestring) : NULL;
    cJSON_Delete(subject);
    subject = NULL;
    check(subject_id && ursatz_gui_subject_link_piece(subject_id, mozart_id, NULL, NULL) == URSATZ_GUI_SUBJECT_OK,
          "the subject links the piece");
    char dir[256];
    snprintf(dir, sizeof(dir), "pieces/%s", mozart_id);
    response = http_exchange("DELETE", path, NULL, NULL, 0);
    check(response_has_status(response, 200), "DELETE removes a piece");
    free(response);
    check(subject_id && ursatz_gui_subject_get(subject_id, &subject) == URSATZ_GUI_SUBJECT_OK &&
              cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(subject, "pieces")) == 0,
          "deleting a piece removes it from the subjects that linked it");
    cJSON_Delete(subject);
    if (subject_id) {
      ursatz_gui_subject_delete(subject_id);
    }
    free(subject_id);
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
