#include "composer_routes.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <civetweb.h>
#include <cJSON.h>

#include "route_util.h"
#include "../server.h"


static int send_error(struct mg_connection *conn, int status, const char *error) {
  cJSON *object = cJSON_CreateObject();
  if (object) {
    cJSON_AddStringToObject(object, "error", error);
  }
  return ursatz_gui_send_object(conn, status, object);
}

/* The failure of a Composer query: {"error"} with the status of the table. */
static int failed_composer(struct mg_connection *conn, UrsatzStatus status, const UrsatzError *error) {
  return send_error(conn, ursatz_gui_http_status(status, NULL), error->message);
}

/* GET /api/composer/status: the versions this build holds, from the workshop. "version" keeps the text the command printed; api_version,
 * analyzer_version (the Analyzer's, the workshop holds both apps) and producer_key are additive. */
static int handle_status(struct mg_connection *conn) {
  return ursatz_gui_answer_query(conn, "composer.status", NULL, NULL, failed_composer, false);
}

/* GET /api/composer/vocabulary: the words the Composer reads (built once and kept, R-270). */
static int handle_vocabulary(struct mg_connection *conn) {
  return ursatz_gui_answer_query(conn, "composer.vocabulary", NULL, NULL, failed_composer, true);
}

/* GET /api/composer/forms: the forms the Composer builds and their sections, sizes and slots (built once and kept). */
static int handle_forms(struct mg_connection *conn) {
  return ursatz_gui_answer_query(conn, "composer.forms", NULL, NULL, failed_composer, true);
}

static int route_status(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { (void)args; return handle_status(conn); }
static int route_vocabulary(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { (void)args; return handle_vocabulary(conn); }
static int route_forms(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) { (void)args; return handle_forms(conn); }

static const UrsatzGuiRoute kRoutes[] = {
    {"GET", "/api/composer/status", route_status},
    {"GET", "/api/composer/vocabulary", route_vocabulary},
    {"GET", "/api/composer/forms", route_forms},
};

const UrsatzGuiRoute *ursatz_gui_composer_routes(size_t *count) {
  *count = sizeof(kRoutes) / sizeof(kRoutes[0]);
  return kRoutes;
}
