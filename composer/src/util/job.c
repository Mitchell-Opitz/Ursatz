#include "util/job.h"

bool job_cancelled(const ComposerJob *job) {
    return job && job->cancelled && job->cancelled(job->user);
}

bool job_step(const ComposerJob *job, const char *stage, unsigned done, unsigned total) {
    if (!job) {
        return false;
    }
    if (job->progress) {
        job->progress(job->user, stage, done, total);
    }
    return job_cancelled(job);
}
