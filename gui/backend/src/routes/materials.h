#ifndef URSATZ_GUI_ROUTE_MATERIALS_H
#define URSATZ_GUI_ROUTE_MATERIALS_H

#include <cJSON.h>

struct mg_context;

/* Registers the material routes ("/api/materials/...") against ctx, following the tool-registration convention in
 * docs/architecture.md (Layers). The motif routes are in motifs.c and the subject routes (subjects.h) have their own handler. */
void ursatz_gui_materials_register(struct mg_context *ctx);

#endif /* URSATZ_GUI_ROUTE_MATERIALS_H */
