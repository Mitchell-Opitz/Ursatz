#include "links.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <civetweb.h>

#include "route_util.h"
#include "workshop_internal.h"

static const char *kLinksPrefix = "/api/links";

/* --- scope ---------------------------------------------------------------- */

/* --- handlers ------------------------------------------------------------- */

static int send_link_failure(struct mg_connection *conn, UrsatzStatus result) {
  return ursatz_gui_send_service_failure(conn, result, "link operation failed");
}

/* {"ok":true,<key>:value}; takes value. */
static int send_ok_with(struct mg_connection *conn, int status, const char *key, cJSON *value) {
  cJSON *object = cJSON_CreateObject();
  if (!object || !value || !cJSON_AddBoolToObject(object, "ok", 1) || !cJSON_AddItemToObject(object, key, value)) {
    cJSON_Delete(value);
    cJSON_Delete(object);
    return ursatz_gui_send_failure(conn, 500, "out of memory");
  }
  return ursatz_gui_send_object(conn, status, object);
}

static int handle_start_run(struct mg_connection *conn) {
  cJSON *body = ursatz_gui_read_json_body(conn, NULL);
  if (!body) {
    return ursatz_gui_send_failure(conn, 400, "invalid request body");
  }
  WorkshopLinkScope scope;
  if (!workshop_link_scope_parse(cJSON_GetObjectItemCaseSensitive(body, "scope"), &scope)) {
    cJSON_Delete(body);
    return ursatz_gui_send_failure(conn, 400, "invalid scope");
  }
  char *id = NULL;
  char job_id[WORKSHOP_JOB_ID_BYTES];
  UrsatzStatus result = workshop_link_run_start(&scope, &id, job_id);
  cJSON_Delete(body);
  if (result != URSATZ_OK) {
    return send_link_failure(conn, result);
  }
  cJSON *run = NULL;
  result = workshop_link_run_get(id, &run);
  free(id);
  if (result != URSATZ_OK) {
    return send_link_failure(conn, result);
  }
  /* 202 {"ok", "run", "job"}: the page follows the job (BackendJobs.runJob) and gets {"ok", "run"} when it ends. */
  cJSON *object = cJSON_CreateObject();
  cJSON *job = object ? cJSON_AddObjectToObject(object, "job") : NULL;
  if (!job || !cJSON_AddBoolToObject(object, "ok", 1) || !cJSON_AddItemToObject(object, "run", run) ||
      !cJSON_AddStringToObject(job, "id", job_id) || !cJSON_AddStringToObject(job, "kind", "links") ||
      !cJSON_AddStringToObject(job, "state", workshop_job_state_name(WORKSHOP_JOB_QUEUED))) {
    cJSON_Delete(object);
    if (!object) {
      cJSON_Delete(run);
    }
    return ursatz_gui_send_failure(conn, 500, "out of memory");
  }
  return ursatz_gui_send_object(conn, 202, object);
}

static int handle_list_runs(struct mg_connection *conn) {
  cJSON *runs = NULL;
  UrsatzStatus result = workshop_link_run_list(&runs);
  if (result != URSATZ_OK) {
    return send_link_failure(conn, result);
  }
  return send_ok_with(conn, 200, "runs", runs);
}

static int handle_get_run(struct mg_connection *conn, const char *id) {
  cJSON *run = NULL;
  UrsatzStatus result = workshop_link_run_get(id, &run);
  if (result != URSATZ_OK) {
    return result == URSATZ_ERR_NOT_FOUND ? ursatz_gui_send_failure(conn, 404, "run not found")
                                                    : send_link_failure(conn, result);
  }
  if (!workshop_link_titles_for_run(run)) {
    cJSON_Delete(run);
    return ursatz_gui_send_failure(conn, 500, "out of memory");
  }
  return send_ok_with(conn, 200, "run", run);
}

static int handle_delete_run(struct mg_connection *conn, const char *id) {
  UrsatzStatus result = workshop_link_run_delete(id);
  if (result == URSATZ_ERR_NOT_FOUND) {
    return ursatz_gui_send_failure(conn, 404, "run not found");
  }
  if (result != URSATZ_OK) {
    return send_link_failure(conn, result);
  }
  ursatz_gui_send_json(conn, "{\"ok\":true}");
  return 200;
}

static int handle_piece_links(struct mg_connection *conn, const char *piece_id) {
  cJSON *links = NULL;
  UrsatzStatus result = workshop_link_piece_links(piece_id, &links);
  if (result != URSATZ_OK) {
    return send_link_failure(conn, result);
  }
  if (!workshop_link_titles_for_piece_links(links)) {
    cJSON_Delete(links);
    return ursatz_gui_send_failure(conn, 500, "out of memory");
  }
  return send_ok_with(conn, 200, "links", links);
}

/* The id after "<prefix>/", or false when it is empty, too long, or has a slash in it. */
static bool path_id(const char *rest, char *out, size_t out_size) {
  size_t len = strlen(rest);
  if (len == 0 || len >= out_size || strchr(rest, '/') != NULL) {
    return false;
  }
  memcpy(out, rest, len + 1);
  return true;
}

/*
 * Single handler registered at kLinksPrefix ("/api/links"). civetweb routes both the exact prefix and any
 * "prefix/something" URI to a handler registered at prefix (see the identical note in corpus.c), so every links route
 * arrives here and is dispatched by local_uri.
 */
static int links_handler(struct mg_connection *conn, void *cbdata) {
  (void)cbdata;
  const struct mg_request_info *info = mg_get_request_info(conn);
  const char *uri = info->local_uri;
  const char *method = info->request_method;
  size_t prefix_len = strlen(kLinksPrefix);

  if (strncmp(uri, kLinksPrefix, prefix_len) != 0 || uri[prefix_len] != '/') {
    mg_send_http_error(conn, 404, "Not Found");
    return 404;
  }
  const char *rest = uri + prefix_len + 1;
  char id[256];

  if (strcmp(rest, "runs") == 0) {
    if (strcmp(method, "POST") == 0) {
      return handle_start_run(conn);
    }
    if (strcmp(method, "GET") == 0) {
      return handle_list_runs(conn);
    }
    mg_send_http_error(conn, 405, "Method Not Allowed");
    return 405;
  }

  if (strncmp(rest, "runs/", 5) == 0) {
    if (!path_id(rest + 5, id, sizeof(id)) || !workshop_collection_is_valid_id(id)) {
      return ursatz_gui_send_failure(conn, 404, "run not found");
    }
    if (strcmp(method, "GET") == 0) {
      return handle_get_run(conn, id);
    }
    if (strcmp(method, "DELETE") == 0) {
      return handle_delete_run(conn, id);
    }
    mg_send_http_error(conn, 405, "Method Not Allowed");
    return 405;
  }

  if (strncmp(rest, "pieces/", 7) == 0) {
    if (!path_id(rest + 7, id, sizeof(id)) || !workshop_collection_is_valid_id(id)) {
      return ursatz_gui_send_failure(conn, 404, "piece not found");
    }
    if (strcmp(method, "GET") == 0) {
      return handle_piece_links(conn, id);
    }
    mg_send_http_error(conn, 405, "Method Not Allowed");
    return 405;
  }

  mg_send_http_error(conn, 404, "Not Found");
  return 404;
}

void ursatz_gui_links_register(struct mg_context *ctx) {
  mg_set_request_handler(ctx, kLinksPrefix, links_handler, NULL);
}
