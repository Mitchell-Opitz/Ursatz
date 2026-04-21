#include "persistence/persistence_cadence.h"

#include <stdlib.h>

#include "core/value.h"
#include "identity/entity_id.h"
#include "persistence/persistence_support.h"

static const EntityID *event_id_of(const void *owner, size_t index) {
    return cadence_event_id_at((const Cadence *)owner, index);
}

PersistenceCadenceResult persistence_store_cadence(PersistenceHandle *handle, const Cadence *cadence) {
    if (handle == NULL || cadence == NULL) {
        return PERSISTENCE_CADENCE_ERR_NULL_ARG;
    }

    sqlite3 *db = persistence_db(handle);
    const char *id_value = entity_id_value(cadence_id(cadence));
    bool failed = false;
    if (persistence_support_id_exists(db, "cadences", id_value, &failed)) {
        return PERSISTENCE_CADENCE_ERR_DUPLICATE_ID;
    }
    if (failed || !persistence_savepoint_begin(db)) {
        return PERSISTENCE_CADENCE_ERR_DB;
    }

    sqlite3_stmt *stmt = NULL;
    bool ok = sqlite3_prepare_v2(db,
                                 "INSERT INTO cadences (id, harmonic_context_type, harmonic_context_content, "
                                 "harmonic_context_content_size) VALUES (?, ?, ?, ?)",
                                 -1, &stmt, NULL) == SQLITE_OK;
    if (ok) {
        sqlite3_bind_text(stmt, 1, id_value, -1, SQLITE_TRANSIENT);
        persistence_support_bind_value(stmt, 2, cadence_optional_harmonic_context(cadence));
        ok = sqlite3_step(stmt) == SQLITE_DONE;
    }
    sqlite3_finalize(stmt);
    ok = ok && persistence_support_store_id_list(db, "cadence_events", id_value, cadence,
                                                 cadence_event_id_count(cadence), event_id_of);
    if (!ok || !persistence_savepoint_release(db)) {
        persistence_savepoint_rollback(db);
        return PERSISTENCE_CADENCE_ERR_DB;
    }
    return PERSISTENCE_CADENCE_OK;
}

PersistenceCadenceResult persistence_load_cadence(PersistenceHandle *handle, const char *cadence_id_value,
                                                  Cadence **out_cadence) {
    if (handle == NULL || cadence_id_value == NULL || out_cadence == NULL) {
        return PERSISTENCE_CADENCE_ERR_NULL_ARG;
    }

    sqlite3 *db = persistence_db(handle);
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db,
                           "SELECT harmonic_context_type, harmonic_context_content, harmonic_context_content_size "
                           "FROM cadences WHERE id = ?",
                           -1, &stmt, NULL) != SQLITE_OK) {
        return PERSISTENCE_CADENCE_ERR_DB;
    }
    sqlite3_bind_text(stmt, 1, cadence_id_value, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return rc == SQLITE_DONE ? PERSISTENCE_CADENCE_ERR_NOT_FOUND : PERSISTENCE_CADENCE_ERR_DB;
    }
    Value *context = NULL;
    PersistenceSupportResult read = persistence_support_column_value(stmt, 0, &context);
    sqlite3_finalize(stmt);
    if (read != PERSISTENCE_SUPPORT_OK) {
        return PERSISTENCE_CADENCE_ERR_ALLOC;
    }

    EntityID **event_ids = NULL;
    size_t event_count = 0;
    PersistenceSupportResult listed =
        persistence_support_load_id_list(db, "cadence_events", cadence_id_value, &event_ids, &event_count);
    if (listed != PERSISTENCE_SUPPORT_OK) {
        if (context != NULL) {
            value_destroy(context);
        }
        return listed == PERSISTENCE_SUPPORT_ERR_ALLOC ? PERSISTENCE_CADENCE_ERR_ALLOC : PERSISTENCE_CADENCE_ERR_DB;
    }

    EntityID *id = NULL;
    Cadence *cadence = NULL;
    if (event_count == 0 || entity_id_create(cadence_id_value, &id) != ENTITY_ID_OK ||
        cadence_create(id, event_ids, event_count, context, &cadence) != CADENCE_OK) {
        PersistenceCadenceResult result = event_count == 0 ? PERSISTENCE_CADENCE_ERR_DB : PERSISTENCE_CADENCE_ERR_ALLOC;
        entity_id_destroy(id);
        if (context != NULL) {
            value_destroy(context);
        }
        for (size_t i = 0; i < event_count; i++) {
            entity_id_destroy(event_ids[i]);
        }
        free(event_ids);
        return result;
    }
    free(event_ids); /* cadence_create copied the array and adopted the ids */
    *out_cadence = cadence;
    return PERSISTENCE_CADENCE_OK;
}
