#include "persistence/persistence_phrase_occurrence.h"

#include <stdlib.h>
#include <string.h>

#include "core/occurrence.h"
#include "identity/entity_id.h"
#include "persistence/persistence_support.h"

static const EntityID *event_id_of(const void *owner, size_t index) {
    return phrase_occurrence_region_event_id_at((const PhraseOccurrence *)owner, index);
}

PersistencePhraseOccurrenceResult persistence_store_phrase_occurrence(PersistenceHandle *handle,
                                                          const PhraseOccurrence *phrase_occurrence) {
    if (handle == NULL || phrase_occurrence == NULL) {
        return PERSISTENCE_PHRASE_OCCURRENCE_ERR_NULL_ARG;
    }

    sqlite3 *db = persistence_db(handle);
    const char *id_value = entity_id_value(phrase_occurrence_id(phrase_occurrence));
    bool failed = false;
    if (persistence_support_id_exists(db, "phrase_occurrences", id_value, &failed)) {
        return PERSISTENCE_PHRASE_OCCURRENCE_ERR_DUPLICATE_ID;
    }
    if (failed || !persistence_savepoint_begin(db)) {
        return PERSISTENCE_PHRASE_OCCURRENCE_ERR_DB;
    }

    sqlite3_stmt *stmt = NULL;
    bool ok = sqlite3_prepare_v2(db, "INSERT INTO phrase_occurrences (id, phrase_id) VALUES (?, ?)", -1, &stmt, NULL) ==
              SQLITE_OK;
    if (ok) {
        sqlite3_bind_text(stmt, 1, id_value, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, entity_id_value(phrase_occurrence_phrase_id(phrase_occurrence)), -1, SQLITE_TRANSIENT);
        ok = sqlite3_step(stmt) == SQLITE_DONE;
    }
    sqlite3_finalize(stmt);
    ok = ok && persistence_support_store_id_list(db, "phrase_occurrence_events", id_value, phrase_occurrence,
                                                 phrase_occurrence_region_event_id_count(phrase_occurrence), event_id_of);
    if (!ok || !persistence_savepoint_release(db)) {
        persistence_savepoint_rollback(db);
        return PERSISTENCE_PHRASE_OCCURRENCE_ERR_DB;
    }
    return PERSISTENCE_PHRASE_OCCURRENCE_OK;
}

PersistencePhraseOccurrenceResult persistence_load_phrase_occurrence(PersistenceHandle *handle, const char *phrase_occurrence_id_value,
                                                         PhraseOccurrence **out_phrase_occurrence) {
    if (handle == NULL || phrase_occurrence_id_value == NULL || out_phrase_occurrence == NULL) {
        return PERSISTENCE_PHRASE_OCCURRENCE_ERR_NULL_ARG;
    }

    sqlite3 *db = persistence_db(handle);
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT phrase_id FROM phrase_occurrences WHERE id = ?", -1, &stmt, NULL) != SQLITE_OK) {
        return PERSISTENCE_PHRASE_OCCURRENCE_ERR_DB;
    }
    sqlite3_bind_text(stmt, 1, phrase_occurrence_id_value, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return rc == SQLITE_DONE ? PERSISTENCE_PHRASE_OCCURRENCE_ERR_NOT_FOUND : PERSISTENCE_PHRASE_OCCURRENCE_ERR_DB;
    }
    EntityID *phrase_id = NULL;
    PersistenceSupportResult read = persistence_support_column_id(stmt, 0, &phrase_id);
    sqlite3_finalize(stmt);
    if (read != PERSISTENCE_SUPPORT_OK || phrase_id == NULL) {
        entity_id_destroy(phrase_id);
        return PERSISTENCE_PHRASE_OCCURRENCE_ERR_DB;
    }

    EntityID **region_ids = NULL;
    size_t region_count = 0;
    PersistenceSupportResult listed =
        persistence_support_load_id_list(db, "phrase_occurrence_events", phrase_occurrence_id_value, &region_ids, &region_count);
    if (listed != PERSISTENCE_SUPPORT_OK) {
        entity_id_destroy(phrase_id);
        return listed == PERSISTENCE_SUPPORT_ERR_ALLOC ? PERSISTENCE_PHRASE_OCCURRENCE_ERR_ALLOC : PERSISTENCE_PHRASE_OCCURRENCE_ERR_DB;
    }

    EntityID *id = NULL;
    Occurrence *occurrence = NULL;
    PhraseOccurrence *phrase_occurrence = NULL;
    PersistencePhraseOccurrenceResult result;
    if (region_count == 0 || entity_id_create(phrase_occurrence_id_value, &id) != ENTITY_ID_OK ||
        occurrence_create(id, phrase_id, &occurrence) != OCCURRENCE_OK) {
        result = region_count == 0 ? PERSISTENCE_PHRASE_OCCURRENCE_ERR_DB : PERSISTENCE_PHRASE_OCCURRENCE_ERR_ALLOC;
        entity_id_destroy(id);
        entity_id_destroy(phrase_id);
        for (size_t i = 0; i < region_count; i++) {
            entity_id_destroy(region_ids[i]);
        }
        free(region_ids);
        return result;
    }
    if (phrase_occurrence_create(occurrence, region_ids, region_count, &phrase_occurrence) != PHRASE_OCCURRENCE_OK) {
        occurrence_destroy(occurrence);
        for (size_t i = 0; i < region_count; i++) {
            entity_id_destroy(region_ids[i]);
        }
        free(region_ids);
        return PERSISTENCE_PHRASE_OCCURRENCE_ERR_ALLOC;
    }
    free(region_ids); /* the constructor copied the array and adopted the ids */
    *out_phrase_occurrence = phrase_occurrence;
    return PERSISTENCE_PHRASE_OCCURRENCE_OK;
}

