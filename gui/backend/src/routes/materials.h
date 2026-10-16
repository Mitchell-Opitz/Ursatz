#ifndef URSATZ_GUI_ROUTE_MATERIALS_H
#define URSATZ_GUI_ROUTE_MATERIALS_H

#include <cJSON.h>

struct mg_context;

/* Registers the material and subject routes ("/api/materials/..." and
 * "/api/subjects/...") against ctx, following the tool-registration
 * convention described in ursatz_gui/tool.h. They only dispatch into
 * service/material_service.h and service/subject_service.h, which own the
 * materials/ and subjects/ stores. */
void ursatz_gui_materials_register(struct mg_context *ctx);

/*
 * One-time re-analysis for tone labels (R-204): every full motif whose current version's analysis has no tones is
 * analyzed again and saved as a new version with the note "Re-analyzed" (the Re-analyze path). A motif the Composer
 * refuses or cannot analyze keeps its analysis_error and does not stop the others. A marker file `tones-migrated` in
 * materials_dir stops later runs; without a Composer nothing is changed and no marker is written. Returns the number of
 * motifs re-analyzed. Call after ursatz_gui_material_service_init.
 */
int ursatz_gui_materials_migrate_tones(const char *materials_dir);

/*
 * One-time clear of the themes for the v2 body (R-205): every theme item is deleted, referenced or not (a piece that used
 * one then shows its source missing, R-131). A marker file `themes-cleared` in materials_dir stops later runs. A failure
 * is logged and does not stop the start; no marker is written then, so the next start tries again. Returns the number of
 * themes deleted. Call after ursatz_gui_material_service_init.
 */
int ursatz_gui_materials_clear_themes(const char *materials_dir);

/*
 * The subjects that link a material item (kind "items") or a Library piece
 * (kind "pieces"): a caller-owned array of {id, name, kind} in name order,
 * empty when none link it. NULL on allocation or store failure.
 */
cJSON *ursatz_gui_materials_subjects_for(const char *kind, const char *id);

#endif /* URSATZ_GUI_ROUTE_MATERIALS_H */
