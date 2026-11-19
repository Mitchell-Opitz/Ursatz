#ifndef WORKSHOP_ITEMS_STORE_H
#define WORKSHOP_ITEMS_STORE_H

#include <common/status.h>

#include "txn.h"

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

/*
 * Prepares the store (projects/ in the data folder, created if absent).
 * Called once at server startup. When projects/ holds no project, a project
 * named "General" is created. Returns false when the store cannot be used.
 */
bool workshop_material_service_init(void);

/* Forgets the store's directory. Nothing persistent is freed or deleted. */
void workshop_material_service_shutdown(void);

/*
 * The function the store calls for every item an item delete removes, after it released its lock (the subjects drop their links to
 * it: the coordinator registers workshop_subject_forget_item's adapter at open, so the store includes no subjects header).
 * NULL (the start state) calls nothing.
 */
void workshop_store_set_forget_item(void (*forget)(WorkshopTxn *txn, const char *item_id));

/* --- projects ------------------------------------------------------------ */

/*
 * Creates a project with a generated id ("prj-<hex>"). name is trimmed and
 * must be 1 to 200 characters (INVALID_NAME); description is trimmed and at
 * most 2000 characters (INVALID_NAME), NULL or "" for none. On OK,
 * *out_project (may be NULL) is the new project object, caller-owned.
 */
UrsatzStatus workshop_material_project_create(const char *name, const char *description,
                                                            cJSON **out_project);

/*
 * Lists every project sorted by name (ASCII case-insensitive), then id. On
 * OK, *out_projects is a caller-owned array of project objects, each with an
 * "item_count" member.
 */
UrsatzStatus workshop_material_project_list(cJSON **out_projects);

/* One project object with "item_count". NOT_FOUND if absent. Caller-owned. */
UrsatzStatus workshop_material_project_get(const char *id, cJSON **out_project);

/*
 * Changes the name and/or the description (NULL = unchanged), with the same
 * limits as create. *out_project (may be NULL) is the updated project,
 * caller-owned.
 */
UrsatzStatus workshop_material_project_update(const char *id, const char *name, const char *description,
                                                            cJSON **out_project);

