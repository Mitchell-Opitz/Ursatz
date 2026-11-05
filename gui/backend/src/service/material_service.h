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
 *     kept, changed, harmony, functions, flag}, plus an optional boolean standalone: a derived motif is a variant
 *     of its source unless standalone is true (R-187)).
 *   theme: form (null, a string or an object) and slots (an array). A body with a form object, a slot written as
 *     {slot, item_id, version} or any of pool, accompaniment, brief, style_weight, without, tempo, check, seed and
 *     result is a composable theme (R-167):
 *       form {into, hybrid, key, mode (major | minor | style), meter, bars (4 | 8), cadence, first_cadence}, each
 *       member null or as the Composer routes take it; slots [{slot (basic | contrasting | continuation | cadential),
 *       item_id, version}] with basic once and each slot at most once; pool (approved-and-unflagged | approved |
 *       none); accompaniment (null, {pattern} or {pattern: "motive", item_id, version}); brief (null or {scopes,
 *       piece_ids, style_ids}); style_weight ("n" or "n/d"); without (up to 4 motive categories); tempo (null or
 *       20 to 400); check (bool); seed (null or a whole number); result (null or an object).
 *     Any other theme is a placeholder and stays valid. Creating or adding a version checks that each slot names a
 *     melodic motif of form full at an existing version and the accompaniment motive an accompaniment motif, and
 *     replaces the body's uses of those items by one use for each version named.
 *   piece: structure (null, a string, an object or an array).
 */
UrsatzGuiMaterialResult ursatz_gui_material_body_validate(const char *type, const cJSON *body, char *error,
                                                           size_t error_size);

/*
 * *out is a copy of body (a theme or piece form not saved yet) with the uses its references imply and a fresh copy of
 * each used version in them, as a save makes them; nothing is written. Not OK when a reference names no usable motif
 * or version. Caller-owned.
 */
UrsatzGuiMaterialResult ursatz_gui_material_body_with_uses(const cJSON *body, cJSON **out);

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

/* As ursatz_gui_material_item_create, with note (trimmed, at most 500 characters, NULL or "" for none) on version 1. */
UrsatzGuiMaterialResult ursatz_gui_material_item_create_noted(const char *type, const char *name,
                                                               const char *project_id, const cJSON *body,
                                                               const char *note, cJSON **out_item);

/*
 * Lists items, newest "updated_at" first, then id. project_id and type
 * (each may be NULL for any) filter the list. On OK, *out_items is a
 * caller-owned array of item objects.
 */
UrsatzGuiMaterialResult ursatz_gui_material_item_list(const char *project_id, const char *type, cJSON **out_items);

/*
 * Whether a motif version body is a variant: it has a derived record and standalone is not true (R-187). Any other
 * body, a non-motif body included, is not.
 *
 * Every item object the list and the get answer carries "variant_of": the derived source_item of a variant, else
 * null. It is computed from the current version and never stored in item.json.
 */
bool ursatz_gui_material_item_is_variant(const cJSON *body);

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

/*
 * Lists, in one scan of the store, every motif whose current version has a derived record: *out_motifs is a
 * caller-owned array of {id, name, current_version, derived}, in id order.
 */
UrsatzGuiMaterialResult ursatz_gui_material_derived_motifs(cJSON **out_motifs);

/*
 * Writes a new version of item id identical to its current one, except that
 * every use of source_id points at the source's current version with a fresh
 * copy; the note is "updated <source name> to v<n>". NOT_FOUND when the item
 * or the source is absent or the current version does not use the source.
 * *out_item (may be NULL) is the updated item.json, caller-owned.
 */
UrsatzGuiMaterialResult ursatz_gui_material_update_use(const char *id, const char *source_id, cJSON **out_item);

/*
 * Deletes the item directory with every version, and with it the variants of the item, recursively (R-187: a variant
 * belongs to its motif). NOT_FOUND if absent; REFERENCED while items outside that set use any member of it, unless
 * force is true. A forced delete leaves the users' copies untouched; their uses then report missing. Nothing is
 * deleted on REFERENCED.
 */
UrsatzGuiMaterialResult ursatz_gui_material_item_delete(const char *id, bool force);

