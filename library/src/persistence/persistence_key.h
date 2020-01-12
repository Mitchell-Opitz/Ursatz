#ifndef MUSIC_PERSISTENCE_KEY_H
#define MUSIC_PERSISTENCE_KEY_H

#include "interpretation/key.h"
#include "persistence/persistence.h"

/*
 * Key persistence: stores/reloads a Key (interpretation/key.h) into the
 * `keys` table, mirroring persistence_chord.h's treatment of Chord --
 * see that header's comment for the shared-id rationale.
 */

typedef enum {
    PERSISTENCE_KEY_OK = 0,
    PERSISTENCE_KEY_ERR_NULL_ARG,
    PERSISTENCE_KEY_ERR_DB,
    PERSISTENCE_KEY_ERR_ALLOC
} PersistenceKeyResult;

PersistenceKeyResult persistence_store_key(PersistenceHandle *handle, const Key *key);

/* key_id is the Key's embedded Interpretation's own EntityID value. */
PersistenceKeyResult persistence_load_key(PersistenceHandle *handle, const char *key_id, Key **out_key);

#endif
