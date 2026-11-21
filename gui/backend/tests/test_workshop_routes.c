/* The generic workshop routes (R-659): any query or command of the workshop by its name, as the command line connects to a process that holds a
 * data folder. The test serves its working directory as the data folder and calls the routes with the shared HTTP client. */

#include <civetweb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cJSON.h>

#include "support/server_support.h"
#include "server.h"
#include "ursatz_check.h"
#include "test_http.h"

#define PORT "18780"

/* A percent-encoded copy of text (every byte but a letter, a digit, "-", "_" and "."). */
static char *encode(const char *text) {
  size_t length = strlen(text);
  char *out = malloc(length * 3 + 1);
  size_t used = 0;
  for (size_t i = 0; i < length; i++) {
    unsigned char c = (unsigned char)text[i];
    if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '-' || c == '_' || c == '.') {
      out[used++] = (char)c;
    } else {
      used += (size_t)snprintf(out + used, 4, "%%%02X", c);
    }
  }
  out[used] = '\0';
  return out;
}

static const char *text_of(const cJSON *object, const char *name) {
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
  return cJSON_IsString(item) ? item->valuestring : "";
}

/* Whether the last line of the journal records the command with the actor. */
static bool last_record_is(const char *command, const char *actor) {
  char *journal = test_read_file("journal/journal.jsonl", NULL);
  if (!journal) {
    return false;
  }
  char *last = journal;
  for (char *line = strtok(journal, "\n"); line; line = strtok(NULL, "\n")) {
    last = line;
  }
  cJSON *record = cJSON_Parse(last);
  bool same = record && strcmp(text_of(record, "command"), command) == 0 && strcmp(text_of(record, "actor"), actor) == 0;
  cJSON_Delete(record);
  free(journal);
  return same;
}

/* Whether a TCP connection to host:PORT is accepted. */
static bool connects_to(const char *host) {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    return false;
  }
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof addr);
  addr.sin_family = AF_INET;
  addr.sin_port = htons((unsigned short)atoi(PORT));
  inet_pton(AF_INET, host, &addr.sin_addr);
  bool connected = connect(sock, (struct sockaddr *)&addr, sizeof addr) == 0;
  close(sock);
  return connected;
}

