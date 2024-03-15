#include "health.h"

#include <stdio.h>
#include <stdlib.h>

#include <civetweb.h>

#include "../service/persistence_service.h"

static const char *kHealthTestDbPath = "ursatz_gui_health_check.db";

static int health_status_handler(struct mg_connection *conn, void *cbdata) {
  (void)cbdata;

  char *message = NULL;
  bool ok = ursatz_gui_persistence_check(kHealthTestDbPath, &message);

  char body[512];
  snprintf(body, sizeof(body),
           "{\"ok\":%s,\"message\":\"%s\"}",
           ok ? "true" : "false",
           message ? message : "");
  free(message);

  mg_send_http_ok(conn, "application/json", (long)strlen(body));
  mg_write(conn, body, strlen(body));
  return 200;
}

void ursatz_gui_health_register(struct mg_context *ctx) {
  mg_set_request_handler(ctx, "/api/health/status", health_status_handler, NULL);
}
