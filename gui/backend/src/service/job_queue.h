#ifndef URSATZ_GUI_JOB_QUEUE_H
#define URSATZ_GUI_JOB_QUEUE_H

#include <stdbool.h>
#include <stddef.h>

#include "ursatz_composer/composer_api.h"

/*
 * The job queue (R-219, R-274): long Composer runs go to worker threads so no request thread waits on a compose. A job
 * is memory only (lost on restart). It reports progress from the Composer's ComposerJob callbacks and can be cancelled.
 * Finished jobs are kept for 10 minutes or the newest 64, whichever drops them first.
 */

#define URSATZ_GUI_JOB_ID_BYTES 32
#define URSATZ_GUI_JOB_KIND_BYTES 32
#define URSATZ_GUI_JOB_STAGE_BYTES 32

typedef enum {
  URSATZ_GUI_JOB_QUEUED = 0,
  URSATZ_GUI_JOB_RUNNING,
  URSATZ_GUI_JOB_DONE,      /* the run answered with a 2xx status */
  URSATZ_GUI_JOB_FAILED,    /* the run answered with any other status */
  URSATZ_GUI_JOB_CANCELLED  /* cancel was asked for and the run did not complete */
} UrsatzGuiJobState;

/* What a job run answers: the HTTP status the route would have sent and its JSON body (heap, moved to the queue). */
typedef struct {
  int http_status;
  char *json;
} UrsatzGuiJobResult;

/* Runs on a worker thread. input is what ursatz_gui_job_submit was given; job carries the progress and cancel
 * callbacks for the Composer entry (pass it on). Sets out->http_status and out->json. */
typedef void (*UrsatzGuiJobFn)(void *input, const ComposerJob *job, UrsatzGuiJobResult *out);

/* A copy of a job's state; result is the reply JSON (heap, free it with ursatz_gui_job_info_destroy) once finished. */
typedef struct {
  char id[URSATZ_GUI_JOB_ID_BYTES];
  char kind[URSATZ_GUI_JOB_KIND_BYTES];
  UrsatzGuiJobState state;
  char stage[URSATZ_GUI_JOB_STAGE_BYTES];
  unsigned done;
  unsigned total;
  int http_status;
  char *result;
} UrsatzGuiJobInfo;

/* Starts the workers: workers is 1 to 8, 0 for the default (URSATZ_GUI_JOB_WORKERS when set, else 2). A running queue is
 * left as it is. false when a thread could not be made. */
bool ursatz_gui_jobs_start(size_t workers);

/* Cancels every job, joins the workers and drops all jobs. Safe when not started. */
void ursatz_gui_jobs_stop(void);

/* Queues a job. The queue owns input and calls free_input(input) once the job is finished or dropped. id_out receives the
 * job id. false when the queue is not running or memory is out (input is then freed too). */
bool ursatz_gui_job_submit(const char *kind, UrsatzGuiJobFn run, void *input, void (*free_input)(void *),
                           char id_out[URSATZ_GUI_JOB_ID_BYTES]);

/* Copies the state of job id into info. false when there is no such job. */
bool ursatz_gui_job_get(const char *id, UrsatzGuiJobInfo *info);

/* Asks job id to stop: a queued job is cancelled at once, a running one at the Composer's next report. false when there is
 * no such job; true (and nothing changes) for a finished job. */
bool ursatz_gui_job_cancel(const char *id);

void ursatz_gui_job_info_destroy(UrsatzGuiJobInfo *info);

/* The state's name in the HTTP answers: queued, running, done, failed or cancelled. */
const char *ursatz_gui_job_state_name(UrsatzGuiJobState state);

#endif /* URSATZ_GUI_JOB_QUEUE_H */
