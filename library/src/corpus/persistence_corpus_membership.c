#include "corpus/persistence_corpus_membership.h"

#include <stdlib.h>
#include <string.h>

static const char *CORPUS_MEMBERSHIP_SCHEMA =
    "CREATE TABLE IF NOT EXISTS corpus_members ("
    "  corpus_id TEXT NOT NULL,"
    "  piece_ref TEXT NOT NULL,"
    "  UNIQUE(corpus_id, piece_ref)"
    ");";

static PersistenceCorpusMembershipResult ensure_schema(PersistenceHandle *handle) {
    sqlite3 *db = persistence_db(handle);
    char *errmsg = NULL;
    int rc = sqlite3_exec(db, CORPUS_MEMBERSHIP_SCHEMA, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        sqlite3_free(errmsg);
        return PERSISTENCE_CORPUS_MEMBERSHIP_ERR_DB;
    }
    return PERSISTENCE_CORPUS_MEMBERSHIP_OK;
}

static int is_member(sqlite3 *db, const char *corpus_id, const char *piece_ref) {
    const char *sql = "SELECT 1 FROM corpus_members WHERE corpus_id = ? AND piece_ref = ?";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }
    sqlite3_bind_text(stmt, 1, corpus_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, piece_ref, -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    int found = (rc == SQLITE_ROW) ? 1 : 0;
    sqlite3_finalize(stmt);
    if (rc != SQLITE_ROW && rc != SQLITE_DONE) {
        return -1;
    }
    return found;
}

PersistenceCorpusMembershipResult corpus_add_piece_ref(PersistenceHandle *handle, const char *corpus_id,
                                                        const char *piece_ref) {
    if (handle == NULL || corpus_id == NULL || piece_ref == NULL) {
        return PERSISTENCE_CORPUS_MEMBERSHIP_ERR_NULL_ARG;
    }

    PersistenceCorpusMembershipResult schema_result = ensure_schema(handle);
    if (schema_result != PERSISTENCE_CORPUS_MEMBERSHIP_OK) {
        return schema_result;
    }

    sqlite3 *db = persistence_db(handle);

    int already = is_member(db, corpus_id, piece_ref);
    if (already < 0) {
        return PERSISTENCE_CORPUS_MEMBERSHIP_ERR_DB;
    }
    if (already) {
        return PERSISTENCE_CORPUS_MEMBERSHIP_ERR_ALREADY_MEMBER;
    }

    const char *sql = "INSERT INTO corpus_members (corpus_id, piece_ref) VALUES (?, ?)";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return PERSISTENCE_CORPUS_MEMBERSHIP_ERR_DB;
    }
    sqlite3_bind_text(stmt, 1, corpus_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, piece_ref, -1, SQLITE_TRANSIENT);

    PersistenceCorpusMembershipResult result = PERSISTENCE_CORPUS_MEMBERSHIP_OK;
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        result = PERSISTENCE_CORPUS_MEMBERSHIP_ERR_DB;
    }

    sqlite3_finalize(stmt);
    return result;
}

PersistenceCorpusMembershipResult corpus_remove_piece_ref(PersistenceHandle *handle, const char *corpus_id,
                                                           const char *piece_ref) {
    if (handle == NULL || corpus_id == NULL || piece_ref == NULL) {
        return PERSISTENCE_CORPUS_MEMBERSHIP_ERR_NULL_ARG;
    }

    PersistenceCorpusMembershipResult schema_result = ensure_schema(handle);
    if (schema_result != PERSISTENCE_CORPUS_MEMBERSHIP_OK) {
        return schema_result;
    }

    sqlite3 *db = persistence_db(handle);

    int already = is_member(db, corpus_id, piece_ref);
    if (already < 0) {
        return PERSISTENCE_CORPUS_MEMBERSHIP_ERR_DB;
    }
    if (!already) {
        return PERSISTENCE_CORPUS_MEMBERSHIP_ERR_NOT_MEMBER;
    }

    const char *sql = "DELETE FROM corpus_members WHERE corpus_id = ? AND piece_ref = ?";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return PERSISTENCE_CORPUS_MEMBERSHIP_ERR_DB;
    }
    sqlite3_bind_text(stmt, 1, corpus_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, piece_ref, -1, SQLITE_TRANSIENT);

    PersistenceCorpusMembershipResult result = PERSISTENCE_CORPUS_MEMBERSHIP_OK;
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        result = PERSISTENCE_CORPUS_MEMBERSHIP_ERR_DB;
    }

    sqlite3_finalize(stmt);
    return result;
}

static char *copy_string(const char *value) {
    size_t len = strlen(value);
    char *copy = (char *)malloc(len + 1);
    if (copy != NULL) {
        memcpy(copy, value, len + 1);
    }
    return copy;
}

PersistenceCorpusMembershipResult corpus_list_piece_refs(PersistenceHandle *handle, const char *corpus_id,
                                                          char ***out_refs, size_t *out_count) {
    if (handle == NULL || corpus_id == NULL || out_refs == NULL || out_count == NULL) {
        return PERSISTENCE_CORPUS_MEMBERSHIP_ERR_NULL_ARG;
    }

    PersistenceCorpusMembershipResult schema_result = ensure_schema(handle);
    if (schema_result != PERSISTENCE_CORPUS_MEMBERSHIP_OK) {
        return schema_result;
    }

    sqlite3 *db = persistence_db(handle);
    const char *sql = "SELECT piece_ref FROM corpus_members WHERE corpus_id = ?";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return PERSISTENCE_CORPUS_MEMBERSHIP_ERR_DB;
    }
    sqlite3_bind_text(stmt, 1, corpus_id, -1, SQLITE_TRANSIENT);

    char **refs = NULL;
    size_t count = 0;
    int step_rc;
    while ((step_rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const unsigned char *ref_text = sqlite3_column_text(stmt, 0);
        char *copied = copy_string((const char *)ref_text);
        if (copied == NULL) {
            for (size_t i = 0; i < count; i++) {
                free(refs[i]);
            }
            free(refs);
            sqlite3_finalize(stmt);
            return PERSISTENCE_CORPUS_MEMBERSHIP_ERR_ALLOC;
        }

        char **grown = (char **)realloc(refs, sizeof(char *) * (count + 1));
        if (grown == NULL) {
            free(copied);
            for (size_t i = 0; i < count; i++) {
                free(refs[i]);
            }
            free(refs);
            sqlite3_finalize(stmt);
            return PERSISTENCE_CORPUS_MEMBERSHIP_ERR_ALLOC;
        }
        refs = grown;
        refs[count] = copied;
        count++;
    }
    sqlite3_finalize(stmt);

    if (step_rc != SQLITE_DONE) {
        for (size_t i = 0; i < count; i++) {
            free(refs[i]);
        }
        free(refs);
        return PERSISTENCE_CORPUS_MEMBERSHIP_ERR_DB;
    }

    *out_refs = refs;
    *out_count = count;
    return PERSISTENCE_CORPUS_MEMBERSHIP_OK;
}
