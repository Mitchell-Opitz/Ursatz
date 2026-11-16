#ifndef URSATZ_GUI_ROUTE_ANALYZER_H
#define URSATZ_GUI_ROUTE_ANALYZER_H

struct mg_context;

/* Registers the Analyzer tool's routes ("/api/analyzer/pieces",
 * "/api/analyzer/pieces/<id>") against ctx, following the
 * tool-registration convention in docs/architecture.md (Layers). */
void ursatz_gui_analyzer_register(struct mg_context *ctx);

#endif /* URSATZ_GUI_ROUTE_ANALYZER_H */
