#include "jobs.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <civetweb.h>
#include <cJSON.h>

#include "route_util.h"

static const char *kJobsPrefix = "/api/jobs";

static int send_error(struct mg_connection *conn, int status, const char *error) {
  cJSON *object = cJSON_CreateObject();
  if (object) {
    cJSON_AddBoolToObject(object, "ok", 0);
    cJSON_AddStringToObject(object, "error", error);
  }
  return ursatz_gui_send_object(conn, status, object);
}

int ursatz_gui_jobs_send_accepted(struct mg_connection *conn, const char *kind, WorkshopJobFn run, void *input,
                                  void (*free_input)(void *)) {
  char id[WORKSHOP_JOB_ID_BYTES];
  if (!workshop_job_submit(kind, run, input, free_input, id)) {
    return send_error(conn, 503, "the job queue is not running");
  }
  cJSON *object = cJSON_CreateObject();
  cJSON *job = object ? cJSON_AddObjectToObject(object, "job") : NULL;
  if (!job || !cJSON_AddBoolToObject(object, "ok", 1) || !cJSON_AddStringToObject(job, "id", id) ||
      !cJSON_AddStringToObject(job, "kind", kind) ||
      !cJSON_AddStringToObject(job, "state", workshop_job_state_name(WORKSHOP_JOB_QUEUED))) {
    cJSON_Delete(object);
    return send_error(conn, 500, "out of memory");
  }
  return ursatz_gui_send_object(conn, 202, object);
}

/* The failure of a job query: {"ok": false, "error"} with the status of the table. */
static int failed_job(struct mg_connection *conn, UrsatzStatus status, const UrsatzError *error) {
  return send_error(conn, workshop_http_status(status, NULL), error->message);
}

/* GET /api/jobs/<id>: {"ok": true, "job": {id, kind, state, stage, done, total, http_status?, result?}}. */
static int handle_get(struct mg_connection *conn, const char *id) {
  return ursatz_gui_answer_query(conn, "job.get", ursatz_gui_request_params(conn, "id", id, NULL), NULL, failed_job, false);
}

/* DELETE /api/jobs/<id>: asks the job to stop (204). */
static int handle_delete(struct mg_connection *conn, const char *id) {
  if (!workshop_job_cancel(id)) {
    return send_error(conn, 404, "job not found");
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
      strchr(uri + prefix_len + 1, '/') != NULL || strlen(uri + prefix_len + 1) >= WORKSHOP_JOB_ID_BYTES) {
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
