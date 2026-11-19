#include "jobs.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <civetweb.h>
#include <cJSON.h>

#include "route_util.h"
#include "../server.h"

static const char *kJobsPrefix = "/api/jobs";

/* A job id is shorter than this (the workshop checks it again). */
static const size_t kJobIdBytes = 32;

static int send_error(struct mg_connection *conn, int status, const char *error) {
  cJSON *object = cJSON_CreateObject();
  if (object) {
    cJSON_AddBoolToObject(object, "ok", 0);
    cJSON_AddStringToObject(object, "error", error);
  }
  return ursatz_gui_send_object(conn, status, object);
}

/* The failure of a job query: {"ok": false, "error"} with the status of the table. */
static int failed_job(struct mg_connection *conn, UrsatzStatus status, const UrsatzError *error) {
  return send_error(conn, ursatz_gui_http_status(status, NULL), error->message);
}

/* GET /api/jobs/<id>: {"ok": true, "job": {id, kind, state, stage, done, total, http_status?, result?}}. */
static int handle_get(struct mg_connection *conn, const char *id) {
  return ursatz_gui_answer_query(conn, "job.get", ursatz_gui_request_params(conn, "id", id, NULL), NULL, failed_job, false);
}

/* DELETE /api/jobs/<id>: asks the job to stop (204). */
static int handle_delete(struct mg_connection *conn, const char *id) {
  WorkshopReply reply;
  UrsatzError error = {{0}, {0}};
  cJSON *command = cJSON_CreateObject();
  if (!command || !cJSON_AddStringToObject(command, "command", "job.cancel") || !cJSON_AddStringToObject(command, "actor", "michelle") ||
      !cJSON_AddStringToObject(command, "target", id)) {
    cJSON_Delete(command);
    return send_error(conn, 500, "out of memory");
  }
  UrsatzStatus status = workshop_command(ursatz_gui_workshop(), command, NULL, &reply, &error);
  cJSON_Delete(command);
  workshop_reply_destroy(&reply);
  if (status != URSATZ_OK) {
    return send_error(conn, ursatz_gui_http_status(status, NULL), error.message);
  }
  mg_printf(conn, "HTTP/1.1 204 No Content\r\nConnection: close\r\n\r\n");
  return 204;
}

static int jobs_handler(struct mg_connection *conn, void *cbdata) {
  (void)cbdata;
  const struct mg_request_info *info = mg_get_request_info(conn);
  const char *uri = info->local_uri;
  size_t prefix_len = strlen(kJobsPrefix);
  if (strncmp(uri, kJobsPrefix, prefix_len) != 0 || uri[prefix_len] != '/' || uri[prefix_len + 1] == '\0' ||
      strchr(uri + prefix_len + 1, '/') != NULL || strlen(uri + prefix_len + 1) >= kJobIdBytes) {
    return send_error(conn, 404, "not found");
  }
  const char *id = uri + prefix_len + 1;
  if (strcmp(info->request_method, "GET") == 0) {
    return handle_get(conn, id);
  }
  if (strcmp(info->request_method, "DELETE") == 0) {
    return handle_delete(conn, id);
  }
  return send_error(conn, 405, "method not allowed");
}

void ursatz_gui_jobs_register(struct mg_context *ctx) {
  mg_set_request_handler(ctx, kJobsPrefix, jobs_handler, NULL);
}
