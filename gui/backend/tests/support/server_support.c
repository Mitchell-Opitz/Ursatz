#include "server_support.h"

#include <stdio.h>
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
