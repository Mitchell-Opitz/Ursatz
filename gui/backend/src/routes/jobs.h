#ifndef URSATZ_GUI_ROUTE_JOBS_H
#define URSATZ_GUI_ROUTE_JOBS_H

#include "../service/job_queue.h"

struct mg_connection;
struct mg_context;

/* Registers GET and DELETE /api/jobs/<id> against ctx. */
void ursatz_gui_jobs_register(struct mg_context *ctx);

/* Queues a job and answers 202 {"ok": true, "job": {"id", "kind", "state"}}. Returns the status sent (the queue owns input;
 * it is freed on a failure too). */
int ursatz_gui_jobs_send_accepted(struct mg_connection *conn, const char *kind, UrsatzGuiJobFn run, void *input,
                                  void (*free_input)(void *));

#endif /* URSATZ_GUI_ROUTE_JOBS_H */
