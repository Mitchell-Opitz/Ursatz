#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <cJSON.h>

#include <ursatz_workshop/workshop_api.h>

#include "server.h"

#define USAGE "usage: ursatz-gui-backend [--data <dir>] [--port <n>] [--migrate [--dry-run] [--backup <dir>]]\n"

static void sleep_100ms(void) {
  struct timespec ts = {0, 100 * 1000 * 1000};
  nanosleep(&ts, NULL);
}

static volatile bool keep_running = true;

static void handle_signal(int sig) {
  (void)sig;
  keep_running = false;
}

/* --data <dir>, --port <n> and the migration's --migrate, --dry-run and --backup <dir>; anything else is a usage error. */
static bool parse_flags(int argc, char **argv, WorkshopConfig *flags, bool *migrate, WorkshopMigrateOptions *options) {
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--migrate") == 0) {
      *migrate = true;
    } else if (strcmp(argv[i], "--dry-run") == 0) {
      options->dry_run = true;
    } else if (strcmp(argv[i], "--backup") == 0 && i + 1 < argc) {
      options->backup_dir = argv[++i];
    } else if (strcmp(argv[i], "--data") == 0 && i + 1 < argc) {
      flags->data_dir = argv[++i];
    } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
      char *end = NULL;
      errno = 0;
      long port = strtol(argv[++i], &end, 10);
      if (end == argv[i] || *end != '\0' || errno != 0 || port < 1 || port > 65535) {
        fprintf(stderr, "ursatz-gui-backend: --port takes a whole number from 1 to 65535\n");
        return false;
      }
      flags->port = (int)port;
    } else {
      return false;
    }
  }
  return true;
}

int main(int argc, char **argv) {
  WorkshopConfig flags;
  memset(&flags, 0, sizeof(flags));
  bool migrate = false;
  WorkshopMigrateOptions migrate_options;
  memset(&migrate_options, 0, sizeof(migrate_options));
  if (!parse_flags(argc, argv, &flags, &migrate, &migrate_options) ||
      (!migrate && (migrate_options.dry_run || migrate_options.backup_dir))) {
    fputs(USAGE, stderr);
    return 2;
  }

  WorkshopConfig settings;
  char storage[2048];
  UrsatzError error = {{0}};
  Workshop *workshop = NULL;
  if (migrate) {
    /* The migration of the GUI's data folder to layout 1 (R-642): the report is printed, then the process exits. */
    cJSON *report = NULL;
    if (workshop_settings_resolve(&flags, &settings, storage, sizeof(storage), &error) != URSATZ_OK ||
        workshop_migrate(settings.data_dir, &migrate_options, &report, &error) != URSATZ_OK) {
      fprintf(stderr, "ursatz-gui-backend: %s\n", error.message);
      return 1;
    }
    char *text = report ? cJSON_Print(report) : NULL;
    printf("%s\n", text ? text : "{}");
    free(text);
    cJSON_Delete(report);
    return 0;
  }
  if (workshop_settings_resolve(&flags, &settings, storage, sizeof(storage), &error) != URSATZ_OK ||
      workshop_open(&settings, &workshop, &error) != URSATZ_OK) {
    fprintf(stderr, "ursatz-gui-backend: %s\n", error.message);
    return 1;
  }

  if (workshop_recovery_summary(workshop)[0] != '\0') {
    fprintf(stderr, "ursatz-gui-backend: %s\n", workshop_recovery_summary(workshop));
  }
  signal(SIGINT, handle_signal);
  signal(SIGTERM, handle_signal);

  char port[16];
  snprintf(port, sizeof(port), "%d", workshop_port(workshop));
  struct mg_context *ctx = ursatz_gui_server_start(workshop, port, URSATZ_GUI_FRONTEND_DIR);
  if (!ctx) {
    fprintf(stderr, "ursatz-gui-backend: failed to start server\n");
    workshop_close(workshop);
    return 1;
  }

  /* The lock file names the port, so a command line that finds the folder in use connects here (R-659). */
  if (workshop_announce(workshop, workshop_port(workshop)) != URSATZ_OK) {
    fprintf(stderr, "ursatz-gui-backend: cannot write the port into the lock file\n");
  }
  printf("ursatz-gui-backend listening on http://localhost:%s\n", port);
  fflush(stdout);

  while (keep_running) {
    /* civetweb serves requests on its own thread pool; the main thread
     * just waits for a shutdown signal. */
    sleep_100ms();
  }

  ursatz_gui_server_stop(ctx);
  workshop_close(workshop);
  printf("ursatz-gui-backend stopped\n");
  return 0;
}
