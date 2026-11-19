#include "composer.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <civetweb.h>
#include <cJSON.h>

#include "route_util.h"
#include "../server.h"
#include "workshop_internal.h"

static const char *kComposerPrefix = "/api/composer";
#define MAX_SEGMENTS 8

static int send_error(struct mg_connection *conn, int status, const char *error) {
  cJSON *object = cJSON_CreateObject();
  if (object) {
    cJSON_AddStringToObject(object, "error", error);
  }
  return ursatz_gui_send_object(conn, status, object);
}

/* GET /api/composer/status: the versions this build holds, from the workshop (workshop_status). "version" keeps the text the command
 * printed; api_version, analyzer_version (the Analyzer's, the workshop holds both apps) and producer_key are additive. */
static int handle_status(struct mg_connection *conn) {
  cJSON *versions = NULL;
  UrsatzError failure = {{0}};
  Workshop *workshop = ursatz_gui_workshop();
  if (!workshop || workshop_status(workshop, &versions, &failure) != URSATZ_OK) {
    cJSON *object = cJSON_CreateObject();
    if (object) {
      cJSON_AddStringToObject(object, "error", "composer failed");
      cJSON_AddStringToObject(object, "stderr", failure.message);
    }
    return ursatz_gui_send_object(conn, 500, object);
  }
  const cJSON *composer_version = cJSON_GetObjectItemCaseSensitive(versions, "composer_version");
  const cJSON *api_version = cJSON_GetObjectItemCaseSensitive(versions, "api_version");
  const cJSON *producer_key = cJSON_GetObjectItemCaseSensitive(versions, "producer_key");
  const cJSON *analyzer = cJSON_GetObjectItemCaseSensitive(versions, "analyzer_version");
  char version[128];
  snprintf(version, sizeof(version), "ursatz-composer %s", cJSON_IsString(composer_version) ? composer_version->valuestring : "");
  cJSON *object = cJSON_CreateObject();
  bool built = object && cJSON_IsString(composer_version) && cJSON_IsString(api_version) &&
               cJSON_IsString(producer_key) && cJSON_IsString(analyzer) &&
               cJSON_AddTrueToObject(object, "ok") && cJSON_AddTrueToObject(object, "available") &&
               cJSON_AddStringToObject(object, "version", version) &&
               cJSON_AddStringToObject(object, "api_version", api_version->valuestring) &&
               cJSON_AddStringToObject(object, "analyzer_version", analyzer->valuestring) &&
               cJSON_AddStringToObject(object, "producer_key", producer_key->valuestring);
  cJSON_Delete(versions);
  if (!built) {
    cJSON_Delete(object);
    return send_error(conn, 500, "composer failed");
  }
  return ursatz_gui_send_object(conn, 200, object);
}

/* GET /api/composer/vocabulary: the words the Composer reads, as composer_api_vocabulary gives them (built once and
 * kept, R-270). */
static int handle_vocabulary(struct mg_connection *conn) {
  const char *vocabulary = workshop_composer_vocabulary();
  if (!vocabulary) {
    return send_error(conn, 500, "composer failed");
  }
  return ursatz_gui_send_json_status(conn, 200, "OK", vocabulary);
}

/* GET /api/composer/forms: the forms the Composer builds and their sections, sizes and slots, as composer_api_forms
 * gives them (built once and kept). */
static int handle_forms(struct mg_connection *conn) {
  const char *forms = workshop_composer_forms();
  if (!forms) {
    return send_error(conn, 500, "composer failed");
  }
  return ursatz_gui_send_json_status(conn, 200, "OK", forms);
}

static size_t split_path(char *rest, char *segments[MAX_SEGMENTS]) {
  size_t count = 0;
  char *start = rest;
  for (;;) {
    char *slash = strchr(start, '/');
    if (*start == '\0' || slash == start || count == MAX_SEGMENTS) {
      return 0;
    }
    segments[count++] = start;
    if (!slash) {
      return count;
    }
    *slash = '\0';
    start = slash + 1;
  }
}

/*
 * Single handler registered at kComposerPrefix ("/api/composer"). civetweb
 * routes the prefix and every "prefix/something" URI to it (see the note in
 * analyzer.c), so each composer route is dispatched here by local_uri.
 */
static int composer_handler(struct mg_connection *conn, void *cbdata) {
  (void)cbdata;
  const struct mg_request_info *info = mg_get_request_info(conn);
  const char *uri = info->local_uri;
  const char *method = info->request_method;
  size_t prefix_len = strlen(kComposerPrefix);

  if (strncmp(uri, kComposerPrefix, prefix_len) != 0 || uri[prefix_len] != '/') {
    return send_error(conn, 404, "not found");
  }

  char rest[256];
  if (snprintf(rest, sizeof(rest), "%s", uri + prefix_len + 1) >= (int)sizeof(rest)) {
    return send_error(conn, 404, "not found");
  }
  char *segments[MAX_SEGMENTS];
  size_t count = split_path(rest, segments);
  if (count == 0) {
    return send_error(conn, 404, "not found");
  }

  if (count == 1 && strcmp(segments[0], "status") == 0) {
    if (strcmp(method, "GET") != 0) {
      return send_error(conn, 405, "method not allowed");
    }
    return handle_status(conn);
  }

  if (count == 1 && strcmp(segments[0], "vocabulary") == 0) {
    if (strcmp(method, "GET") != 0) {
      return send_error(conn, 405, "method not allowed");
    }
    return handle_vocabulary(conn);
  }

  if (count == 1 && strcmp(segments[0], "forms") == 0) {
    if (strcmp(method, "GET") != 0) {
      return send_error(conn, 405, "method not allowed");
    }
    return handle_forms(conn);
  }

  return send_error(conn, 404, "not found");
}

void ursatz_gui_composer_register(struct mg_context *ctx) {
  mg_set_request_handler(ctx, kComposerPrefix, composer_handler, NULL);
}
