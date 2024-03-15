#include <signal.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>

#include "server.h"

static volatile bool keep_running = true;

static void handle_signal(int sig) {
  (void)sig;
  keep_running = false;
}

int main(void) {
  signal(SIGINT, handle_signal);
  signal(SIGTERM, handle_signal);

  struct mg_context *ctx = ursatz_gui_server_start("8765", URSATZ_GUI_FRONTEND_DIR);
  if (!ctx) {
    fprintf(stderr, "ursatz-gui-backend: failed to start server\n");
    return 1;
  }

  printf("ursatz-gui-backend listening on http://localhost:8765\n");

  while (keep_running) {
    /* civetweb serves requests on its own thread pool; the main thread
     * just waits for a shutdown signal. */
    struct timespec ts = {0, 100 * 1000 * 1000};
    nanosleep(&ts, NULL);
  }

  ursatz_gui_server_stop(ctx);
  printf("ursatz-gui-backend stopped\n");
  return 0;
}