/* Deletes the project. NOT_EMPTY while it holds items. NOT_FOUND if absent. */
UrsatzStatus workshop_material_project_delete(const char *id);

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
 *     notes (1 to 4000 characters), meter ("n/m"), material (null or the id of a material, "omt.material.major"; a motif is in C),
 *     pickup (beats, "n" or "n/d"), features [{label 1 to 8 characters,
 *     first, last with 0 <= first <= last}], analysis (null or an object),
 *     derived (null or {source_item, source_version, operations, seed, kind,
 *     kept, changed, harmony, functions, flag}, plus an optional boolean standalone: a derived motif is a variant
 *     of its source unless standalone is true (R-187)).
 *   theme: form (null, a string or an object) and slots (an array). A body with a form object, a slot written as
 *     {slot, item_id, version} or any of pool, accompaniment, brief, style_weight, without, tempo, check, seed and
 *     result is a composable theme (R-167):
 *       form {into, hybrid, material (a material's id | style), meter, bars (4 | 8), cadence, first_cadence}, each
 *       member null or as the Composer routes take it; slots [{slot (basic | contrasting | continuation | cadential),
 *       item_id, version}] with basic once and each slot at most once; pool (approved-and-unflagged | approved |
 *       none); accompaniment (null, {pattern} or {pattern: "motive", item_id, version}); brief (null or {scopes,
 *       piece_ids, style_ids}); style_weight ("n" or "n/d"); without (up to 4 motive categories); tempo (null or
 *       20 to 400); check (bool); seed (null or a whole number); result (null or an object).
 *     Any other theme is a placeholder and stays valid. Creating or adding a version checks that each slot names a
 *     melodic motif of form full at an existing version and the accompaniment motive an accompaniment motif, and
 *     replaces the body's uses of those items by one use for each version named.
 *   piece: structure (null, a string, an object or an array); an object has key (the tonic), material (a material's id |
 *     style), meter and the form's own members.
 *   The old members are read: a motif's key "C major" or "C minor", a theme form's or a piece structure's mode (major, minor,
 *   style). workshop_material_body_migrate gives the new ones, and every body read from the store or saved has them.
 */
UrsatzStatus workshop_material_body_validate(const char *type, const cJSON *body, UrsatzError *error);

/*
 * *out is a copy of body (a theme or piece form not saved yet) with the uses its references imply and a fresh copy of
 * each used version in them, as a save makes them; nothing is written. Not OK when a reference names no usable motif
 * or version. Caller-owned.
 */
UrsatzStatus workshop_material_body_with_uses(const cJSON *body, cJSON **out);

/*
 * Writes the members of a body as the store keeps them: a motif's `material` (the id) for its key text "C major" or
 * "C minor", a theme form's and a piece structure's `material` for their `mode` word, in the uses' copies too. A member
 * already new wins, and a value that names no material stays as it was. In place; a body that is not an object is left alone.
 */
void workshop_material_body_migrate(cJSON *body);

/* --- items and versions -------------------------------------------------- */

/*
 * Creates an item of type "motif", "theme" or "piece" (INVALID_TYPE) with a
 * generated id, in the project project_id (INVALID_ID, NOT_FOUND). name has
 * the project name limits (INVALID_NAME). body is the first version, v1
 * (INVALID_BODY when workshop_material_body_validate refuses it), with note (trimmed, at most 500 characters, NULL
 * or "" for none) on version 1. On OK, *out_item (may be NULL) is the item object (item.json), caller-owned.
 */
UrsatzStatus workshop_material_item_create_noted(const char *type, const char *name,
                                                               const char *project_id, const cJSON *body,
                                                               const char *note, cJSON **out_item);

/*
 * Lists items, newest "updated_at" first, then id. project_id and type
 * (each may be NULL for any) filter the list. On OK, *out_items is a
 * caller-owned array of item objects.
 */
UrsatzStatus workshop_material_item_list(const char *project_id, const char *type, cJSON **out_items);

/*
 * Whether a motif version body is a variant: it has a derived record and standalone is not true (R-187). Any other
 * body, a non-motif body included, is not.
 *
 * Every item object the list and the get answer carries "variant_of": the derived source_item of a variant, else
 * null. It is computed from the current version and never stored in item.json.
 */
bool workshop_material_item_is_variant(const cJSON *body);

/*
 * Loads one item: *out_meta (may be NULL) is item.json, *out_body (may be
 * NULL) the body of its current version. Both caller-owned. NOT_FOUND if
 * absent.
 */
UrsatzStatus workshop_material_item_get(const char *id, cJSON **out_meta, cJSON **out_body);

/* Renames the item. *out_item (may be NULL) is the updated item.json, caller-owned. */
UrsatzStatus workshop_material_item_rename(const char *id, const char *name, cJSON **out_item);

/* Moves the item to another project (NOT_FOUND if the project is absent). */
UrsatzStatus workshop_material_item_move(const char *id, const char *project_id, cJSON **out_item);

/*
 * Writes body as version current_version + 1 and makes it current. note is
 * trimmed and at most 500 characters (INVALID_NAME), NULL or "" for none.
 * *out_item (may be NULL) is the updated item.json, caller-owned.
 */
UrsatzStatus workshop_material_item_add_version(const char *id, const cJSON *body, const char *note,
                                                              cJSON **out_item);

/*
 * As workshop_material_item_add_version, with the files of the version made beforehand: stage is a folder (beside the version folder) that
 * becomes the version folder, so a version never exists without its files. The stage is gone on success and on failure of the commit.
 */
UrsatzStatus workshop_material_item_add_version_staged(const char *id, const cJSON *body, const char *note, const char *stage,
                                                       cJSON **out_item);

/* The body of version number (1 to current_version). NOT_FOUND otherwise. Caller-owned. */
UrsatzStatus workshop_material_item_get_version(const char *id, int number, cJSON **out_body);

/*
 * Reports, for every use in the item's current version, how its source stands.
 * *out_status is a caller-owned array of {item_id, version (the used one),
 * updated, missing} plus, while the source exists, current_version, name, type
 * and project_id of the source. "updated" is true when the source's
 * current_version is newer than the used version; "missing" is true when the
 * source was deleted (the copy stays in the body). NOT_FOUND if the item is absent.
 */
UrsatzStatus workshop_material_uses_status(const char *id, cJSON **out_status);

/*
 * Lists the items whose current version uses any version of id, in id order.
 * *out_users is a caller-owned array of {id, name, type, project_id, version
 * (the used one)}. id need not exist: a deleted source can still have users.
 */
UrsatzStatus workshop_material_used_by(const char *id, cJSON **out_users);

/*
 * Lists, in one scan of the store, every motif whose current version has a derived record: *out_motifs is a
 * caller-owned array of {id, name, current_version, derived}, in id order.
 */
UrsatzStatus workshop_material_derived_motifs(cJSON **out_motifs);

/*
 * Writes a new version of item id identical to its current one, except that
 * every use of source_id points at the source's current version with a fresh
 * copy; the note is "updated <source name> to v<n>". NOT_FOUND when the item
 * or the source is absent or the current version does not use the source.
 * *out_item (may be NULL) is the updated item.json, caller-owned.
 */
UrsatzStatus workshop_material_update_use(const char *id, const char *source_id, cJSON **out_item);

/*
 * Deletes the item directory with every version, and with it the variants of the item, recursively (R-187: a variant
 * belongs to its motif). NOT_FOUND if absent; REFERENCED while items outside that set use any member of it, unless
 * force is true. A forced delete leaves the users' copies untouched; their uses then report missing. Nothing is
 * deleted on REFERENCED.
 */
UrsatzStatus workshop_material_item_delete(const char *id, bool force);

/*
 * The items that keep workshop_material_item_delete from deleting id: those outside id and its variants that use any
 * of them, *out_users a caller-owned array of {id, name, type, project_id, version} (each item once). Empty when
 * the delete would go through.
 */
UrsatzStatus workshop_material_delete_blockers(const char *id, cJSON **out_users);

/* --- operation marks ----------------------------------------------------- */

/*
 * The marks a motif keeps from Try every operation (R-190): items/<id>/op-checks.json, beside the versions and not a
 * version, so a mark never makes a new version. {"checks": {"<op>": {verdict ("works" | "wrong"), comment (text, at
 * most 500 bytes), seed (whole number from 0), notes_text (the result checked, at most 4000 bytes), date
 * ("YYYY-MM-DD")}}}, at most 64 operations, each named in lowercase letters, digits and hyphens. Deleting the item
 * deletes the file with its folder.
 */

/* Checks body against that shape; INVALID_BODY with the reason in error. */
UrsatzStatus workshop_material_op_checks_validate(const cJSON *body, UrsatzError *error);

/* *out is the marks of the motif id, {"checks": {}} when it has none. NOT_FOUND for an item that is not a motif. */
UrsatzStatus workshop_material_op_checks_get(const char *id, cJSON **out);

/* Replaces the marks of the motif id with body (validated first); *out is what is stored. */
UrsatzStatus workshop_material_op_checks_put(const char *id, const cJSON *body, UrsatzError *error, cJSON **out);

/* --- version files ------------------------------------------------------- */

/*
 * A theme version keeps its Composer files beside its body, in projects/<prj>/items/<id>/v<n>/ (R-167): request.json,
 * brief.style.json, plan.json, theme.mid and theme.musicxml. A piece version keeps request.json, brief.style.json
 * and, for each version of its form named <name> (the game loop's loop and ending, a sized form's piece), <name>.mid and
 * <name>.musicxml with the plan: plan.json for the first version, <name>-plan.json for each later one (so a game loop
 * has plan.json, ending-plan.json, loop.mid, loop.musicxml, ending.mid and ending.musicxml), and
 * theme-<section id>.plan.json for each theme a section uses (R-174, R-430). The folder is written before the version is, so
 * a version that exists has its files whole; deleting the item removes them with its directory.
 */

/* The content type of one of those names ("application/json", "audio/midi", "application/xml"), else NULL. */
const char *workshop_material_version_file_type(const char *name);

/* Whether name is a version name of a piece's form: a lowercase letter, then up to 30 lowercase letters, digits or
 * hyphens. The names of the files of a version are made from it. */
bool workshop_material_version_name_ok(const char *name);

/* The folder of version number of item id, as a path under the data folder (it need not exist; the item must). false if it does not fit. */
bool workshop_material_version_dir(const char *id, int number, char *out, size_t out_size);

/* The folder for the MIDI files of unsaved renders, "cache/tmp" (the Analyzer imports by path, R-275). It
 * need not exist; false if the store is not ready or the path does not fit. */
bool workshop_material_render_tmp_dir(char *out, size_t out_size);

/*
 * The bytes of one of the five files of version number (1 to current_version). NOT_FOUND for any other name, an
 * absent item, version or file. *out_data is caller-owned (free).
 */
UrsatzStatus workshop_material_version_file(const char *id, int number, const char *name,
                                                          unsigned char **out_data, size_t *out_size);

#endif /* WORKSHOP_ITEMS_STORE_H */
