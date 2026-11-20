#include "server_support.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>

#include <ursatz_workshop/workshop_api.h>

#include "server.h"
#include "test_http.h"

static Workshop *g_workshop = NULL;

struct mg_context *test_gui_server_start(const char *port) {
  WorkshopConfig flags;
  memset(&flags, 0, sizeof(flags));
  flags.data_dir = ".";
  WorkshopConfig settings;
  char storage[2048];
  UrsatzError error = {{0}};
  if (workshop_settings_resolve(&flags, &settings, storage, sizeof(storage), &error) != URSATZ_OK ||
      workshop_open(&settings, &g_workshop, &error) != URSATZ_OK) {
    fprintf(stderr, "test_gui_server_start: %s\n", error.message);
    return NULL;
  }
  struct mg_context *ctx = ursatz_gui_server_start(g_workshop, port, ".");
  if (!ctx) {
    workshop_close(g_workshop);
    g_workshop = NULL;
  }
  return ctx;
}

void test_gui_server_stop(struct mg_context *ctx) {
  ursatz_gui_server_stop(ctx);
  workshop_close(g_workshop);
  g_workshop = NULL;
}

bool test_gui_item_dir(const char *id, char *out, size_t size) {
  DIR *projects = opendir("projects");
  bool found = false;
  for (struct dirent *entry = projects ? readdir(projects) : NULL; entry && !found; entry = readdir(projects)) {
    if (entry->d_name[0] == '.') {
      continue;
    }
    struct stat st;
    found = snprintf(out, size, "projects/%s/items/%s", entry->d_name, id) < (int)size && stat(out, &st) == 0 && S_ISDIR(st.st_mode);
  }
  if (projects) {
    closedir(projects);
  }
  return found;
}

bool test_gui_version_dir(const char *id, int version, char *out, size_t size) {
  char item[400];
  return test_gui_item_dir(id, item, sizeof(item)) && snprintf(out, size, "%s/v%d", item, version) < (int)size;
}

/* The command object as JSON text; NULL when params_json is not JSON. */
static char *command_text(const char *name, const char *target, int base_version, const char *params_json) {
  cJSON *command = cJSON_CreateObject();
  cJSON *params = cJSON_Parse(params_json ? params_json : "{}");
  char *text = NULL;
  if (command && params && cJSON_AddStringToObject(command, "command", name) && cJSON_AddStringToObject(command, "actor", "michelle") &&
      (!target || target[0] == '\0' || cJSON_AddStringToObject(command, "target", target)) &&
      (base_version < 0 || cJSON_AddNumberToObject(command, "base_version", base_version)) && cJSON_AddItemToObject(command, "params", params)) {
    params = NULL;
    text = cJSON_PrintUnformatted(command);
  }
  cJSON_Delete(params);
  cJSON_Delete(command);
  return text;
}

static bool answered(char *response, int *http_status, cJSON **reply) {
  cJSON *json = response ? cJSON_Parse(test_http_body(response)) : NULL;
  if (http_status) {
    *http_status = test_http_status(response);
  }
  free(response);
  bool parsed = json != NULL;
  if (reply) {
    *reply = json;
  } else {
    cJSON_Delete(json);
  }
  return parsed;
}

bool test_gui_command(const char *port, const char *name, const char *target, int base_version, const char *params_json, int *http_status,
                      cJSON **reply) {
  char *text = command_text(name, target, base_version, params_json);
  char *response = text ? test_http_exchange(port, "POST", "/api/workshop/command", NULL, text, strlen(text)) : NULL;
  free(text);
  return answered(response, http_status, reply);
}

bool test_gui_command_finish(const char *port, const char *name, const char *target, int base_version, const char *params_json, int *http_status,
                             cJSON **reply) {
  char *text = command_text(name, target, base_version, params_json);
  char *response = text ? test_http_exchange(port, "POST", "/api/workshop/command", NULL, text, strlen(text)) : NULL;
  free(text);
  return answered(test_http_finish(port, response), http_status, reply);
}

bool test_gui_command_file(const char *port, const char *name, const char *target, int base_version, const char *params_json,
                           const char *encoded_filename, const char *data, size_t size, int *http_status, cJSON **reply) {
  char *text = command_text(name, target, base_version, params_json);
  char *response = text ? test_http_command_file(port, text, encoded_filename, data, size) : NULL;
  free(text);
  return answered(test_http_finish(port, response), http_status, reply);
}

int test_gui_item_version(const char *port, const char *id) {
  char path[320];
  snprintf(path, sizeof(path), "/api/materials/items/%s", id);
  int status = 0;
  cJSON *answer = test_http_json(port, "GET", path, NULL, &status);
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(answer, "item");
  const cJSON *current = cJSON_GetObjectItemCaseSensitive(item, "current_version");
  int version = cJSON_IsNumber(current) ? (int)current->valuedouble : -1;
  cJSON_Delete(answer);
  return version;
}

static int implied_base_version(const char *port, const char *name, const char *target) {
  bool item_write = strncmp(name, "item.", 5) == 0 && target && target[0] != '\0';
  return item_write ? test_gui_item_version(port, target) : -1;
}

cJSON *test_gui_send(const char *port, const char *name, const char *target, const char *params_json, int *http_status) {
  cJSON *reply = NULL;
  test_gui_command(port, name, target, implied_base_version(port, name, target), params_json, http_status, &reply);
  return reply;
}

cJSON *test_gui_send_finish(const char *port, const char *name, const char *target, const char *params_json, int *http_status) {
  cJSON *reply = NULL;
  test_gui_command_finish(port, name, target, implied_base_version(port, name, target), params_json, http_status, &reply);
  return reply;
}
