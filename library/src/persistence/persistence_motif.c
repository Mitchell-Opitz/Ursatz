#include "persistence/persistence_motif.h"

#include <stdlib.h>

#include "identity/entity_id.h"
#include "persistence/persistence_support.h"

static const EntityID *event_id_of(const void *owner, size_t index) {
    return motif_event_id_at((const Motif *)owner, index);
}

PersistenceMotifResult persistence_store_motif(PersistenceHandle *handle, const Motif *motif) {
    if (handle == NULL || motif == NULL) {
        return PERSISTENCE_MOTIF_ERR_NULL_ARG;
    }

    sqlite3 *db = persistence_db(handle);
    const char *id_value = entity_id_value(motif_id(motif));
    bool failed = false;
    if (persistence_support_id_exists(db, "motifs", id_value, &failed)) {
        return PERSISTENCE_MOTIF_ERR_DUPLICATE_ID;
    }
    if (failed || !persistence_savepoint_begin(db)) {
        return PERSISTENCE_MOTIF_ERR_DB;
    }

    sqlite3_stmt *stmt = NULL;
    bool ok = sqlite3_prepare_v2(db, "INSERT INTO motifs (id) VALUES (?)", -1, &stmt, NULL) == SQLITE_OK;
    if (ok) {
        sqlite3_bind_text(stmt, 1, id_value, -1, SQLITE_TRANSIENT);
        ok = sqlite3_step(stmt) == SQLITE_DONE;
    }
    sqlite3_finalize(stmt);
    ok = ok && persistence_support_store_id_list(db, "motif_events", id_value, motif, motif_event_id_count(motif),
                                                 event_id_of);
    if (!ok || !persistence_savepoint_release(db)) {
        persistence_savepoint_rollback(db);
        return PERSISTENCE_MOTIF_ERR_DB;
    }
    return PERSISTENCE_MOTIF_OK;
}

PersistenceMotifResult persistence_load_motif(PersistenceHandle *handle, const char *motif_id_value,
                                              Motif **out_motif) {
    if (handle == NULL || motif_id_value == NULL || out_motif == NULL) {
        return PERSISTENCE_MOTIF_ERR_NULL_ARG;
    }

    sqlite3 *db = persistence_db(handle);
    bool failed = false;
    if (!persistence_support_id_exists(db, "motifs", motif_id_value, &failed)) {
        return failed ? PERSISTENCE_MOTIF_ERR_DB : PERSISTENCE_MOTIF_ERR_NOT_FOUND;
    }

    EntityID **event_ids = NULL;
    size_t event_count = 0;
    PersistenceSupportResult listed =
        persistence_support_load_id_list(db, "motif_events", motif_id_value, &event_ids, &event_count);
    if (listed != PERSISTENCE_SUPPORT_OK) {
        return listed == PERSISTENCE_SUPPORT_ERR_ALLOC ? PERSISTENCE_MOTIF_ERR_ALLOC : PERSISTENCE_MOTIF_ERR_DB;
    }

    EntityID *id = NULL;
    Motif *motif = NULL;
    if (event_count == 0 || entity_id_create(motif_id_value, &id) != ENTITY_ID_OK ||
        motif_create(id, event_ids, event_count, &motif) != MOTIF_OK) {
        PersistenceMotifResult result = event_count == 0 ? PERSISTENCE_MOTIF_ERR_DB : PERSISTENCE_MOTIF_ERR_ALLOC;
        entity_id_destroy(id);
        for (size_t i = 0; i < event_count; i++) {
            entity_id_destroy(event_ids[i]);
        }
        free(event_ids);
        return result;
    }
    *out_motif = motif;
    return PERSISTENCE_MOTIF_OK;
}
