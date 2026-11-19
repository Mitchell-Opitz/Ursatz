#ifndef WORKSHOP_COLLECTION_SERVICE_H
#define WORKSHOP_COLLECTION_SERVICE_H

#include <common/status.h>

#include <stdbool.h>
#include <stddef.h>

/*
 * Collection service: named groups of piece ids, owned by the GUI (R-105).
 *
 * Each collection is one file, <dir>/<id>.json:
 *   {"id", "name", "description", "pieces": [piece ids in add order],
 *    "created_at", "updated_at"}            (timestamps ISO 8601 UTC)
 * written to "<file>.tmp" and renamed over the old one, so a reader never
 * sees half a file. civetweb serves requests on several threads, so every
 * operation here runs under one mutex.
 *
 * A collection holds ids only: a piece is not copied into it, and a
 * collection is a fixed set. The Tags panel is the live grouping.
 */

/* One entry in workshop_collection_list's result. */
typedef struct {
    char *id;
    char *name;
    char *description; /* "" when unset */
    size_t piece_count;
    char *updated_at;
} WorkshopCollectionEntry;

typedef struct {
    char *id;
    char *name;
    char *description;
    char **pieces; /* piece_count piece ids in add order; NULL when none */
    size_t piece_count;
    char *created_at;
    char *updated_at;
} WorkshopCollection;

/*
 * Prepares the store in dir (created if absent). Called once at server
 * startup. Returns false when the store cannot be used.
 */
bool ursatz_gui_collection_service_init(const char *dir);

/* Forgets the store's directory. Nothing persistent is freed or deleted. */
void workshop_collection_service_shutdown(void);

/*
 * True for a plain path-segment id: 1 to 64 characters, each alnum, '-' or
 * '_'. Used for collection ids and piece ids taken from HTTP requests, and
 * by the Composer for composition ids.
 */
bool workshop_collection_is_valid_id(const char *id);

/* True when pieces/<piece_id>/meta.json exists. */
bool workshop_collection_piece_exists(const char *piece_id);

/* True when a collection with this id exists. */
bool workshop_collection_exists(const char *id);

/*
 * Creates a collection with a generated id ("col-<hex>"). name is
 * normalized like piece metadata values and must be non-empty and at most
 * 120 bytes (INVALID_NAME); description is trimmed, whitespace-collapsed and
 * at most 500 bytes (INVALID_DESCRIPTION), NULL or "" for none. On OK,
 * *out_id is newly allocated and caller-owned.
 */
UrsatzStatus workshop_collection_create(const char *name, const char *description, char **out_id);

/* As create, with the caller's id: INVALID_ID if invalid, ALREADY_EXISTS if used. */
UrsatzStatus workshop_collection_create_with_id(const char *id, const char *name,
                                                                const char *description);

/*
 * Lists every collection sorted by name (ASCII case-insensitive), then id.
 * On OK, *out_entries holds *out_count entries (NULL when none); free with
 * workshop_collection_free_entries.
 */
UrsatzStatus workshop_collection_list(WorkshopCollectionEntry **out_entries, size_t *out_count);

void workshop_collection_free_entries(WorkshopCollectionEntry *entries, size_t count);

/* Loads one collection. NOT_FOUND if absent. Free with workshop_collection_destroy. */
UrsatzStatus workshop_collection_get(const char *id, WorkshopCollection **out_collection);

void workshop_collection_destroy(WorkshopCollection *collection);

/* Changes the name and/or the description (NULL = unchanged). */
UrsatzStatus workshop_collection_update(const char *id, const char *name, const char *description);

/* Deletes the collection. Its pieces are not touched. NOT_FOUND if absent. */
UrsatzStatus workshop_collection_delete(const char *id);

/*
 * Adds the piece ids (each a valid id; whether the piece exists is the
 * caller's check) that are not members yet, in order. *added counts the
 * ones added, *already the ones that were members, or repeated in ids.
 */
UrsatzStatus workshop_collection_add_pieces(const char *id, const char *const *piece_ids, size_t count,
                                                            size_t *added, size_t *already);

/* NOT_MEMBER if the piece is not in the collection. */
UrsatzStatus workshop_collection_remove_piece(const char *id, const char *piece_id);

/* Removes the piece id from every collection that holds it (absent everywhere is fine). */
UrsatzStatus workshop_collection_remove_piece_everywhere(const char *piece_id);

#endif /* WORKSHOP_COLLECTION_SERVICE_H */
