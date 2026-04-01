#include "persistence/persistence_relationship.h"

#include <stdio.h>
#include <stdlib.h>

#include "core/provenance_reference.h"
#include "core/value.h"
#include "identity/entity_id.h"
#include "persistence/persistence_support.h"

#define RELATIONSHIP_COLUMNS                                                                                         \
    "id, source_id, predicate_name, target_id, scope_type, scope_content, scope_content_size, context_id, "        \
    "qualifiers_type, qualifiers_content, qualifiers_content_size, evidence_id, uncertainty_type, "                 \
    "uncertainty_content, uncertainty_content_size, provenance_state, provenance_id"

PersistenceRelationshipResult persistence_store_relationship(PersistenceHandle *handle,
                                                             const Relationship *relationship) {
    if (handle == NULL || relationship == NULL) {
        return PERSISTENCE_RELATIONSHIP_ERR_NULL_ARG;
    }

    sqlite3 *db = persistence_db(handle);
    const char *id_value = entity_id_value(relationship_id(relationship));
    bool failed = false;
    if (persistence_support_id_exists(db, "relationships", id_value, &failed)) {
        return PERSISTENCE_RELATIONSHIP_ERR_DUPLICATE_ID;
    }
    if (failed) {
        return PERSISTENCE_RELATIONSHIP_ERR_DB;
    }

    const char *sql = "INSERT INTO relationships (" RELATIONSHIP_COLUMNS ") "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return PERSISTENCE_RELATIONSHIP_ERR_DB;
    }

    const ProvenanceReference *provenance = relationship_provenance(relationship);
    sqlite3_bind_text(stmt, 1, id_value, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, entity_id_value(relationship_source_id(relationship)), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, relationship_predicate_name(relationship), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, entity_id_value(relationship_target_id(relationship)), -1, SQLITE_TRANSIENT);
    persistence_support_bind_value(stmt, 5, relationship_scope(relationship));
    persistence_support_bind_optional_id(stmt, 8, relationship_context_id(relationship));
    persistence_support_bind_value(stmt, 9, relationship_qualifiers(relationship));
    persistence_support_bind_optional_id(stmt, 12, relationship_evidence_id(relationship));
    persistence_support_bind_value(stmt, 13, relationship_uncertainty(relationship));
    sqlite3_bind_int(stmt, 16, (int)provenance_reference_state(provenance));
    if (provenance_reference_state(provenance) == PROVENANCE_REFERENCE_PRESENT) {
        sqlite3_bind_text(stmt, 17, entity_id_value(provenance_reference_target_id(provenance)), -1,
                          SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt, 17);
    }

    PersistenceRelationshipResult result = PERSISTENCE_RELATIONSHIP_OK;
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        result = PERSISTENCE_RELATIONSHIP_ERR_DB;
    }
    sqlite3_finalize(stmt);
    return result;
}

/* Builds a Relationship from the current row of a statement that selected RELATIONSHIP_COLUMNS. */
static PersistenceRelationshipResult relationship_from_row(sqlite3_stmt *stmt, Relationship **out) {
    EntityID *id = NULL;
    EntityID *source_id = NULL;
    EntityID *target_id = NULL;
    Value *scope = NULL;
    EntityID *context_id = NULL;
    Value *qualifiers = NULL;
    EntityID *evidence_id = NULL;
    Value *uncertainty = NULL;
    ProvenanceReference *provenance = NULL;
    Relationship *relationship = NULL;
    PersistenceRelationshipResult result = PERSISTENCE_RELATIONSHIP_ERR_ALLOC;

    if (persistence_support_column_id(stmt, 0, &id) != PERSISTENCE_SUPPORT_OK ||
        persistence_support_column_id(stmt, 1, &source_id) != PERSISTENCE_SUPPORT_OK ||
        persistence_support_column_id(stmt, 3, &target_id) != PERSISTENCE_SUPPORT_OK ||
        persistence_support_column_value(stmt, 4, &scope) != PERSISTENCE_SUPPORT_OK ||
        persistence_support_column_id(stmt, 7, &context_id) != PERSISTENCE_SUPPORT_OK ||
        persistence_support_column_value(stmt, 8, &qualifiers) != PERSISTENCE_SUPPORT_OK ||
        persistence_support_column_id(stmt, 11, &evidence_id) != PERSISTENCE_SUPPORT_OK ||
        persistence_support_column_value(stmt, 12, &uncertainty) != PERSISTENCE_SUPPORT_OK) {
        goto done;
    }
    if (sqlite3_column_int(stmt, 15) == PROVENANCE_REFERENCE_PRESENT) {
        EntityID *provenance_id = NULL;
        if (persistence_support_column_id(stmt, 16, &provenance_id) != PERSISTENCE_SUPPORT_OK ||
            provenance_id == NULL) {
            result = PERSISTENCE_RELATIONSHIP_ERR_DB;
            goto done;
        }
        if (provenance_reference_create_present(provenance_id, &provenance) != PROVENANCE_REFERENCE_OK) {
            entity_id_destroy(provenance_id);
            goto done;
        }
    } else if (provenance_reference_create_absent(&provenance) != PROVENANCE_REFERENCE_OK) {
        goto done;
    }

    if (relationship_create(id, source_id, (const char *)sqlite3_column_text(stmt, 2), target_id, scope, context_id,
                            qualifiers, evidence_id, uncertainty, provenance, &relationship) != RELATIONSHIP_OK) {
        goto done;
    }
    *out = relationship;
    return PERSISTENCE_RELATIONSHIP_OK;

done:
    entity_id_destroy(id);
    entity_id_destroy(source_id);
    entity_id_destroy(target_id);
    if (scope != NULL) {
        value_destroy(scope);
    }
    entity_id_destroy(context_id);
    if (qualifiers != NULL) {
        value_destroy(qualifiers);
    }
    entity_id_destroy(evidence_id);
    if (uncertainty != NULL) {
        value_destroy(uncertainty);
    }
    if (provenance != NULL) {
        provenance_reference_destroy(provenance);
    }
    return result;
}

