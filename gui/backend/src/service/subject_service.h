#ifndef URSATZ_GUI_SUBJECT_SERVICE_H
#define URSATZ_GUI_SUBJECT_SERVICE_H

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>

/*
 * Subject service: characters, places, ideas and notes that items and Library
 * pieces can be linked to, owned by the GUI (R-115 to R-119, R-130).
 *
 * Layout under the store directory (default "subjects"):
 *   <sub-id>.json   {id, name, kind, note, items: [item ids],
 *                    pieces: [library piece ids], related: [subject ids],
 *                    created_at, updated_at}
 *
 * kind is "character", "place", "idea" or "note".
 *
 * Every file is written to "<file>.tmp" and renamed over the old one, so a
 * reader never sees half a file. civetweb serves requests on several
 * threads, so every operation here runs under one mutex.
 *
 * Subjects are cJSON objects. Every function that returns a cJSON hands the
 * caller ownership: free it with cJSON_Delete.
 */

typedef enum {
    URSATZ_GUI_SUBJECT_OK = 0,
    URSATZ_GUI_SUBJECT_ERR_INVALID_ID,
    URSATZ_GUI_SUBJECT_ERR_INVALID_NAME,
    URSATZ_GUI_SUBJECT_ERR_INVALID_KIND,
    URSATZ_GUI_SUBJECT_ERR_NOT_FOUND,
    URSATZ_GUI_SUBJECT_ERR_ITEM_NOT_FOUND,
    URSATZ_GUI_SUBJECT_ERR_PIECE_NOT_FOUND,
    URSATZ_GUI_SUBJECT_ERR_NOT_LINKED,
    URSATZ_GUI_SUBJECT_ERR_SELF,
    URSATZ_GUI_SUBJECT_ERR_NOT_RELATED,
    URSATZ_GUI_SUBJECT_ERR_FAILED,
    URSATZ_GUI_SUBJECT_ERR_ALLOC,
    URSATZ_GUI_SUBJECT_ERR_NOT_INITIALIZED
} UrsatzGuiSubjectResult;

/* Prepares the store in dir (created if absent). Returns false when it cannot be used. */
bool ursatz_gui_subject_service_init(const char *dir);

/* Forgets the store's directory. Nothing persistent is freed or deleted. */
void ursatz_gui_subject_service_shutdown(void);

/*
 * Creates a subject with a generated id ("sub-<hex>"). name is trimmed and
 * must be 1 to 200 characters (INVALID_NAME); kind must be "character",
 * "place", "idea" or "note" (INVALID_KIND); note is trimmed and at most 4000
 * characters (INVALID_NAME), NULL or "" for none. On OK, *out_subject (may be
 * NULL) is the new subject, caller-owned.
 */
UrsatzGuiSubjectResult ursatz_gui_subject_create(const char *name, const char *kind, const char *note,
                                                 cJSON **out_subject);

/*
 * Lists every subject sorted by name (ASCII case-insensitive), then id. On
 * OK, *out_subjects is a caller-owned array of subject objects, each with
 * "item_count", "piece_count" and "related_count" members.
 */
UrsatzGuiSubjectResult ursatz_gui_subject_list(cJSON **out_subjects);

/* One subject object. NOT_FOUND if absent. Caller-owned. */
UrsatzGuiSubjectResult ursatz_gui_subject_get(const char *id, cJSON **out_subject);

/*
 * Changes the name, the kind and/or the note (NULL = unchanged), with the same
 * limits as create. *out_subject (may be NULL) is the updated subject,
 * caller-owned.
 */
UrsatzGuiSubjectResult ursatz_gui_subject_update(const char *id, const char *name, const char *kind, const char *note,
                                                 cJSON **out_subject);

/*
 * Deletes the subject and removes it from every other subject's related
 * list. NOT_FOUND if absent. Items and pieces it linked are not touched.
 */
UrsatzGuiSubjectResult ursatz_gui_subject_delete(const char *id);

/*
 * Links the subject to a material item (INVALID_ID for a bad item id,
 * ITEM_NOT_FOUND when the material store has no such item). Linking an item
 * that is already linked is not an error: *out_already (may be NULL) is true
 * and nothing changes. *out_subject (may be NULL) is the subject after the
 * call, caller-owned.
 */
UrsatzGuiSubjectResult ursatz_gui_subject_link_item(const char *id, const char *item_id, bool *out_already,
                                                    cJSON **out_subject);

/* Removes an item link. NOT_LINKED when the subject does not link the item. */
UrsatzGuiSubjectResult ursatz_gui_subject_unlink_item(const char *id, const char *item_id, cJSON **out_subject);

/* As link_item, for a Library piece (PIECE_NOT_FOUND when pieces/<id>/meta.json is absent). */
UrsatzGuiSubjectResult ursatz_gui_subject_link_piece(const char *id, const char *piece_id, bool *out_already,
                                                     cJSON **out_subject);

/* Removes a piece link. NOT_LINKED when the subject does not link the piece. */
UrsatzGuiSubjectResult ursatz_gui_subject_unlink_piece(const char *id, const char *piece_id, cJSON **out_subject);

/*
 * Relates two subjects. The relation is stored on both sides. A subject
 * cannot relate to itself (SELF); NOT_FOUND when either is absent. Relating
 * twice is not an error: *out_already (may be NULL) is true and nothing
 * changes. *out_subject (may be NULL) is the first subject after the call,
 * caller-owned.
 */
UrsatzGuiSubjectResult ursatz_gui_subject_relate(const char *id, const char *other_id, bool *out_already,
                                                 cJSON **out_subject);

/* Removes the relation from both sides. NOT_RELATED when the two are not related. */
UrsatzGuiSubjectResult ursatz_gui_subject_unrelate(const char *id, const char *other_id, cJSON **out_subject);

/*
 * Removes the id from the pieces list of every subject that holds it. Called
 * by the analyzer route after a Library piece is deleted. Absent everywhere is
 * fine.
 */
UrsatzGuiSubjectResult ursatz_gui_subject_forget_piece(const char *piece_id);

/*
 * Removes the id from the items list of every subject that holds it. Called
 * by the material service after an item is deleted, with no material lock
 * held. Absent everywhere is fine.
 */
UrsatzGuiSubjectResult ursatz_gui_subject_forget_item(const char *item_id);

/*
 * The panel's view: *out_groups is a caller-owned array of four objects, one
 * per kind in the order character, place, idea, note: {kind, subjects: [{id,
 * name, item_count, piece_count, link_count}]}. link_count is the item count
 * plus the piece count. Subjects are in name order; a kind with none has an
 * empty array.
 */
UrsatzGuiSubjectResult ursatz_gui_subject_groups(cJSON **out_groups);

#endif /* URSATZ_GUI_SUBJECT_SERVICE_H */
