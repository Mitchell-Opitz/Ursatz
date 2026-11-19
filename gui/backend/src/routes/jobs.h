#ifndef URSATZ_GUI_ROUTE_JOBS_H
#define URSATZ_GUI_ROUTE_JOBS_H


struct mg_connection;
struct mg_context;

/* Registers GET and DELETE /api/jobs/<id> against ctx. */
void ursatz_gui_jobs_register(struct mg_context *ctx);

#endif /* URSATZ_GUI_ROUTE_JOBS_H */
