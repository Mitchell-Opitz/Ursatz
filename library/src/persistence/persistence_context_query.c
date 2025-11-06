#include "persistence/persistence_context_query.h"

#include <stdlib.h>
#include <string.h>

void persistence_free_id_list(char **ids, size_t count) {
    if (ids == NULL) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        free(ids[i]);
    }
    free(ids);
}

/* Runs a prepared statement whose first column is an id and collects every row. */
static PersistenceQueryResult collect_ids(sqlite3_stmt *stmt, char ***out_ids, size_t *out_count) {
    char **ids = NULL;
    size_t count = 0;
    int rc;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const char *text = (const char *)sqlite3_column_text(stmt, 0);
        char **grown = (char **)realloc(ids, (count + 1) * sizeof(*grown));
        if (grown == NULL) {
            persistence_free_id_list(ids, count);
            return PERSISTENCE_QUERY_ERR_ALLOC;
        }
        ids = grown;
        ids[count] = strdup(text == NULL ? "" : text);
        if (ids[count] == NULL) {
            persistence_free_id_list(ids, count);
            return PERSISTENCE_QUERY_ERR_ALLOC;
        }
        count++;
    }
    if (rc != SQLITE_DONE) {
        persistence_free_id_list(ids, count);
        return PERSISTENCE_QUERY_ERR_DB;
    }
    *out_ids = ids;
    *out_count = count;
    return PERSISTENCE_QUERY_OK;
}

PersistenceQueryResult persistence_list_chord_ids_for_context(PersistenceHandle *handle, const char *context_id,
                                                              char ***out_ids, size_t *out_count) {
    if (handle == NULL || context_id == NULL || out_ids == NULL || out_count == NULL) {
        return PERSISTENCE_QUERY_ERR_NULL_ARG;
    }
    const char *sql = "SELECT chords.id FROM chords JOIN interpretations ON interpretations.id = "
                      "chords.interpretation_id WHERE interpretations.context_id = ? ORDER BY chords.rowid";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(persistence_db(handle), sql, -1, &stmt, NULL) != SQLITE_OK) {
        return PERSISTENCE_QUERY_ERR_DB;
    }
    sqlite3_bind_text(stmt, 1, context_id, -1, SQLITE_TRANSIENT);
    PersistenceQueryResult result = collect_ids(stmt, out_ids, out_count);
    sqlite3_finalize(stmt);
    return result;
}

PersistenceQueryResult persistence_list_interpretation_ids_for_context(PersistenceHandle *handle,
                                                                       const char *context_id,
                                                                       const char *claim_value_type,
                                                                       char ***out_ids, size_t *out_count) {
    if (handle == NULL || context_id == NULL || claim_value_type == NULL || out_ids == NULL || out_count == NULL) {
        return PERSISTENCE_QUERY_ERR_NULL_ARG;
    }
    const char *sql = "SELECT id FROM interpretations WHERE context_id = ? AND claim_value_type = ? ORDER BY rowid";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(persistence_db(handle), sql, -1, &stmt, NULL) != SQLITE_OK) {
        return PERSISTENCE_QUERY_ERR_DB;
    }
    sqlite3_bind_text(stmt, 1, context_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, claim_value_type, -1, SQLITE_TRANSIENT);
    PersistenceQueryResult result = collect_ids(stmt, out_ids, out_count);
    sqlite3_finalize(stmt);
    return result;
}