int main(void) {
  struct mg_context *ctx = test_gui_server_start(PORT);
  check(ctx != NULL, "server starts");
  if (!ctx) {
    return 1;
  }
  test_sleep_ms(200);
  int status = 0;

  /* a query by its name: the params are the body */
  cJSON *json = test_http_json(PORT, "POST", "/api/workshop/query/project.list", "{}", &status);
  check(status == 200 && cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(json, "ok")) && cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(json, "projects")),
        "project.list through the query route");
  cJSON_Delete(json);
  json = test_http_json(PORT, "POST", "/api/workshop/query/project.list", NULL, &status);
  check(status == 200, "an empty body is {}");
  cJSON_Delete(json);

  /* a command by its name, as the actor it names */
  json = test_http_json(PORT, "POST", "/api/workshop/command",
                        "{\"command\":\"project.create\",\"actor\":\"claude\",\"params\":{\"name\":\"Remote\"}}", &status);
  check(status == 200 && cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(json, "ok")) && cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(json, "journal_seq")),
        "project.create with actor claude through the command route");
  char project[128];
  snprintf(project, sizeof(project), "%s", text_of(cJSON_GetObjectItemCaseSensitive(json, "project"), "id"));
  cJSON_Delete(json);
  check(last_record_is("project.create", "claude"), "the journal record has the actor claude");

  /* a file as a raw body, the command in a header */
  char *file_data = test_read_file(URSATZ_GUI_TEST_MIDI, NULL);
  size_t file_size = 0;
  char *again = test_read_file(URSATZ_GUI_TEST_MIDI, &file_size);
  free(again);
  check(file_data != NULL && file_size > 0, "the fixture MIDI file reads");
  const char *import = "{\"command\":\"library.import\",\"actor\":\"claude\",\"params\":{\"filename\":\"Bach 114.mid\"}}";
  char *encoded_command = encode(import);
  char *encoded_name = encode("Bach 114.mid");
  char headers[4096];
  snprintf(headers, sizeof(headers), "Content-Type: application/octet-stream\r\nX-Ursatz-Command: %s\r\nX-Filename: %s\r\n", encoded_command, encoded_name);
  char *response = test_http_exchange_headers(PORT, "POST", "/api/workshop/command", headers, file_data, file_size);
  cJSON *accepted = response ? cJSON_Parse(test_http_body(response)) : NULL;
  const cJSON *job = cJSON_GetObjectItemCaseSensitive(accepted, "job");
  check(test_http_status(response) == 202 && cJSON_IsString(cJSON_GetObjectItemCaseSensitive(job, "id")), "library.import of a raw body is accepted as a job");
  char job_path[160];
  snprintf(job_path, sizeof(job_path), "{\"id\":\"%s\"}", text_of(job, "id"));
  const char *state = "";
  cJSON *polled = NULL;
  for (int attempt = 0; attempt < 600 && strcmp(state, "done") != 0 && strcmp(state, "failed") != 0; attempt++) {
    cJSON_Delete(polled);
    polled = test_http_json(PORT, "POST", "/api/workshop/query/job.get", job_path, &status);
    state = text_of(cJSON_GetObjectItemCaseSensitive(polled, "job"), "state");
    test_sleep_ms(50);
  }
  check(strcmp(state, "done") == 0, "the import job is done (waited for through the query route job.get)");
  cJSON_Delete(polled);
  cJSON_Delete(accepted);
  free(response);
  free(encoded_command);
  free(encoded_name);
  free(file_data);

  /* a stale base version: 409 with conflict and the current version */
  char create[3000];
  snprintf(create, sizeof(create),
           "{\"command\":\"item.create\",\"actor\":\"claude\",\"params\":{\"type\":\"motif\",\"name\":\"Source\",\"project_id\":\"%s\",\"body\":"
           "{\"uses\":[],\"motif\":{\"role\":\"melodic\",\"form\":\"full\",\"notes\":\"C4:e Eb4:e G4:e Eb4:e C4:e Eb4:e G4:e C5:e\","
           "\"meter\":\"4/4\",\"key\":\"C minor\",\"pickup\":\"0\",\"features\":[],\"analysis\":null,\"derived\":null}}}}",
           project);
  json = test_http_json(PORT, "POST", "/api/workshop/command", create, &status);
  char item[128];
  snprintf(item, sizeof(item), "%s", text_of(cJSON_GetObjectItemCaseSensitive(json, "item"), "id"));
  check(status == 200 && item[0] != '\0', "item.create through the command route");
  cJSON_Delete(json);
  char save[1200];
  snprintf(save, sizeof(save),
           "{\"command\":\"item.save\",\"actor\":\"claude\",\"target\":\"%s\",\"base_version\":7,\"params\":{\"note\":\"x\",\"body\":"
           "{\"uses\":[],\"motif\":{\"role\":\"melodic\",\"form\":\"full\",\"notes\":\"C4:q Eb4:e G4:e C5:h\",\"meter\":\"4/4\","
           "\"key\":\"C minor\",\"pickup\":\"0\",\"features\":[],\"analysis\":null,\"derived\":null}}}}",
           item);
  json = test_http_json(PORT, "POST", "/api/workshop/command", save, &status);
  check(status == 409 && cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(json, "ok")) && strcmp(text_of(json, "code"), "conflict") == 0 &&
            cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(json, "current_version")),
        "item.save with a stale base version answers 409 with code conflict and current_version");
  cJSON_Delete(json);

  /* refusals: an unknown query, an unknown command, a body that is not an object */
  json = test_http_json(PORT, "POST", "/api/workshop/query/no.such.query", "{}", &status);
  check(status == 400 && strcmp(text_of(json, "code"), "request") == 0 && strcmp(text_of(json, "path"), "/name") == 0,
        "an unknown query is 400 with code request and path /name");
  cJSON_Delete(json);
  json = test_http_json(PORT, "POST", "/api/workshop/command", "{\"command\":\"no.such\",\"actor\":\"claude\"}", &status);
  check(status == 400 && strcmp(text_of(json, "code"), "request") == 0 && strcmp(text_of(json, "path"), "/command") == 0, "an unknown command is 400 at /command");
  cJSON_Delete(json);
  json = test_http_json(PORT, "POST", "/api/workshop/command", "[1]", &status);
  check(status == 400 && strcmp(text_of(json, "path"), "/body") == 0, "a body that is not an object is 400 at /body");
  cJSON_Delete(json);

  /* the status: the folder and the process */
  json = test_http_json(PORT, "GET", "/api/workshop/status", NULL, &status);
  check(status == 200 && strstr(text_of(json, "data_dir"), "workshop_routes") != NULL && cJSON_GetObjectItemCaseSensitive(json, "pid")->valuedouble > 0 &&
            cJSON_IsString(cJSON_GetObjectItemCaseSensitive(json, "composer_version")) && cJSON_IsString(cJSON_GetObjectItemCaseSensitive(json, "recovery")),
        "the status route gives data_dir, pid, the versions and the recovery");
  cJSON_Delete(json);
  json = test_http_json(PORT, "GET", "/api/workshop/command", NULL, &status);
  check(status == 405, "the command route accepts only POST");
  cJSON_Delete(json);

  /* the server listens on 127.0.0.1 only (R-684): another loopback address is refused */
  check(connects_to("127.0.0.1"), "127.0.0.1 connects");
  check(!connects_to("127.0.0.2"), "127.0.0.2 is refused");

  test_gui_server_stop(ctx);
  return test_finish("workshop_routes");
}
