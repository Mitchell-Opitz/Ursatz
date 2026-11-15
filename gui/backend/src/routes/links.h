#ifndef URSATZ_GUI_ROUTE_LINKS_H
#define URSATZ_GUI_ROUTE_LINKS_H

struct mg_context;

/* Registers the motive link routes ("/api/links/runs", "/api/links/runs/<id>", "/api/links/pieces/<piece id>")
 * against ctx, following the tool-registration convention described in ursatz_gui/tool.h. They only dispatch into
 * service/link_service.h, which owns the links/ store. */
void ursatz_gui_links_register(struct mg_context *ctx);

#endif /* URSATZ_GUI_ROUTE_LINKS_H */
