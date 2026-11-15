#ifndef URSATZ_GUI_ROUTE_HEALTH_H
#define URSATZ_GUI_ROUTE_HEALTH_H

struct mg_context;

/* Registers the health-check route ("/api/health/status") against ctx.
 * Reference implementation of the tool-registration convention described
 * in ursatz_gui/tool.h. */
void ursatz_gui_health_register(struct mg_context *ctx);

#endif /* URSATZ_GUI_ROUTE_HEALTH_H */