/*
 * The items that keep ursatz_gui_material_item_delete from deleting id: those outside id and its variants that use any
 * of them, *out_users a caller-owned array of {id, name, type, project_id, version} (each item once). Empty when
 * the delete would go through.
 */
UrsatzGuiMaterialResult ursatz_gui_material_delete_blockers(const char *id, cJSON **out_users);

/* --- operation marks ----------------------------------------------------- */

/*
 * The marks a motif keeps from Try every operation (R-190): items/<id>/op-checks.json, beside the versions and not a
 * version, so a mark never makes a new version. {"checks": {"<op>": {verdict ("works" | "wrong"), comment (text, at
 * most 500 bytes), seed (whole number from 0), notes_text (the result checked, at most 4000 bytes), date
 * ("YYYY-MM-DD")}}}, at most 64 operations, each named in lowercase letters, digits and hyphens. Deleting the item
 * deletes the file with its folder.
 */

/* Checks body against that shape; INVALID_BODY with the reason in error. */
UrsatzGuiMaterialResult ursatz_gui_material_op_checks_validate(const cJSON *body, char *error, size_t error_size);

/* *out is the marks of the motif id, {"checks": {}} when it has none. NOT_FOUND for an item that is not a motif. */
UrsatzGuiMaterialResult ursatz_gui_material_op_checks_get(const char *id, cJSON **out);

/* Replaces the marks of the motif id with body (validated first); *out is what is stored. */
UrsatzGuiMaterialResult ursatz_gui_material_op_checks_put(const char *id, const cJSON *body, char *error,
                                                           size_t error_size, cJSON **out);

/* --- version files ------------------------------------------------------- */

/*
 * A theme version keeps its Composer files beside its body, in items/<id>/v<n>/ (R-167): request.json,
 * brief.style.json, plan.json, theme.mid and theme.musicxml. A piece version keeps request.json, brief.style.json,
 * plan.json, ending-plan.json, loop.mid, loop.musicxml, ending.mid and ending.musicxml, and
 * theme-<section id>.plan.json for each theme a section uses (R-174). The folder is written before the version is, so
 * a version that exists has its files whole; deleting the item removes them with its directory.
 */

/* The content type of one of those names ("application/json", "audio/midi", "application/xml"), else NULL. */
const char *ursatz_gui_material_version_file_type(const char *name);

/* The folder of version number of item id, as a path under the store (it need not exist). false if it does not fit. */
bool ursatz_gui_material_version_dir(const char *id, int number, char *out, size_t out_size);

/* The folder for the MIDI files of unsaved renders, "<store>/.render-tmp" (the Analyzer imports by path, R-275). It
 * need not exist; false if the store is not ready or the path does not fit. */
bool ursatz_gui_material_render_tmp_dir(char *out, size_t out_size);

/*
 * The bytes of one of the five files of version number (1 to current_version). NOT_FOUND for any other name, an
 * absent item, version or file. *out_data is caller-owned (free).
 */
UrsatzGuiMaterialResult ursatz_gui_material_version_file(const char *id, int number, const char *name,
                                                          unsigned char **out_data, size_t *out_size);

/*
 * One-time re-analysis for tone labels (R-204): every full motif whose current version's analysis has no tones is
 * analyzed again and saved as a new version with the note "Re-analyzed" (the Re-analyze path). A motif the Composer
 * refuses or cannot analyze keeps its analysis_error and does not stop the others. A marker file `tones-migrated` in
 * materials_dir stops later runs; without a Composer nothing is changed and no marker is written. Returns the number of
 * motifs re-analyzed. Call after ursatz_gui_material_service_init.
 */
int ursatz_gui_material_migrate_tones(const char *materials_dir);

/*
 * One-time clear of the themes for the v2 body (R-205): every theme item is deleted, referenced or not (a piece that used
 * one then shows its source missing, R-131). A marker file `themes-cleared` in materials_dir stops later runs. A failure
 * is logged and does not stop the start; no marker is written then, so the next start tries again. Returns the number of
 * themes deleted. Call after ursatz_gui_material_service_init.
 */
int ursatz_gui_material_clear_themes(const char *materials_dir);

#endif /* URSATZ_GUI_MATERIAL_SERVICE_H */
