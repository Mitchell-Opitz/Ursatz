#ifndef MUSIC_PERSISTENCE_PERSISTENCE_H
#define MUSIC_PERSISTENCE_PERSISTENCE_H

#include <sqlite3.h>

/*
 * Persistence (L9, minimal): SQLite-backed storage for a NoteEvent
 * sequence plus the Chord/Key interpretations produced by analyzing it.
 *
 * This is deliberately NOT the full L9 EntityRepository/
 * RelationshipRepository/StructureRepository contract from the System
 * Registry -- just enough for a personal-scale tool to save and reload
 * an analyzed piece. See docs/modules/persistence.md for scope and
 * deferred work.
 *
 * Fail-fast: persistence_open never returns a handle on failure (bad
 * path, permission error, corrupt file, schema-creation failure) --
 * there is no partial/default state to inspect afterward.
 */

typedef struct PersistenceHandle PersistenceHandle;

typedef enum {
    PERSISTENCE_OK = 0,
    PERSISTENCE_ERR_NULL_ARG,
    PERSISTENCE_ERR_OPEN_FAILED,
    PERSISTENCE_ERR_SCHEMA_FAILED,
    PERSISTENCE_ERR_ALLOC
} PersistenceResult;

/*
 * Opens (creating if absent) a SQLite database at path and ensures the
 * schema exists. On any failure, *out is left untouched and no handle
 * is allocated -- nothing to close, nothing left half-open.
 */
PersistenceResult persistence_open(const char *path, PersistenceHandle **out);

/* Closes the underlying database connection and frees handle. */
void persistence_close(PersistenceHandle *handle);

/* Internal accessor for other persistence/*.c translation units only. */
sqlite3 *persistence_db(PersistenceHandle *handle);

#endif
