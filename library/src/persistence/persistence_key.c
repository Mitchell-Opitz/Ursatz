#include "persistence/persistence_key.h"

#include <stdio.h>

#include "identity/entity_id.h"
#include "persistence/persistence_interpretation.h"

PersistenceKeyResult persistence_store_key(PersistenceHandle *handle, const Key *key) {
    if (handle == NULL || key == NULL) {
        return PERSISTENCE_KEY_ERR_NULL_ARG;
    }

    const Interpretation *interpretation = key_interpretation(key);
    if (persistence_store_interpretation(handle, interpretation) != PERSISTENCE_INTERPRETATION_OK) {
        return PERSISTENCE_KEY_ERR_DB;
    }

    const char *id_value = entity_id_value(interpretation_id(interpretation));

    sqlite3 *db = persistence_db(handle);
    const char *sql = "INSERT INTO keys (id, interpretation_id, label) VALUES (?, ?, ?)";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return PERSISTENCE_KEY_ERR_DB;
    }

    sqlite3_bind_text(stmt, 1, id_value, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, id_value, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, key_label(key), -1, SQLITE_TRANSIENT);

    PersistenceKeyResult result = PERSISTENCE_KEY_OK;
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        result = PERSISTENCE_KEY_ERR_DB;
    }

    sqlite3_finalize(stmt);
    return result;
}

PersistenceKeyResult persistence_load_key(PersistenceHandle *handle, const char *key_id, Key **out_key) {
    if (handle == NULL || key_id == NULL || out_key == NULL) {
        return PERSISTENCE_KEY_ERR_NULL_ARG;
    }

    sqlite3 *db = persistence_db(handle);
    const char *sql = "SELECT label FROM keys WHERE id = ?";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return PERSISTENCE_KEY_ERR_DB;
    }
    sqlite3_bind_text(stmt, 1, key_id, -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return PERSISTENCE_KEY_ERR_DB;
    }
    const unsigned char *label_text = sqlite3_column_text(stmt, 0);
    char label[512];
    snprintf(label, sizeof(label), "%s", (const char *)label_text);
    sqlite3_finalize(stmt);

    Interpretation *interpretation = NULL;
    if (persistence_load_interpretation(handle, key_id, &interpretation) != PERSISTENCE_INTERPRETATION_OK) {
        return PERSISTENCE_KEY_ERR_DB;
    }

    Key *key = NULL;
    if (key_create(interpretation, label, &key) != KEY_OK) {
        interpretation_destroy(interpretation);
        return PERSISTENCE_KEY_ERR_DB;
    }

    *out_key = key;
    return PERSISTENCE_KEY_OK;
}
