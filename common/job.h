#ifndef URSATZ_COMMON_JOB_H
#define URSATZ_COMMON_JOB_H

#include <stdbool.h>
#include <stddef.h>

/*
 * The progress and cancel handle of a long call (R-645): the caller owns user and both functions, any of them may be NULL, and a
 * function may be called from the worker thread that runs the call.
 */
typedef struct {
    void (*progress)(void *user, const char *stage, unsigned done, unsigned total);
    bool (*cancelled)(void *user);
    void *user;
} UrsatzJob;

/* True when the job asks to stop: false for a NULL job or a NULL cancelled member. */
static inline bool ursatz_job_cancelled(const UrsatzJob *job) {
    return job && job->cancelled && job->cancelled(job->user);
}

#endif
