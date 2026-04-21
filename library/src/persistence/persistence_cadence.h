#ifndef MUSIC_PERSISTENCE_PERSISTENCE_CADENCE_H
#define MUSIC_PERSISTENCE_PERSISTENCE_CADENCE_H

#include "persistence/persistence.h"
#include "structure/cadence.h"

/*
 * Stores and loads Cadences (structure/cadence.h) in the `cadences` table, with the event ids in order in the child
 * table `cadence_events`. The optional harmonic-context Value is stored as type, content and size columns (the
 * Interpretation claim pattern); an absent context is NULL columns and comes back as no context. A stored Cadence is
 * a descriptive record: it promotes nothing to a Rule and carries no authority.
 *
 * References by EntityID only. The event ids are text values, not foreign keys, and are not checked against the
 * stored note events (they may be stored later, elsewhere, or never). The only foreign key is from `cadence_events`
 * to its own parent `cadences`.
 *
 * The id list reloads in the order it was stored. Storing writes the parent row and the whole list in one
 * savepoint, so a store that fails partway leaves no rows. Storing an id that already exists is
 * PERSISTENCE_CADENCE_ERR_DUPLICATE_ID and changes nothing (the older repositories report a duplicate key as the
 * generic ERR_DB; this one names it). Loading an id that is not stored is PERSISTENCE_CADENCE_ERR_NOT_FOUND and *out
 * is untouched.
 */

typedef enum {
    PERSISTENCE_CADENCE_OK = 0,
    PERSISTENCE_CADENCE_ERR_NULL_ARG,
    PERSISTENCE_CADENCE_ERR_DB,
    PERSISTENCE_CADENCE_ERR_ALLOC,
    PERSISTENCE_CADENCE_ERR_NOT_FOUND,
    PERSISTENCE_CADENCE_ERR_DUPLICATE_ID
} PersistenceCadenceResult;

PersistenceCadenceResult persistence_store_cadence(PersistenceHandle *handle, const Cadence *cadence);

PersistenceCadenceResult persistence_load_cadence(PersistenceHandle *handle, const char *cadence_id,
                                                  Cadence **out_cadence);

#endif
