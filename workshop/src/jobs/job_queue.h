#ifndef WORKSHOP_JOB_QUEUE_H
#define WORKSHOP_JOB_QUEUE_H

#include <stdbool.h>
#include <stddef.h>

#include "ursatz_composer/composer_api.h"

/*
 * The job queue (R-219, R-274): long Composer runs go to worker threads so no request thread waits on a compose. A job
 * is memory only (lost on restart). It reports progress from the Composer's UrsatzJob callbacks and can be cancelled.
 * Finished jobs are kept for 10 minutes or the newest 64, whichever drops them first.
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

/* What a job run answers: the HTTP status the route would have sent and its JSON body (heap, moved to the queue). */
typedef struct {
  int http_status;
  char *json;
} WorkshopJobResult;

/* Runs on a worker thread. input is what workshop_job_submit was given; job carries the progress and cancel
 * callbacks for the Composer entry (pass it on). Sets out->http_status and out->json. */
typedef void (*UrsatzGuiJobFn)(void *input, const UrsatzJob *job, WorkshopJobResult *out);

/* A copy of a job's state; result is the reply JSON (heap, free it with workshop_job_info_destroy) once finished. */
typedef struct {
  char id[WORKSHOP_JOB_ID_BYTES];
  char kind[WORKSHOP_JOB_KIND_BYTES];
  WorkshopJobState state;
  char stage[WORKSHOP_JOB_STAGE_BYTES];
  unsigned done;
  unsigned total;
  int http_status;
  char *result;
} WorkshopJobInfo;

/* Starts the workers: workers is 1 to 8 (the workshop's job_workers setting, URSATZ_JOB_WORKERS), 0 for the default of 2. A running queue is
 * left as it is. false when a thread could not be made. */
bool workshop_jobs_start(size_t workers);

/* Cancels every job, joins the workers and drops all jobs. Safe when not started. */
void workshop_jobs_stop(void);

/* Queues a job. The queue owns input and calls free_input(input) once the job is finished or dropped. id_out receives the
 * job id. false when the queue is not running or memory is out (input is then freed too). */
bool workshop_job_submit(const char *kind, UrsatzGuiJobFn run, void *input, void (*free_input)(void *),
                           char id_out[WORKSHOP_JOB_ID_BYTES]);

/* Copies the state of job id into info. false when there is no such job. */
bool workshop_job_get(const char *id, WorkshopJobInfo *info);

/* Asks job id to stop: a queued job is cancelled at once, a running one at the Composer's next report. false when there is
 * no such job; true (and nothing changes) for a finished job. */
bool workshop_job_cancel(const char *id);

void workshop_job_info_destroy(WorkshopJobInfo *info);

/* The state's name in the HTTP answers: queued, running, done, failed or cancelled. */
const char *workshop_job_state_name(WorkshopJobState state);

#endif /* WORKSHOP_JOB_QUEUE_H */
