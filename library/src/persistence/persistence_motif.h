#ifndef MUSIC_PERSISTENCE_PERSISTENCE_MOTIF_H
#define MUSIC_PERSISTENCE_PERSISTENCE_MOTIF_H

#include "persistence/persistence.h"
#include "structure/motif.h"

/*
 * Stores and loads Motifs (structure/motif.h) in the `motifs` table, with the event ids in order in the child table
 * `motif_events`. A stored Motif is a descriptive record: it promotes nothing to a Rule and carries no authority.
 *
 * References by EntityID only. The event ids a Motif lists are text values, not foreign keys, and are not checked
 * against the stored note events (they may be stored later, elsewhere, or never). The only foreign key is from
 * `motif_events` to its own parent `motifs`.
 *
 * The id list reloads in the order it was stored. Storing writes the parent row and the whole list in one
 * savepoint, so a store that fails partway leaves no rows. Storing an id that already exists is
 * PERSISTENCE_MOTIF_ERR_DUPLICATE_ID and changes nothing (the older repositories report a duplicate key as the
 * generic ERR_DB; this one names it). Loading an id that is not stored is PERSISTENCE_MOTIF_ERR_NOT_FOUND and *out is
 * untouched.
 */

typedef enum {
    PERSISTENCE_MOTIF_OK = 0,
    PERSISTENCE_MOTIF_ERR_NULL_ARG,
    PERSISTENCE_MOTIF_ERR_DB,
    PERSISTENCE_MOTIF_ERR_ALLOC,
    PERSISTENCE_MOTIF_ERR_NOT_FOUND,
    PERSISTENCE_MOTIF_ERR_DUPLICATE_ID
} PersistenceMotifResult;

PersistenceMotifResult persistence_store_motif(PersistenceHandle *handle, const Motif *motif);

PersistenceMotifResult persistence_load_motif(PersistenceHandle *handle, const char *motif_id, Motif **out_motif);

#endif
