#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "coordinator/dispatch.h"

/* The job queries (R-645, R-646): what a long command is doing. */

/* {id, kind, state, stage, done, total, http_status?, result?} for info; the result moves into the object. */
static cJSON *job_object(WorkshopJobInfo *info) {
    cJSON *job = cJSON_CreateObject();
    bool ok = job && cJSON_AddStringToObject(job, "id", info->id) && cJSON_AddStringToObject(job, "kind", info->kind) &&
              cJSON_AddStringToObject(job, "state", workshop_job_state_name(info->state)) &&
              cJSON_AddStringToObject(job, "stage", info->stage) && cJSON_AddNumberToObject(job, "done", info->done) &&
              cJSON_AddNumberToObject(job, "total", info->total);
    if (ok && info->result) {
        ok = cJSON_AddNumberToObject(job, "http_status", info->http_status) && cJSON_AddItemToObject(job, "result", info->result);
        if (ok) {
            info->result = NULL; /* the answer owns the tree now */
        }
    }
    if (!ok) {
        cJSON_Delete(job);
        return NULL;
    }
    return job;
}

/* {"job": {...}} for params id. */
WORKSHOP_HANDLER(workshop_q_job_get) {
    const char *id = NULL;
    if (!workshop_param_require_string(call->params, "id", &id, error)) {
        return URSATZ_ERR_REQUEST;
    }
    WorkshopJobInfo info;
    if (strlen(id) >= WORKSHOP_JOB_ID_BYTES || !workshop_job_get(id, &info)) {
        return workshop_call_fail(error, URSATZ_ERR_NOT_FOUND, "", "job not found");
    }
    cJSON *job = job_object(&info);
    workshop_job_info_destroy(&info);
    return job ? workshop_reply_member(reply, "job", job, error) : workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
}

/* {"jobs": [...]}: the queued and running jobs, newest first. */
WORKSHOP_HANDLER(workshop_q_job_list) {
    (void)call;
    WorkshopJobInfo *infos = NULL;
    size_t count = 0;
    if (!workshop_jobs_list(&infos, &count)) {
        return workshop_call_fail(error, URSATZ_ERR_INTERNAL, "", "the job queue is not running");
    }
    cJSON *jobs = cJSON_CreateArray();
    bool built = jobs != NULL;
    for (size_t i = 0; built && i < count; i++) {
        cJSON *job = job_object(&infos[i]);
        built = job != NULL;
        if (built) {
            cJSON_AddItemToArray(jobs, job);
        }
    }
    workshop_job_list_destroy(infos, count);
    if (!built) {
        cJSON_Delete(jobs);
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    return workshop_reply_member(reply, "jobs", jobs, error);
}