PersistencePhraseOccurrenceResult persistence_list_phrase_occurrences_by_phrase_id(PersistenceHandle *handle, const char *phrase_id,
                                                                  PhraseOccurrence ***out_phrase_occurrences,
                                                                  size_t *out_count) {
    if (handle == NULL || phrase_id == NULL || out_phrase_occurrences == NULL || out_count == NULL) {
        return PERSISTENCE_PHRASE_OCCURRENCE_ERR_NULL_ARG;
    }

    /* The ids first, so no statement is open while each occurrence is loaded. */
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(persistence_db(handle), "SELECT id FROM phrase_occurrences WHERE phrase_id = ? ORDER BY rowid", -1,
                           &stmt, NULL) != SQLITE_OK) {
        return PERSISTENCE_PHRASE_OCCURRENCE_ERR_DB;
    }
    sqlite3_bind_text(stmt, 1, phrase_id, -1, SQLITE_TRANSIENT);
    char **ids = NULL;
    size_t count = 0;
    size_t capacity = 0;
    PersistencePhraseOccurrenceResult result = PERSISTENCE_PHRASE_OCCURRENCE_OK;
    int rc;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (count == capacity) {
            size_t grown = capacity == 0 ? 8 : capacity * 2;
            char **bigger = (char **)realloc(ids, grown * sizeof(char *));
            if (bigger == NULL) {
                result = PERSISTENCE_PHRASE_OCCURRENCE_ERR_ALLOC;
                break;
            }
            ids = bigger;
            capacity = grown;
        }
        const char *text = (const char *)sqlite3_column_text(stmt, 0);
        ids[count] = (char *)malloc(strlen(text) + 1);
        if (ids[count] == NULL) {
            result = PERSISTENCE_PHRASE_OCCURRENCE_ERR_ALLOC;
            break;
        }
        memcpy(ids[count], text, strlen(text) + 1);
        count++;
    }
    if (result == PERSISTENCE_PHRASE_OCCURRENCE_OK && rc != SQLITE_DONE) {
        result = PERSISTENCE_PHRASE_OCCURRENCE_ERR_DB;
    }
    sqlite3_finalize(stmt);

    PhraseOccurrence **items = NULL;
    size_t loaded = 0;
    if (result == PERSISTENCE_PHRASE_OCCURRENCE_OK && count > 0) {
        items = (PhraseOccurrence **)calloc(count, sizeof(PhraseOccurrence *));
        if (items == NULL) {
            result = PERSISTENCE_PHRASE_OCCURRENCE_ERR_ALLOC;
        }
    }
    for (size_t i = 0; i < count && result == PERSISTENCE_PHRASE_OCCURRENCE_OK; i++) {
        result = persistence_load_phrase_occurrence(handle, ids[i], &items[i]);
        if (result == PERSISTENCE_PHRASE_OCCURRENCE_OK) {
            loaded++;
        }
    }
    for (size_t i = 0; i < count; i++) {
        free(ids[i]);
    }
    free(ids);

    if (result != PERSISTENCE_PHRASE_OCCURRENCE_OK) {
        for (size_t i = 0; i < loaded; i++) {
            phrase_occurrence_destroy(items[i]);
        }
        free(items);
        return result;
    }
    *out_phrase_occurrences = items;
    *out_count = count;
    return PERSISTENCE_PHRASE_OCCURRENCE_OK;
}
