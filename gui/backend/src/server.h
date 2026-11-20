#ifndef URSATZ_GUI_SERVER_H
#define URSATZ_GUI_SERVER_H

#include <ursatz_workshop/workshop_api.h>

struct mg_context;

/* Starts the backend HTTP server on the data folder of an open workshop, registers all tool routes, and serves the static
 * frontend. The request threads and the job workers are the workshop's settings. Returns the civetweb context, or NULL on
 * failure. The caller keeps the workshop open until after ursatz_gui_server_stop and closes it. */
struct mg_context *ursatz_gui_server_start(Workshop *workshop, const char *listen_port, const char *frontend_dir);

void ursatz_gui_server_stop(struct mg_context *ctx);

/* The workshop the server was started with, NULL when it is not running. */
Workshop *ursatz_gui_workshop(void);

#endif /* URSATZ_GUI_SERVER_H */