PersistenceRelationshipResult persistence_load_relationship(PersistenceHandle *handle, const char *relationship_id,
                                                            Relationship **out_relationship) {
    if (handle == NULL || relationship_id == NULL || out_relationship == NULL) {
        return PERSISTENCE_RELATIONSHIP_ERR_NULL_ARG;
    }

    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT " RELATIONSHIP_COLUMNS " FROM relationships WHERE id = ?";
    if (sqlite3_prepare_v2(persistence_db(handle), sql, -1, &stmt, NULL) != SQLITE_OK) {
        return PERSISTENCE_RELATIONSHIP_ERR_DB;
    }
    sqlite3_bind_text(stmt, 1, relationship_id, -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    PersistenceRelationshipResult result;
    if (rc == SQLITE_ROW) {
        result = relationship_from_row(stmt, out_relationship);
    } else if (rc == SQLITE_DONE) {
        result = PERSISTENCE_RELATIONSHIP_ERR_NOT_FOUND;
    } else {
        result = PERSISTENCE_RELATIONSHIP_ERR_DB;
    }
    sqlite3_finalize(stmt);
    return result;
}

/* Every Relationship whose `column` equals value, in insertion (rowid) order. */
static PersistenceRelationshipResult list_where(PersistenceHandle *handle, const char *column, const char *value,
                                                Relationship ***out_relationships, size_t *out_count) {
    if (handle == NULL || value == NULL || out_relationships == NULL || out_count == NULL) {
        return PERSISTENCE_RELATIONSHIP_ERR_NULL_ARG;
    }

    char sql[256];
    snprintf(sql, sizeof(sql), "SELECT " RELATIONSHIP_COLUMNS " FROM relationships WHERE %s = ? ORDER BY rowid",
             column);
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(persistence_db(handle), sql, -1, &stmt, NULL) != SQLITE_OK) {
        return PERSISTENCE_RELATIONSHIP_ERR_DB;
    }
    sqlite3_bind_text(stmt, 1, value, -1, SQLITE_TRANSIENT);

    Relationship **items = NULL;
    size_t count = 0;
    size_t capacity = 0;
    PersistenceRelationshipResult result = PERSISTENCE_RELATIONSHIP_OK;
    int rc;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (count == capacity) {
            size_t grown = capacity == 0 ? 8 : capacity * 2;
            Relationship **bigger = (Relationship **)realloc(items, grown * sizeof(Relationship *));
            if (bigger == NULL) {
                result = PERSISTENCE_RELATIONSHIP_ERR_ALLOC;
                break;
            }
            items = bigger;
            capacity = grown;
        }
        result = relationship_from_row(stmt, &items[count]);
        if (result != PERSISTENCE_RELATIONSHIP_OK) {
            break;
        }
        count++;
    }
    if (result == PERSISTENCE_RELATIONSHIP_OK && rc != SQLITE_DONE) {
        result = PERSISTENCE_RELATIONSHIP_ERR_DB;
    }
    sqlite3_finalize(stmt);

    if (result != PERSISTENCE_RELATIONSHIP_OK) {
        for (size_t i = 0; i < count; i++) {
            relationship_destroy(items[i]);
        }
        free(items);
        return result;
    }
    *out_relationships = items;
    *out_count = count;
    return PERSISTENCE_RELATIONSHIP_OK;
}

PersistenceRelationshipResult persistence_list_relationships_by_source(PersistenceHandle *handle,
                                                                       const char *source_id,
                                                                       Relationship ***out_relationships,
                                                                       size_t *out_count) {
    return list_where(handle, "source_id", source_id, out_relationships, out_count);
}

PersistenceRelationshipResult persistence_list_relationships_by_target(PersistenceHandle *handle,
                                                                       const char *target_id,
                                                                       Relationship ***out_relationships,
                                                                       size_t *out_count) {
    return list_where(handle, "target_id", target_id, out_relationships, out_count);
}
