#include "server_support.h"

#include <dirent.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>

#include <ursatz_workshop/workshop_api.h>

#include "server.h"

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
