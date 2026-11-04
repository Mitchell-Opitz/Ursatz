#include "jobs.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <civetweb.h>
#include <cJSON.h>

#include "route_util.h"

static const char *kJobsPrefix = "/api/jobs";

static const char *reason_phrase(int status) {
  switch (status) {
    case 200:
      return "OK";
    case 202:
      return "Accepted";
    case 204:
      return "No Content";
    case 404:
      return "Not Found";
    case 405:
      return "Method Not Allowed";
    case 503:
      return "Service Unavailable";
    default:
      return "Internal Server Error";
  }
}

static int send_object(struct mg_connection *conn, int status, cJSON *object) {
  char *text = object ? cJSON_PrintUnformatted(object) : NULL;
  cJSON_Delete(object);
  if (!text) {
    return ursatz_gui_send_json_status(conn, 500, "Internal Server Error", "{\"ok\":false,\"error\":\"out of memory\"}");
  }
  int sent = ursatz_gui_send_json_status(conn, status, reason_phrase(status), text);
  free(text);
  return sent;
}

static int send_error(struct mg_connection *conn, int status, const char *error) {
  cJSON *object = cJSON_CreateObject();
  if (object) {
    cJSON_AddBoolToObject(object, "ok", 0);
    cJSON_AddStringToObject(object, "error", error);
  }
  return send_object(conn, status, object);
}

int ursatz_gui_jobs_send_accepted(struct mg_connection *conn, const char *kind, UrsatzGuiJobFn run, void *input,
                                  void (*free_input)(void *)) {
  char id[URSATZ_GUI_JOB_ID_BYTES];
  if (!ursatz_gui_job_submit(kind, run, input, free_input, id)) {
    return send_error(conn, 503, "the job queue is not running");
  }
  cJSON *object = cJSON_CreateObject();
  cJSON *job = object ? cJSON_AddObjectToObject(object, "job") : NULL;
  if (!job || !cJSON_AddBoolToObject(object, "ok", 1) || !cJSON_AddStringToObject(job, "id", id) ||
      !cJSON_AddStringToObject(job, "kind", kind) ||
      !cJSON_AddStringToObject(job, "state", ursatz_gui_job_state_name(URSATZ_GUI_JOB_QUEUED))) {
    cJSON_Delete(object);
    return send_error(conn, 500, "out of memory");
  }
  return send_object(conn, 202, object);
}

/* GET /api/jobs/<id>: {"ok": true, "job": {id, kind, state, stage, done, total, http_status?, result?}}. */
static int handle_get(struct mg_connection *conn, const char *id) {
  UrsatzGuiJobInfo info;
  if (!ursatz_gui_job_get(id, &info)) {
    return send_error(conn, 404, "job not found");
  }
  cJSON *object = cJSON_CreateObject();
  cJSON *job = object ? cJSON_AddObjectToObject(object, "job") : NULL;
  bool ok = job && cJSON_AddBoolToObject(object, "ok", 1) && cJSON_AddStringToObject(job, "id", info.id) &&
            cJSON_AddStringToObject(job, "kind", info.kind) &&
            cJSON_AddStringToObject(job, "state", ursatz_gui_job_state_name(info.state)) &&
            cJSON_AddStringToObject(job, "stage", info.stage) && cJSON_AddNumberToObject(job, "done", info.done) &&
            cJSON_AddNumberToObject(job, "total", info.total);
  if (ok && info.result) {
    cJSON *result = cJSON_Parse(info.result);
    ok = cJSON_AddNumberToObject(job, "http_status", info.http_status) && result && cJSON_AddItemToObject(job, "result", result);
    if (!ok) {
      cJSON_Delete(result);
    }
  }
  ursatz_gui_job_info_destroy(&info);
  if (!ok) {
    cJSON_Delete(object);
    return send_error(conn, 500, "out of memory");
  }
  return send_object(conn, 200, object);
}

/* DELETE /api/jobs/<id>: asks the job to stop (204). */
static int handle_delete(struct mg_connection *conn, const char *id) {
  if (!ursatz_gui_job_cancel(id)) {
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
      strchr(uri + prefix_len + 1, '/') != NULL || strlen(uri + prefix_len + 1) >= URSATZ_GUI_JOB_ID_BYTES) {
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
