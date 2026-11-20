#ifndef URSATZ_COMPOSER_UTIL_JOB_H
#define URSATZ_COMPOSER_UTIL_JOB_H

#include <stdbool.h>

#include <ursatz_composer/composer_api.h>

/*
 * The engine-level job: step, progress and cancel for the long-running runs. The engine runs (fill, compose, grow) and the
 * API files use only these two functions; the API's UrsatzJob is the type they read. A NULL job never reports or cancels.
 */

/* Reports progress when the job has a callback; returns true when the job is cancelled. */
bool job_step(const UrsatzJob *job, const char *stage, unsigned done, unsigned total);

/* True when the job has a cancel callback and it returns true. */
bool job_cancelled(const UrsatzJob *job);

#endif
