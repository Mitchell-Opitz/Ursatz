#ifndef URSATZ_WORKSHOP_COORDINATOR_WORKSHOP_INTERNAL_H
#define URSATZ_WORKSHOP_COORDINATOR_WORKSHOP_INTERNAL_H

#include <ursatz_workshop/workshop_api.h>

/*
 * What the coordinator shares with the workshop's own files. The GUI includes it until B11 gives it the query and command API.
 */

struct Workshop {
    char data_dir[1024];
    int port;
    int http_threads;
    int job_workers;
};

#endif
