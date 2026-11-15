#ifndef URSATZ_GUI_ROUTE_ANALYZER_H
#define URSATZ_GUI_ROUTE_ANALYZER_H

struct mg_context;

/* Registers the Analyzer tool's routes ("/api/analyzer/pieces",
 * "/api/analyzer/pieces/<id>") against ctx, following the
 * tool-registration convention described in ursatz_gui/tool.h. */
void ursatz_gui_analyzer_register(struct mg_context *ctx);

#endif /* URSATZ_GUI_ROUTE_ANALYZER_H */
