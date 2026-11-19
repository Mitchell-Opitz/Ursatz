#include "analysis_jobs.h"

#include <stdlib.h>
#include <string.h>

#include <common/strings.h>

#include "analysis.h"
#include "base/http_status.h"
#include "base/last_error.h"
#include "items/layout.h"

struct WorkshopAnalysisJob {
  WorkshopAnalysisJobOp op;
  unsigned char *data; /* import */
  size_t size;
  char *name;          /* import: the file name */
  char *id;            /* reanalyze, rebuild */
  WorkshopStyleRequest request; /* style build */
  cJSON *body;                  /* what the request's strings point into */
};

static WorkshopAnalysisJob *job_new(WorkshopAnalysisJobOp op) {
  WorkshopAnalysisJob *job = calloc(1, sizeof(*job));
  if (job) {
    job->op = op;
  }
  return job;
}

WorkshopAnalysisJob *workshop_analysis_job_import(const unsigned char *data, size_t size, const char *filename) {
  WorkshopAnalysisJob *job = job_new(WORKSHOP_ANALYSIS_JOB_IMPORT);
  if (!job) {
    return NULL;
  }
  job->data = malloc(size > 0 ? size : 1);
  job->name = common_strdup(filename ? filename : "");
  if (!job->data || !job->name) {
    workshop_analysis_job_free(job);
    return NULL;
  }
  if (size > 0) {
    memcpy(job->data, data, size);
  }
  job->size = size;
  return job;
}

static WorkshopAnalysisJob *job_with_id(WorkshopAnalysisJobOp op, const char *id) {
  WorkshopAnalysisJob *job = job_new(op);
  if (job && !(job->id = common_strdup(id))) {
    workshop_analysis_job_free(job);
    return NULL;
  }
  return job;
}

WorkshopAnalysisJob *workshop_analysis_job_reanalyze(const char *id) {
  return job_with_id(WORKSHOP_ANALYSIS_JOB_REANALYZE, id);
}

WorkshopAnalysisJob *workshop_analysis_job_style_rebuild(const char *id) {
  return job_with_id(WORKSHOP_ANALYSIS_JOB_STYLE_REBUILD, id);
}

WorkshopAnalysisJob *workshop_analysis_job_style_build(WorkshopStyleRequest *request, cJSON *body) {
  WorkshopAnalysisJob *job = job_new(WORKSHOP_ANALYSIS_JOB_STYLE_BUILD);
  if (!job) {
    return NULL; /* the caller keeps request and body */
  }
  job->request = *request;
  if (job->request.source.brief == &request->brief) {
    job->request.source.brief = &job->request.brief; /* the brief moved with the request */
  }
  job->body = body;
  memset(request, 0, sizeof(*request));
  return job;
}

const char *workshop_analysis_job_kind(const WorkshopAnalysisJob *job) {
  return job->op == WORKSHOP_ANALYSIS_JOB_STYLE_BUILD || job->op == WORKSHOP_ANALYSIS_JOB_STYLE_REBUILD ? "style" : "analysis";
}

void workshop_analysis_job_free(void *input) {
  WorkshopAnalysisJob *job = input;
  if (!job) {
    return;
  }
  if (job->op == WORKSHOP_ANALYSIS_JOB_STYLE_BUILD) {
    workshop_style_request_free(&job->request);
    cJSON_Delete(job->body);
  }
  free(job->data);
  free(job->name);
  free(job->id);
  free(job);
}

/* --- answers -------------------------------------------------------------- */

/* The failure of a route's own shape: {"ok": false, "message": text} at status. */
static void fail_message(WorkshopJobResult *out, int status, const char *text) {
  cJSON *object = cJSON_CreateObject();
  if (object) {
    cJSON_AddBoolToObject(object, "ok", 0);
    cJSON_AddStringToObject(object, "message", text);
  }
  workshop_job_result_json(out, status, object);
}

/* {"error": text}: the import routes' answer for a file they refuse. */
static void fail_error(WorkshopJobResult *out, int status, const char *text) {
  cJSON *object = cJSON_CreateObject();
  if (object) {
    cJSON_AddStringToObject(object, "error", text);
  }
  workshop_job_result_json(out, status, object);
}

/* A service failure as ursatz_gui_send_service_failure answered it: the status of the reason, the service's own message or the fallback. */
static void fail_service(WorkshopJobResult *out, UrsatzStatus status, const char *fallback) {
  const UrsatzError *error = workshop_last_error();
  fail_message(out, workshop_http_status(status, error), error->message[0] != '\0' ? error->message : fallback);
}

static void answer_cancelled(WorkshopJobResult *out) {
  workshop_job_result_failure(out, 499, "Cancelled");
}

