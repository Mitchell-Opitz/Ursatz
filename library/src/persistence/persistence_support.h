#ifndef MUSIC_PERSISTENCE_PERSISTENCE_SUPPORT_H
#define MUSIC_PERSISTENCE_PERSISTENCE_SUPPORT_H

#include <stdbool.h>
#include <stddef.h>

#include <sqlite3.h>

#include "core/value.h"
#include "identity/entity_id.h"

/*
 * Internal helpers shared by the relationship and structure repositories (persistence/*.c only): savepoints so a
 * multi-table store leaves no partial rows, ordered EntityID lists in a child table, optional Value columns, and an
 * optional EntityID column. Not a public API; the table and column names passed in are this module's own constants,
 * never user input.
 */

/*
 * SAVEPOINT / RELEASE / ROLLBACK TO: unlike BEGIN these nest, so a store inside a caller's own transaction works.
 * rollback undoes everything since begin and releases the savepoint.
 */
bool persistence_savepoint_begin(sqlite3 *db);
bool persistence_savepoint_release(sqlite3 *db);
void persistence_savepoint_rollback(sqlite3 *db);

/* True when table has a row whose id column equals id. *failed is set when the query itself failed. */
bool persistence_support_id_exists(sqlite3 *db, const char *table, const char *id, bool *failed);

typedef const EntityID *(*PersistenceIdAt)(const void *owner, size_t index);

/*
 * Writes count ids, in order, to child_table (parent_id, position, event_id) under parent_id. Returns false on any
 * database error (the caller rolls back).
 */
bool persistence_support_store_id_list(sqlite3 *db, const char *child_table, const char *parent_id, const void *owner,
                                       size_t count, PersistenceIdAt at);

typedef enum {
    PERSISTENCE_SUPPORT_OK = 0,
    PERSISTENCE_SUPPORT_ERR_DB,
    PERSISTENCE_SUPPORT_ERR_ALLOC
} PersistenceSupportResult;

/*
 * Reads the ids stored under parent_id, in position order, as newly created EntityIDs in a newly allocated array
 * (the caller owns both). On any result other than OK nothing is left allocated and the outputs are untouched.
 */
PersistenceSupportResult persistence_support_load_id_list(sqlite3 *db, const char *child_table, const char *parent_id,
                                                          EntityID ***out_ids, size_t *out_count);

/* Binds an optional Value to three consecutive parameters (type, content, size) starting at first; NULL binds NULLs. */
void persistence_support_bind_value(sqlite3_stmt *stmt, int first, const Value *value);

/* Reads an optional Value from three consecutive columns starting at first: NULL type is *out = NULL. */
PersistenceSupportResult persistence_support_column_value(sqlite3_stmt *stmt, int first, Value **out);

/* Binds an optional EntityID's text (or NULL) to parameter index. */
void persistence_support_bind_optional_id(sqlite3_stmt *stmt, int index, const EntityID *id);

/* Reads an optional EntityID from column: NULL column is *out = NULL. */
PersistenceSupportResult persistence_support_column_id(sqlite3_stmt *stmt, int column, EntityID **out);

#endif
