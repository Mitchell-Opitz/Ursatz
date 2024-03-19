#include <signal.h>
#include <stdio.h>
#include <stdbool.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

#include "server.h"

static void sleep_100ms(void) {
#ifdef _WIN32
  Sleep(100);
#else
  struct timespec ts = {0, 100 * 1000 * 1000};
  nanosleep(&ts, NULL);
#endif
}

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
    sleep_100ms();
  }

  ursatz_gui_server_stop(ctx);
  printf("ursatz-gui-backend stopped\n");
  return 0;
}
