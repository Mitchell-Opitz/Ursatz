#ifndef URSATZ_GUI_SERVER_H
#define URSATZ_GUI_SERVER_H

struct mg_context;

/* Starts the backend HTTP server, registers all tool routes, and serves
 * the static frontend. Returns the civetweb context, or NULL on failure. */
struct mg_context *ursatz_gui_server_start(const char *listen_port,
                                            const char *frontend_dir);

void ursatz_gui_server_stop(struct mg_context *ctx);

#endif /* URSATZ_GUI_SERVER_H */
