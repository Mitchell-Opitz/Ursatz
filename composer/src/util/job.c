#include "util/job.h"

bool job_cancelled(const UrsatzJob *job) {
    return ursatz_job_cancelled(job);
}

bool job_step(const UrsatzJob *job, const char *stage, unsigned done, unsigned total) {
    if (!job) {
        return false;
    }
    if (job->progress) {
        job->progress(job->user, stage, done, total);
    }
    return job_cancelled(job);
}
