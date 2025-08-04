#ifndef URSATZ_GUI_ROUTE_MATERIALS_H
#define URSATZ_GUI_ROUTE_MATERIALS_H

struct mg_context;

/* Registers the material and subject routes ("/api/materials/..." and
 * "/api/subjects/...") against ctx, following the tool-registration
 * convention described in ursatz_gui/tool.h. They only dispatch into
 * service/material_service.h and service/subject_service.h, which own the
 * materials/ and subjects/ stores. */
void ursatz_gui_materials_register(struct mg_context *ctx);

#endif /* URSATZ_GUI_ROUTE_MATERIALS_H */
