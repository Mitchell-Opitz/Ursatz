#ifndef WORKSHOP_ANALYSIS_JOBS_H
#define WORKSHOP_ANALYSIS_JOBS_H

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>

#include <common/status.h>

#include "jobs/job_queue.h"
#include "styles.h"

/*
 * The long work of the library as jobs on the queue (R-645): a piece import and a re-analysis are "analysis" jobs, a style build and a
 * rebuild are "style" jobs (one at a time: a second waits). A job answers what its route answered before jobs: 200 with the body, or the
 * failure with the status of its reason. A style job reports "pieces d/t" and stops between pieces when cancelled, writing nothing.
 */

typedef enum {
  WORKSHOP_ANALYSIS_JOB_IMPORT,
  WORKSHOP_ANALYSIS_JOB_REANALYZE,
  WORKSHOP_ANALYSIS_JOB_STYLE_BUILD,
  WORKSHOP_ANALYSIS_JOB_STYLE_REBUILD
} WorkshopAnalysisJobOp;

typedef struct WorkshopAnalysisJob WorkshopAnalysisJob;

/* An import of the file data (copied) named filename (copied; "" for none): answers {"ok": true, "id", "filename"}, 415 for a format the
 * Analyzer does not take and 422 for a file it cannot read. NULL when memory is out. */
WorkshopAnalysisJob *workshop_analysis_job_import(const unsigned char *data, size_t size, const char *filename);

/* The Analyzer again on piece id's stored source (copied): answers {"ok": true, "id"}, 404 for no such piece, 422 for a file it cannot read. */
WorkshopAnalysisJob *workshop_analysis_job_reanalyze(const char *id);

/* A style build from request, which the job takes over together with body (the request's strings point into the body; both are freed with
 * the job): answers {"ok": true, "style": {list entry}, "skipped": [...]}. */
WorkshopAnalysisJob *workshop_analysis_job_style_build(WorkshopStyleRequest *request, cJSON *body);

/* The style id (copied) built again from its stored source: answers as a build. */
WorkshopAnalysisJob *workshop_analysis_job_style_rebuild(const char *id);

/* The queue kind of the job: "analysis" or "style". */
const char *workshop_analysis_job_kind(const WorkshopAnalysisJob *job);

/* The queue's functions for a job: workshop_job_submit(kind, workshop_analysis_job_run, job, workshop_analysis_job_free, id). */
UrsatzStatus workshop_analysis_job_run(void *input, const UrsatzJob *handle, WorkshopJobResult *out);
void workshop_analysis_job_free(void *input);

#endif /* WORKSHOP_ANALYSIS_JOBS_H */
