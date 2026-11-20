#include "server.h"

#include <civetweb.h>
#include <stdio.h>

#include "routes/route_table.h"


static Workshop *g_workshop = NULL;

Workshop *ursatz_gui_workshop(void) {
  return g_workshop;
}

struct mg_context *ursatz_gui_server_start(Workshop *workshop, const char *listen_port, const char *frontend_dir) {
  char threads[16];
  snprintf(threads, sizeof(threads), "%d", workshop_http_threads(workshop));
  const char *options[] = {
      "listening_ports", listen_port,
      "document_root", frontend_dir,
      "num_threads", threads,
      NULL,
  };

  /* The workshop starts its stores and its job workers before any route (it also makes the data folder the working directory). */
  UrsatzError error = {{0}, {0}};
  if (workshop_start(workshop, &error) != URSATZ_OK) {
    fprintf(stderr, "ursatz-gui-backend: %s\n", error.message);
    return NULL;
  }

  struct mg_context *ctx = mg_start(NULL, NULL, options);
  if (!ctx) {
    workshop_stop(workshop);
    return NULL;
  }

  /* Route registration: each tool module registers itself here. This is
   * the only place that wires tool routes into the running server. */
  g_workshop = workshop;
  ursatz_gui_routes_register(ctx);
  return ctx;
}

void ursatz_gui_server_stop(struct mg_context *ctx) {
  if (ctx) {
    mg_stop(ctx);
  }
  workshop_stop(g_workshop); /* cancels what runs and joins the workers, after the last request is answered */
  g_workshop = NULL;
}
