#include "persistence/persistence_support.h"

#include <stdio.h>
#include <stdlib.h>

bool persistence_savepoint_begin(sqlite3 *db) {
    return sqlite3_exec(db, "SAVEPOINT ursatz_store", NULL, NULL, NULL) == SQLITE_OK;
}

bool persistence_savepoint_release(sqlite3 *db) {
    return sqlite3_exec(db, "RELEASE ursatz_store", NULL, NULL, NULL) == SQLITE_OK;
}

void persistence_savepoint_rollback(sqlite3 *db) {
    sqlite3_exec(db, "ROLLBACK TO ursatz_store", NULL, NULL, NULL);
    sqlite3_exec(db, "RELEASE ursatz_store", NULL, NULL, NULL);
}

bool persistence_support_id_exists(sqlite3 *db, const char *table, const char *id, bool *failed) {
    char sql[128];
    snprintf(sql, sizeof(sql), "SELECT 1 FROM %s WHERE id = ?", table);
    sqlite3_stmt *stmt = NULL;
    *failed = false;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        *failed = true;
        return false;
    }
    sqlite3_bind_text(stmt, 1, id, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_ROW && rc != SQLITE_DONE) {
        *failed = true;
        return false;
    }
    return rc == SQLITE_ROW;
}

bool persistence_support_store_id_list(sqlite3 *db, const char *child_table, const char *parent_id, const void *owner,
                                       size_t count, PersistenceIdAt at) {
    char sql[160];
    snprintf(sql, sizeof(sql), "INSERT INTO %s (parent_id, position, event_id) VALUES (?, ?, ?)", child_table);
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return false;
    }
    bool ok = true;
    for (size_t i = 0; i < count && ok; i++) {
        sqlite3_reset(stmt);
        sqlite3_bind_text(stmt, 1, parent_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 2, (sqlite3_int64)i);
        sqlite3_bind_text(stmt, 3, entity_id_value(at(owner, i)), -1, SQLITE_TRANSIENT);
        ok = sqlite3_step(stmt) == SQLITE_DONE;
    }
    sqlite3_finalize(stmt);
    return ok;
}

PersistenceSupportResult persistence_support_load_id_list(sqlite3 *db, const char *child_table, const char *parent_id,
                                                          EntityID ***out_ids, size_t *out_count) {
    char sql[192];
    snprintf(sql, sizeof(sql), "SELECT event_id FROM %s WHERE parent_id = ? ORDER BY position", child_table);
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return PERSISTENCE_SUPPORT_ERR_DB;
    }
    sqlite3_bind_text(stmt, 1, parent_id, -1, SQLITE_TRANSIENT);

    EntityID **ids = NULL;
    size_t count = 0;
    size_t capacity = 0;
    PersistenceSupportResult result = PERSISTENCE_SUPPORT_OK;
    int rc;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (count == capacity) {
            size_t grown = capacity == 0 ? 8 : capacity * 2;
            EntityID **bigger = (EntityID **)realloc(ids, grown * sizeof(EntityID *));
            if (bigger == NULL) {
                result = PERSISTENCE_SUPPORT_ERR_ALLOC;
                break;
            }
            ids = bigger;
            capacity = grown;
        }
        if (entity_id_create((const char *)sqlite3_column_text(stmt, 0), &ids[count]) != ENTITY_ID_OK) {
            result = PERSISTENCE_SUPPORT_ERR_ALLOC;
            break;
        }
        count++;
    }
    if (result == PERSISTENCE_SUPPORT_OK && rc != SQLITE_DONE) {
        result = PERSISTENCE_SUPPORT_ERR_DB;
    }
    sqlite3_finalize(stmt);

    if (result != PERSISTENCE_SUPPORT_OK) {
        for (size_t i = 0; i < count; i++) {
            entity_id_destroy(ids[i]);
        }
        free(ids);
        return result;
    }
    *out_ids = ids;
    *out_count = count;
    return PERSISTENCE_SUPPORT_OK;
}

void persistence_support_bind_value(sqlite3_stmt *stmt, int first, const Value *value) {
    if (value == NULL) {
        sqlite3_bind_null(stmt, first);
        sqlite3_bind_null(stmt, first + 1);
        sqlite3_bind_null(stmt, first + 2);
        return;
    }
    size_t size = 0;
    const void *content = value_content(value, &size);
    sqlite3_bind_text(stmt, first, value_type(value), -1, SQLITE_TRANSIENT);
    if (content != NULL) {
        sqlite3_bind_blob(stmt, first + 1, content, (int)size, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt, first + 1);
    }
    sqlite3_bind_int64(stmt, first + 2, (sqlite3_int64)size);
}

PersistenceSupportResult persistence_support_column_value(sqlite3_stmt *stmt, int first, Value **out) {
    const unsigned char *type = sqlite3_column_text(stmt, first);
    if (type == NULL) {
        *out = NULL;
        return PERSISTENCE_SUPPORT_OK;
    }
    const void *content = sqlite3_column_blob(stmt, first + 1);
    sqlite3_int64 size = sqlite3_column_int64(stmt, first + 2);
    Value *value = NULL;
    if (value_create((const char *)type, content, (size_t)size, &value) != VALUE_OK) {
        return PERSISTENCE_SUPPORT_ERR_ALLOC;
    }
    *out = value;
    return PERSISTENCE_SUPPORT_OK;
}

void persistence_support_bind_optional_id(sqlite3_stmt *stmt, int index, const EntityID *id) {
    if (id == NULL) {
        sqlite3_bind_null(stmt, index);
    } else {
        sqlite3_bind_text(stmt, index, entity_id_value(id), -1, SQLITE_TRANSIENT);
    }
}

PersistenceSupportResult persistence_support_column_id(sqlite3_stmt *stmt, int column, EntityID **out) {
    const unsigned char *text = sqlite3_column_text(stmt, column);
    if (text == NULL) {
        *out = NULL;
        return PERSISTENCE_SUPPORT_OK;
    }
    EntityID *id = NULL;
    if (entity_id_create((const char *)text, &id) != ENTITY_ID_OK) {
        return PERSISTENCE_SUPPORT_ERR_ALLOC;
    }
    *out = id;
    return PERSISTENCE_SUPPORT_OK;
}