static UrsatzStatus run_import(WorkshopAnalysisJob *job, WorkshopJobResult *out) {
  char *id = NULL;
  UrsatzStatus result = workshop_analysis_import_and_save(WORKSHOP_DIR_PIECES, job->data, job->size, job->name, &id);
  if (result == URSATZ_ERR_REQUEST) {
    fail_error(out, 415, "unsupported format");
  } else if (result == URSATZ_ERR_FORMAT) {
    fail_error(out, 422, "import failed");
  } else if (result != URSATZ_OK) {
    fail_message(out, 500, "failed to save piece");
  } else {
    cJSON *answer = cJSON_CreateObject();
    bool built = answer && cJSON_AddBoolToObject(answer, "ok", 1) && cJSON_AddStringToObject(answer, "id", id) &&
                 cJSON_AddStringToObject(answer, "filename", job->name);
    if (!built) {
      cJSON_Delete(answer);
      answer = NULL;
    }
    workshop_job_result_json(out, 200, answer);
  }
  free(id);
  return result;
}

static UrsatzStatus run_reanalyze(WorkshopAnalysisJob *job, WorkshopJobResult *out) {
  UrsatzStatus result = workshop_analysis_reanalyze(WORKSHOP_DIR_PIECES, job->id);
  if (result == URSATZ_ERR_NOT_FOUND) {
    fail_message(out, 404, "piece not found");
  } else if (result == URSATZ_ERR_FORMAT) {
    fail_error(out, 422, "import failed");
  } else if (result != URSATZ_OK) {
    fail_message(out, 500, "failed to analyze piece again");
  } else {
    cJSON *answer = cJSON_CreateObject();
    bool built = answer && cJSON_AddBoolToObject(answer, "ok", 1) && cJSON_AddStringToObject(answer, "id", job->id);
    if (!built) {
      cJSON_Delete(answer);
      answer = NULL;
    }
    workshop_job_result_json(out, 200, answer);
  }
  return result;
}

/* {"ok": true, "style": {list entry}, "skipped": [...]} for the stored style id. */
static void answer_style(const char *id, WorkshopJobResult *out) {
  cJSON *entry = NULL;
  cJSON *skipped = NULL;
  UrsatzStatus result = workshop_style_get_entry(id, &entry, &skipped);
  if (result != URSATZ_OK) {
    fail_service(out, result, "style operation failed");
    return;
  }
  cJSON *object = cJSON_CreateObject();
  if (!object || !cJSON_AddBoolToObject(object, "ok", 1) || !cJSON_AddItemToObject(object, "style", entry)) {
    cJSON_Delete(entry);
    cJSON_Delete(skipped);
    cJSON_Delete(object);
    workshop_job_result_failure(out, 500, "out of memory");
    return;
  }
  if (!cJSON_AddItemToObject(object, "skipped", skipped)) {
    cJSON_Delete(skipped);
    cJSON_Delete(object);
    workshop_job_result_failure(out, 500, "out of memory");
    return;
  }
  workshop_job_result_json(out, 200, object);
}

static UrsatzStatus run_style_build(WorkshopAnalysisJob *job, const UrsatzJob *handle, WorkshopJobResult *out) {
  char *id = NULL;
  UrsatzStatus result = workshop_style_build(job->request.name, &job->request.source, handle, &id, NULL, NULL);
  WorkshopStyleSourceKind kind = job->request.source.kind;
  if (result == URSATZ_OK) {
    answer_style(id, out);
  } else if (result == URSATZ_ERR_CANCELLED) {
    answer_cancelled(out);
  } else if (result == URSATZ_ERR_NOT_FOUND && kind == WORKSHOP_STYLE_SOURCE_COLLECTION) {
    fail_message(out, 404, "collection not found");
  } else if (result == URSATZ_ERR_NOT_FOUND && kind == WORKSHOP_STYLE_SOURCE_BRIEF) {
    fail_message(out, 404, "brief source not found");
  } else {
    fail_service(out, result, "style operation failed");
  }
  free(id);
  return result;
}

static UrsatzStatus run_style_rebuild(WorkshopAnalysisJob *job, const UrsatzJob *handle, WorkshopJobResult *out) {
  UrsatzStatus result = workshop_style_rebuild(job->id, handle);
  if (result == URSATZ_OK) {
    answer_style(job->id, out);
  } else if (result == URSATZ_ERR_CANCELLED) {
    answer_cancelled(out);
  } else {
    fail_service(out, result, "style operation failed");
  }
  return result;
}

UrsatzStatus workshop_analysis_job_run(void *input, const UrsatzJob *handle, WorkshopJobResult *out) {
  WorkshopAnalysisJob *job = input;
  switch (job->op) {
    case WORKSHOP_ANALYSIS_JOB_IMPORT:
      return run_import(job, out);
    case WORKSHOP_ANALYSIS_JOB_REANALYZE:
      return run_reanalyze(job, out);
    case WORKSHOP_ANALYSIS_JOB_STYLE_BUILD:
      return run_style_build(job, handle, out);
    case WORKSHOP_ANALYSIS_JOB_STYLE_REBUILD:
      return run_style_rebuild(job, handle, out);
  }
  return URSATZ_ERR_INTERNAL;
}
