#ifndef URSATZ_GUI_ROUTE_STYLES_H
#define URSATZ_GUI_ROUTE_STYLES_H

struct mg_context;

/* Registers the Styles routes ("/api/styles", "/api/styles/<id>",
 * "/api/styles/<id>/rebuild") against ctx, following the
 * tool-registration convention described in ursatz_gui/tool.h. They only
 * dispatch into service/style_service.h, which owns the styles/ store. */
void ursatz_gui_styles_register(struct mg_context *ctx);

#endif /* URSATZ_GUI_ROUTE_STYLES_H */
