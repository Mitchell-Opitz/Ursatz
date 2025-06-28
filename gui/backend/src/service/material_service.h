#ifndef URSATZ_GUI_MATERIAL_SERVICE_H
#define URSATZ_GUI_MATERIAL_SERVICE_H

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>

/*
 * Material service: projects that group motifs, themes and pieces, owned by
 * the GUI (R-115 to R-119, R-129, R-130).
 *
 * Layout under the store directory (default "materials"):
 *   projects/<prj-id>.json   {id, name, description, created_at, updated_at}
 *
 * Every file is written to "<file>.tmp" and renamed over the old one, so a
 * reader never sees half a file. civetweb serves requests on several
 * threads, so every operation here runs under one mutex.
 *
 * Projects and items are cJSON objects. Every function that returns a
 * cJSON hands the caller ownership: free it with cJSON_Delete. A cJSON the
 * caller passes in is only read, never freed or kept.
 */

typedef enum {
    URSATZ_GUI_MATERIAL_OK = 0,
    URSATZ_GUI_MATERIAL_ERR_INVALID_ID,
    URSATZ_GUI_MATERIAL_ERR_INVALID_NAME,
    URSATZ_GUI_MATERIAL_ERR_INVALID_TYPE,
    URSATZ_GUI_MATERIAL_ERR_INVALID_BODY,
    URSATZ_GUI_MATERIAL_ERR_NOT_FOUND,
    URSATZ_GUI_MATERIAL_ERR_NOT_EMPTY,
    URSATZ_GUI_MATERIAL_ERR_REFERENCED,
    URSATZ_GUI_MATERIAL_ERR_FAILED,
    URSATZ_GUI_MATERIAL_ERR_ALLOC,
    URSATZ_GUI_MATERIAL_ERR_NOT_INITIALIZED
} UrsatzGuiMaterialResult;

/*
 * Prepares the store in dir (created if absent, with projects/ inside it).
 * Called once at server startup. When projects/ holds no project, a project
 * named "General" is created. Returns false when the store cannot be used.
 */
bool ursatz_gui_material_service_init(const char *dir);

/* Forgets the store's directory. Nothing persistent is freed or deleted. */
void ursatz_gui_material_service_shutdown(void);

/* --- projects ------------------------------------------------------------ */

/*
 * Creates a project with a generated id ("prj-<hex>"). name is trimmed and
 * must be 1 to 200 characters (INVALID_NAME); description is trimmed and at
 * most 2000 characters (INVALID_NAME), NULL or "" for none. On OK,
 * *out_project (may be NULL) is the new project object, caller-owned.
 */
UrsatzGuiMaterialResult ursatz_gui_material_project_create(const char *name, const char *description,
                                                            cJSON **out_project);

/*
 * Lists every project sorted by name (ASCII case-insensitive), then id. On
 * OK, *out_projects is a caller-owned array of project objects, each with an
 * "item_count" member.
 */
UrsatzGuiMaterialResult ursatz_gui_material_project_list(cJSON **out_projects);

/* One project object with "item_count". NOT_FOUND if absent. Caller-owned. */
UrsatzGuiMaterialResult ursatz_gui_material_project_get(const char *id, cJSON **out_project);

/*
 * Changes the name and/or the description (NULL = unchanged), with the same
 * limits as create. *out_project (may be NULL) is the updated project,
 * caller-owned.
 */
UrsatzGuiMaterialResult ursatz_gui_material_project_update(const char *id, const char *name, const char *description,
                                                            cJSON **out_project);

/* Deletes the project. NOT_EMPTY while it holds items. NOT_FOUND if absent. */
UrsatzGuiMaterialResult ursatz_gui_material_project_delete(const char *id);

#endif /* URSATZ_GUI_MATERIAL_SERVICE_H */
