#ifndef WORKSHOP_JOB_QUEUE_H
#define WORKSHOP_JOB_QUEUE_H

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>

#include <common/job.h>
#include <common/status.h>

/*
 * The job queue (R-219, R-274, R-645): all long work (composes, variants, motive link runs, style builds, imports and re-analyses) goes to worker
 * threads so no request thread waits on it. A job is memory only (lost on restart); what it changes is committed through the journal by
 * the job's own transactions. It reports progress through the common job handle (<common/job.h>) and can be cancelled. A kind can have a
 * limit on how many of its jobs run at once ("links" and "style" one each: a second one waits in the queue). Finished jobs are kept for 10
 * minutes or the newest 64, whichever drops them first.
 */

#define WORKSHOP_JOB_ID_BYTES 32
#define WORKSHOP_JOB_KIND_BYTES 32
#define WORKSHOP_JOB_STAGE_BYTES 32

typedef enum {
  WORKSHOP_JOB_QUEUED = 0,
  WORKSHOP_JOB_RUNNING,
  WORKSHOP_JOB_DONE,      /* the run answered with a 2xx status */
  WORKSHOP_JOB_FAILED,    /* the run answered with any other status */
  WORKSHOP_JOB_CANCELLED  /* cancel was asked for and the run did not complete */
} WorkshopJobState;

/* What a job run answers: the HTTP status the route would have sent and its JSON body as a tree (moved to the queue, which keeps it as one until
 * the job is dropped; a poll of a finished job copies it into the answer). */
typedef struct {
  int http_status;
  cJSON *tree;
} WorkshopJobResult;

/* Runs on a worker thread. input is what workshop_job_submit was given; job is the common job handle (progress and cancel: pass it on). Sets
 * out->http_status and out->tree and returns the status of the run: URSATZ_ERR_CANCELLED for a run that stopped because it was asked to. */
typedef UrsatzStatus (*WorkshopJobFn)(void *input, const UrsatzJob *job, WorkshopJobResult *out);

/* A copy of a job's state; result is the reply tree (free it with workshop_job_info_destroy) once finished. */
typedef struct {
  char id[WORKSHOP_JOB_ID_BYTES];
  char kind[WORKSHOP_JOB_KIND_BYTES];
  WorkshopJobState state;
  char stage[WORKSHOP_JOB_STAGE_BYTES];
  unsigned done;
  unsigned total;
  int http_status;
  cJSON *result;
} WorkshopJobInfo;

/* Starts the workers: workers is 1 to 8 (the workshop's job_workers setting, URSATZ_JOB_WORKERS), 0 for the default of 2. A running queue is
 * left as it is. false when a thread could not be made. */
bool workshop_jobs_start(size_t workers);

/* Cancels every job, joins the workers and drops all jobs. Safe when not started. */
void workshop_jobs_stop(void);

/* Queues a job. The queue owns input and calls free_input(input) once the job is finished or dropped. id_out receives the
 * job id. false when the queue is not running or memory is out (input is then freed too). */
bool workshop_job_submit(const char *kind, WorkshopJobFn run, void *input, void (*free_input)(void *),
                           char id_out[WORKSHOP_JOB_ID_BYTES]);

/* workshop_job_submit for a job about something: `subject` (up to 63 characters, "" or NULL for none) names it, so a caller can find the job by
 * what it is about (a link run's id) instead of keeping a list of its own. */
bool workshop_job_submit_subject(const char *kind, const char *subject, WorkshopJobFn run, void *input, void (*free_input)(void *),
                                 char id_out[WORKSHOP_JOB_ID_BYTES]);

/* The id of the queued or running job whose subject is `subject`; false when there is none (a finished job is not found). */
bool workshop_job_find(const char *subject, char *job_id, size_t size);

/* Waits until job id is neither queued nor running (finished, cancelled, dropped or unknown), on the queue's own wake-up: no polling. */
void workshop_job_wait(const char *job_id);

/* Copies the state of job id into info. false when there is no such job. */
bool workshop_job_get(const char *id, WorkshopJobInfo *info);

/* Asks job id to stop: a queued job is cancelled at once, a running one at the Composer's next report. false when there is
 * no such job; true (and nothing changes) for a finished job. */
bool workshop_job_cancel(const char *id);

void workshop_job_info_destroy(WorkshopJobInfo *info);

/* How many jobs of kind may run at once, 0 for no limit besides the worker count. "links" and "style" start with 1. */
void workshop_jobs_set_limit(const char *kind, unsigned limit);

/* The queued and running jobs, newest first: *out is an array of *count infos (no result: they are not finished), free it with
 * workshop_job_list_destroy. */
bool workshop_jobs_list(WorkshopJobInfo **out, size_t *count);
void workshop_job_list_destroy(WorkshopJobInfo *infos, size_t count);

/* Fills out with object (freed) as the answer with the given status, or the out-of-memory failure. */
void workshop_job_result_json(WorkshopJobResult *out, int status, cJSON *object);

/* Fills out with {"ok": false, "error": message} and the status. */
void workshop_job_result_failure(WorkshopJobResult *out, int status, const char *message);

/* The same with the request member at fault as "path" (the Composer's UrsatzError.path); no path member when it is NULL
 * or empty. */
void workshop_job_result_failure_at(WorkshopJobResult *out, int status, const char *message, const char *path);

/* The state's name in the HTTP answers: queued, running, done, failed or cancelled. */
const char *workshop_job_state_name(WorkshopJobState state);

#endif /* WORKSHOP_JOB_QUEUE_H */
