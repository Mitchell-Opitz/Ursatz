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
 *   items/<id>/item.json     {id, type, name, project_id, current_version,
 *                             versions: [{number, created_at, note}],
 *                             created_at, updated_at}
 *   items/<id>/v<n>.json     the version body, numbered from 1
 *
 * An item is a motif, a theme or a piece (ids "mot-", "thm-", "pce-"). A
 * version is never deleted or rewritten: a change to the body is a new
 * version, and rename and move change item.json only.
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

/* --- version bodies ------------------------------------------------------ */

/*
 * Checks a version body for an item of the given type ("motif", "theme" or
 * "piece"; INVALID_TYPE otherwise). Returns OK or INVALID_BODY; when error
 * is not NULL, it receives a one-line reason ("" on OK).
 *
 * Every body is an object whose "uses" is an array (may be empty) of
 * {item_id, version, copy}. A body carries its own type's member ("motif",
 * "theme" or "piece") and no other:
 *   motif: role (melodic | accompaniment), form (full | rhythm | contour),
 *     notes (1 to 4000 characters), meter ("n/m"), key (null or a string),
 *     pickup (beats, "n" or "n/d"), features [{label 1 to 8 characters,
 *     first, last with 0 <= first <= last}], analysis (null or an object),
 *     derived (null or {source_item, source_version, operations, seed, kind,
 *     kept, changed, harmony, functions, flag}).
 *   theme: form (null, a string or an object) and slots (an array).
 *   piece: structure (null, a string, an object or an array).
 */
UrsatzGuiMaterialResult ursatz_gui_material_body_validate(const char *type, const cJSON *body, char *error,
                                                           size_t error_size);

/* --- items and versions -------------------------------------------------- */

/*
 * Creates an item of type "motif", "theme" or "piece" (INVALID_TYPE) with a
 * generated id, in the project project_id (INVALID_ID, NOT_FOUND). name has
 * the project name limits (INVALID_NAME). body is the first version, v1
 * (INVALID_BODY when ursatz_gui_material_body_validate refuses it). On OK, *out_item (may be NULL)
 * is the item object (item.json), caller-owned.
 */
UrsatzGuiMaterialResult ursatz_gui_material_item_create(const char *type, const char *name, const char *project_id,
                                                         const cJSON *body, cJSON **out_item);

/*
 * Lists items, newest "updated_at" first, then id. project_id and type
 * (each may be NULL for any) filter the list. On OK, *out_items is a
 * caller-owned array of item objects.
 */
UrsatzGuiMaterialResult ursatz_gui_material_item_list(const char *project_id, const char *type, cJSON **out_items);

/*
 * Loads one item: *out_meta (may be NULL) is item.json, *out_body (may be
 * NULL) the body of its current version. Both caller-owned. NOT_FOUND if
 * absent.
 */
UrsatzGuiMaterialResult ursatz_gui_material_item_get(const char *id, cJSON **out_meta, cJSON **out_body);

/* Renames the item. *out_item (may be NULL) is the updated item.json, caller-owned. */
UrsatzGuiMaterialResult ursatz_gui_material_item_rename(const char *id, const char *name, cJSON **out_item);

/* Moves the item to another project (NOT_FOUND if the project is absent). */
UrsatzGuiMaterialResult ursatz_gui_material_item_move(const char *id, const char *project_id, cJSON **out_item);

/*
 * Writes body as version current_version + 1 and makes it current. note is
 * trimmed and at most 500 characters (INVALID_NAME), NULL or "" for none.
 * *out_item (may be NULL) is the updated item.json, caller-owned.
 */
UrsatzGuiMaterialResult ursatz_gui_material_item_add_version(const char *id, const cJSON *body, const char *note,
                                                              cJSON **out_item);

/* The body of version number (1 to current_version). NOT_FOUND otherwise. Caller-owned. */
UrsatzGuiMaterialResult ursatz_gui_material_item_get_version(const char *id, int number, cJSON **out_body);

/*
 * Reports, for every use in the item's current version, how its source stands.
 * *out_status is a caller-owned array of {item_id, version (the used one),
 * updated, missing} plus, while the source exists, current_version, name, type
 * and project_id of the source. "updated" is true when the source's
 * current_version is newer than the used version; "missing" is true when the
 * source was deleted (the copy stays in the body). NOT_FOUND if the item is absent.
 */
UrsatzGuiMaterialResult ursatz_gui_material_uses_status(const char *id, cJSON **out_status);

/*
 * Lists the items whose current version uses any version of id, in id order.
 * *out_users is a caller-owned array of {id, name, type, project_id, version
 * (the used one)}. id need not exist: a deleted source can still have users.
 */
UrsatzGuiMaterialResult ursatz_gui_material_used_by(const char *id, cJSON **out_users);

/* Deletes the item directory with every version. NOT_FOUND if absent. */
UrsatzGuiMaterialResult ursatz_gui_material_item_delete(const char *id);

#endif /* URSATZ_GUI_MATERIAL_SERVICE_H */
