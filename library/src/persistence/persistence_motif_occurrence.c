#include "persistence/persistence_motif_occurrence.h"

#include <stdlib.h>
#include <string.h>

#include "core/occurrence.h"
#include "identity/entity_id.h"
#include "persistence/persistence_support.h"

static const EntityID *event_id_of(const void *owner, size_t index) {
    return motif_occurrence_region_event_id_at((const MotifOccurrence *)owner, index);
}

PersistenceMotifOccurrenceResult persistence_store_motif_occurrence(PersistenceHandle *handle,
                                                          const MotifOccurrence *motif_occurrence) {
    if (handle == NULL || motif_occurrence == NULL) {
        return PERSISTENCE_MOTIF_OCCURRENCE_ERR_NULL_ARG;
    }

    sqlite3 *db = persistence_db(handle);
    const char *id_value = entity_id_value(motif_occurrence_id(motif_occurrence));
    bool failed = false;
    if (persistence_support_id_exists(db, "motif_occurrences", id_value, &failed)) {
        return PERSISTENCE_MOTIF_OCCURRENCE_ERR_DUPLICATE_ID;
    }
    if (failed || !persistence_savepoint_begin(db)) {
        return PERSISTENCE_MOTIF_OCCURRENCE_ERR_DB;
    }

    sqlite3_stmt *stmt = NULL;
    bool ok = sqlite3_prepare_v2(db, "INSERT INTO motif_occurrences (id, motif_id) VALUES (?, ?)", -1, &stmt, NULL) ==
              SQLITE_OK;
    if (ok) {
        sqlite3_bind_text(stmt, 1, id_value, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, entity_id_value(motif_occurrence_motif_id(motif_occurrence)), -1, SQLITE_TRANSIENT);
        ok = sqlite3_step(stmt) == SQLITE_DONE;
    }
    sqlite3_finalize(stmt);
    ok = ok && persistence_support_store_id_list(db, "motif_occurrence_events", id_value, motif_occurrence,
                                                 motif_occurrence_region_event_id_count(motif_occurrence), event_id_of);
    if (!ok || !persistence_savepoint_release(db)) {
        persistence_savepoint_rollback(db);
        return PERSISTENCE_MOTIF_OCCURRENCE_ERR_DB;
    }
    return PERSISTENCE_MOTIF_OCCURRENCE_OK;
}

PersistenceMotifOccurrenceResult persistence_load_motif_occurrence(PersistenceHandle *handle, const char *motif_occurrence_id_value,
                                                         MotifOccurrence **out_motif_occurrence) {
    if (handle == NULL || motif_occurrence_id_value == NULL || out_motif_occurrence == NULL) {
        return PERSISTENCE_MOTIF_OCCURRENCE_ERR_NULL_ARG;
    }

    sqlite3 *db = persistence_db(handle);
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT motif_id FROM motif_occurrences WHERE id = ?", -1, &stmt, NULL) != SQLITE_OK) {
        return PERSISTENCE_MOTIF_OCCURRENCE_ERR_DB;
    }
    sqlite3_bind_text(stmt, 1, motif_occurrence_id_value, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return rc == SQLITE_DONE ? PERSISTENCE_MOTIF_OCCURRENCE_ERR_NOT_FOUND : PERSISTENCE_MOTIF_OCCURRENCE_ERR_DB;
    }
    EntityID *motif_id = NULL;
    PersistenceSupportResult read = persistence_support_column_id(stmt, 0, &motif_id);
    sqlite3_finalize(stmt);
    if (read != PERSISTENCE_SUPPORT_OK || motif_id == NULL) {
        entity_id_destroy(motif_id);
        return PERSISTENCE_MOTIF_OCCURRENCE_ERR_DB;
    }

    EntityID **region_ids = NULL;
    size_t region_count = 0;
    PersistenceSupportResult listed =
        persistence_support_load_id_list(db, "motif_occurrence_events", motif_occurrence_id_value, &region_ids, &region_count);
    if (listed != PERSISTENCE_SUPPORT_OK) {
        entity_id_destroy(motif_id);
        return listed == PERSISTENCE_SUPPORT_ERR_ALLOC ? PERSISTENCE_MOTIF_OCCURRENCE_ERR_ALLOC : PERSISTENCE_MOTIF_OCCURRENCE_ERR_DB;
    }

    EntityID *id = NULL;
    Occurrence *occurrence = NULL;
    MotifOccurrence *motif_occurrence = NULL;
    PersistenceMotifOccurrenceResult result;
    if (region_count == 0 || entity_id_create(motif_occurrence_id_value, &id) != ENTITY_ID_OK ||
        occurrence_create(id, motif_id, &occurrence) != OCCURRENCE_OK) {
        result = region_count == 0 ? PERSISTENCE_MOTIF_OCCURRENCE_ERR_DB : PERSISTENCE_MOTIF_OCCURRENCE_ERR_ALLOC;
        entity_id_destroy(id);
        entity_id_destroy(motif_id);
        for (size_t i = 0; i < region_count; i++) {
            entity_id_destroy(region_ids[i]);
        }
        free(region_ids);
        return result;
    }
    if (motif_occurrence_create(occurrence, region_ids, region_count, &motif_occurrence) != MOTIF_OCCURRENCE_OK) {
        occurrence_destroy(occurrence);
        for (size_t i = 0; i < region_count; i++) {
            entity_id_destroy(region_ids[i]);
        }
        free(region_ids);
        return PERSISTENCE_MOTIF_OCCURRENCE_ERR_ALLOC;
    }
    free(region_ids); /* the constructor copied the array and adopted the ids */
    *out_motif_occurrence = motif_occurrence;
    return PERSISTENCE_MOTIF_OCCURRENCE_OK;
}

