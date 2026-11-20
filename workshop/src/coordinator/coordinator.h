#ifndef URSATZ_WORKSHOP_COORDINATOR_COORDINATOR_H
#define URSATZ_WORKSHOP_COORDINATOR_COORDINATOR_H

#include <stdbool.h>

#include <ursatz_workshop/workshop_api.h>

#include "base/knowledge.h"

/*
 * What the coordinator's own files share: the Workshop and the data folder lock.
 */

struct Workshop {
    char data_dir[1024];
    int port;
    int http_threads;
    int job_workers;
    int lock_descriptor; /* the data folder's lock, held until close */
    char recovery[512];  /* what the open recovered from the journal, "" when nothing (workshop_recovery_summary) */
    bool started;        /* workshop_start ran and workshop_stop has not */
    bool tools_only;     /* workshop_tools_open: no data folder, no lock, no services; answers tools only (R-656) */
};

/*
 * Locks the data folder (data_lock.c): flock(LOCK_EX | LOCK_NB) on "<data>/.ursatz.lock", which then holds this process's pid. A folder
 * another workshop holds, in this or another process, is URSATZ_ERR_CONFLICT naming the pid. *descriptor is the lock to give
 * back to workshop_data_lock_release, -1 on failure.
 */
UrsatzStatus workshop_data_lock_acquire(const char *data_dir, int *descriptor, UrsatzError *error);
void workshop_data_lock_release(int descriptor);

/* Rewrites the lock file this descriptor holds as "<pid> <port>\n" (workshop_announce). */
UrsatzStatus workshop_data_lock_announce(int descriptor, int port, UrsatzError *error);

#endif