PersistenceMotifOccurrenceResult persistence_list_motif_occurrences_by_motif_id(PersistenceHandle *handle, const char *motif_id,
                                                                  MotifOccurrence ***out_motif_occurrences,
                                                                  size_t *out_count) {
    if (handle == NULL || motif_id == NULL || out_motif_occurrences == NULL || out_count == NULL) {
        return PERSISTENCE_MOTIF_OCCURRENCE_ERR_NULL_ARG;
    }

    /* The ids first, so no statement is open while each occurrence is loaded. */
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(persistence_db(handle), "SELECT id FROM motif_occurrences WHERE motif_id = ? ORDER BY rowid", -1,
                           &stmt, NULL) != SQLITE_OK) {
        return PERSISTENCE_MOTIF_OCCURRENCE_ERR_DB;
    }
    sqlite3_bind_text(stmt, 1, motif_id, -1, SQLITE_TRANSIENT);
    char **ids = NULL;
    size_t count = 0;
    size_t capacity = 0;
    PersistenceMotifOccurrenceResult result = PERSISTENCE_MOTIF_OCCURRENCE_OK;
    int rc;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (count == capacity) {
            size_t grown = capacity == 0 ? 8 : capacity * 2;
            char **bigger = (char **)realloc(ids, grown * sizeof(char *));
            if (bigger == NULL) {
                result = PERSISTENCE_MOTIF_OCCURRENCE_ERR_ALLOC;
                break;
            }
            ids = bigger;
            capacity = grown;
        }
        const char *text = (const char *)sqlite3_column_text(stmt, 0);
        ids[count] = (char *)malloc(strlen(text) + 1);
        if (ids[count] == NULL) {
            result = PERSISTENCE_MOTIF_OCCURRENCE_ERR_ALLOC;
            break;
        }
        memcpy(ids[count], text, strlen(text) + 1);
        count++;
    }
    if (result == PERSISTENCE_MOTIF_OCCURRENCE_OK && rc != SQLITE_DONE) {
        result = PERSISTENCE_MOTIF_OCCURRENCE_ERR_DB;
    }
    sqlite3_finalize(stmt);

    MotifOccurrence **items = NULL;
    size_t loaded = 0;
    if (result == PERSISTENCE_MOTIF_OCCURRENCE_OK && count > 0) {
        items = (MotifOccurrence **)calloc(count, sizeof(MotifOccurrence *));
        if (items == NULL) {
            result = PERSISTENCE_MOTIF_OCCURRENCE_ERR_ALLOC;
        }
    }
    for (size_t i = 0; i < count && result == PERSISTENCE_MOTIF_OCCURRENCE_OK; i++) {
        result = persistence_load_motif_occurrence(handle, ids[i], &items[i]);
        if (result == PERSISTENCE_MOTIF_OCCURRENCE_OK) {
            loaded++;
        }
    }
    for (size_t i = 0; i < count; i++) {
        free(ids[i]);
    }
    free(ids);

    if (result != PERSISTENCE_MOTIF_OCCURRENCE_OK) {
        for (size_t i = 0; i < loaded; i++) {
            motif_occurrence_destroy(items[i]);
        }
        free(items);
        return result;
    }
    *out_motif_occurrences = items;
    *out_count = count;
    return PERSISTENCE_MOTIF_OCCURRENCE_OK;
}
