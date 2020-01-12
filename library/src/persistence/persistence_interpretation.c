#include "persistence/persistence_interpretation.h"

#include <stdlib.h>

#include "core/framework_reference.h"
#include "core/provenance_reference.h"
#include "core/value.h"
#include "identity/entity_id.h"
#include "persistence/persistence_uncertainty.h"

PersistenceInterpretationResult persistence_store_interpretation(PersistenceHandle *handle,
                                                                  const Interpretation *interpretation) {
    if (handle == NULL || interpretation == NULL) {
        return PERSISTENCE_INTERPRETATION_ERR_NULL_ARG;
    }

    sqlite3_int64 uncertainty_id = 0;
    if (persistence_store_uncertainty(handle, interpretation_uncertainty(interpretation), &uncertainty_id) !=
        PERSISTENCE_UNCERTAINTY_OK) {
        return PERSISTENCE_INTERPRETATION_ERR_DB;
    }

    sqlite3 *db = persistence_db(handle);
    const char *sql = "INSERT INTO interpretations "
                       "(id, target_id, claim_value_type, claim_value_content, claim_value_content_size, "
                       " context_id, evidence_id, framework_reference, uncertainty_id, author, method, status, "
                       " provenance_state, provenance_id) "
                       "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return PERSISTENCE_INTERPRETATION_ERR_DB;
    }

    const Value *claim = interpretation_claim(interpretation);
    size_t claim_size = 0;
    const void *claim_content = value_content(claim, &claim_size);

    const EntityID *context_id = interpretation_context_id(interpretation);
    const EntityID *evidence_id = interpretation_evidence_id(interpretation);
    const FrameworkReference *framework_reference = interpretation_framework_reference(interpretation);
    const ProvenanceReference *provenance = interpretation_provenance(interpretation);

    sqlite3_bind_text(stmt, 1, entity_id_value(interpretation_id(interpretation)), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, entity_id_value(interpretation_target_id(interpretation)), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, value_type(claim), -1, SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 4, claim_content, (int)claim_size, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 5, (sqlite3_int64)claim_size);

    if (context_id != NULL) {
        sqlite3_bind_text(stmt, 6, entity_id_value(context_id), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt, 6);
    }
    if (evidence_id != NULL) {
        sqlite3_bind_text(stmt, 7, entity_id_value(evidence_id), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt, 7);
    }
    if (framework_reference != NULL) {
        sqlite3_bind_text(stmt, 8, entity_id_value(framework_reference_target_id(framework_reference)), -1,
                           SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt, 8);
    }

    sqlite3_bind_int64(stmt, 9, uncertainty_id);
    sqlite3_bind_text(stmt, 10, interpretation_author(interpretation), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 11, interpretation_method(interpretation), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 12, (int)interpretation_status(interpretation));
    sqlite3_bind_int(stmt, 13, (int)provenance_reference_state(provenance));
    if (provenance_reference_state(provenance) == PROVENANCE_REFERENCE_PRESENT) {
        sqlite3_bind_text(stmt, 14, entity_id_value(provenance_reference_target_id(provenance)), -1,
                           SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt, 14);
    }

    PersistenceInterpretationResult result = PERSISTENCE_INTERPRETATION_OK;
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        result = PERSISTENCE_INTERPRETATION_ERR_DB;
    }

    sqlite3_finalize(stmt);
    return result;
}

PersistenceInterpretationResult persistence_load_interpretation(PersistenceHandle *handle,
                                                                 const char *interpretation_id_value,
                                                                 Interpretation **out_interpretation) {
    if (handle == NULL || interpretation_id_value == NULL || out_interpretation == NULL) {
        return PERSISTENCE_INTERPRETATION_ERR_NULL_ARG;
    }

    sqlite3 *db = persistence_db(handle);
    const char *sql = "SELECT target_id, claim_value_type, claim_value_content, claim_value_content_size, "
                       "context_id, evidence_id, framework_reference, uncertainty_id, author, method, status, "
                       "provenance_state, provenance_id "
                       "FROM interpretations WHERE id = ?";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return PERSISTENCE_INTERPRETATION_ERR_DB;
    }
    sqlite3_bind_text(stmt, 1, interpretation_id_value, -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return PERSISTENCE_INTERPRETATION_ERR_DB;
    }

    const unsigned char *target_id_text = sqlite3_column_text(stmt, 0);
    const unsigned char *claim_type_text = sqlite3_column_text(stmt, 1);
    const void *claim_content = sqlite3_column_blob(stmt, 2);
    sqlite3_int64 claim_size = sqlite3_column_int64(stmt, 3);
    const unsigned char *context_id_text = sqlite3_column_text(stmt, 4);
    const unsigned char *evidence_id_text = sqlite3_column_text(stmt, 5);
    const unsigned char *framework_ref_text = sqlite3_column_text(stmt, 6);
    sqlite3_int64 uncertainty_id = sqlite3_column_int64(stmt, 7);
    const unsigned char *author_text = sqlite3_column_text(stmt, 8);
    const unsigned char *method_text = sqlite3_column_text(stmt, 9);
    int status = sqlite3_column_int(stmt, 10);
    int provenance_state = sqlite3_column_int(stmt, 11);
    const unsigned char *provenance_id_text = sqlite3_column_text(stmt, 12);

    EntityID *id = NULL;
    EntityID *target_id = NULL;
    Value *claim = NULL;
    EntityID *context_id = NULL;
    EntityID *evidence_id = NULL;
    FrameworkReference *framework_reference = NULL;
    Uncertainty *uncertainty = NULL;
    ProvenanceReference *provenance = NULL;
    Interpretation *interpretation = NULL;
    PersistenceInterpretationResult result = PERSISTENCE_INTERPRETATION_ERR_DB;

    if (entity_id_create(interpretation_id_value, &id) != ENTITY_ID_OK) {
        goto done;
    }
    if (entity_id_create((const char *)target_id_text, &target_id) != ENTITY_ID_OK) {
        goto done;
    }
    if (value_create((const char *)claim_type_text, claim_content, (size_t)claim_size, &claim) != VALUE_OK) {
        goto done;
    }
    if (context_id_text != NULL) {
        if (entity_id_create((const char *)context_id_text, &context_id) != ENTITY_ID_OK) {
            goto done;
        }
    }
    if (evidence_id_text != NULL) {
        if (entity_id_create((const char *)evidence_id_text, &evidence_id) != ENTITY_ID_OK) {
            goto done;
        }
    }
    if (framework_ref_text != NULL) {
        EntityID *framework_target = NULL;
        if (entity_id_create((const char *)framework_ref_text, &framework_target) != ENTITY_ID_OK) {
            goto done;
        }
        if (framework_reference_create(framework_target, &framework_reference) != FRAMEWORK_REFERENCE_OK) {
            entity_id_destroy(framework_target);
            goto done;
        }
    }
    if (persistence_load_uncertainty(handle, uncertainty_id, &uncertainty) != PERSISTENCE_UNCERTAINTY_OK) {
        goto done;
    }
    if (provenance_state == PROVENANCE_REFERENCE_PRESENT) {
        EntityID *provenance_id = NULL;
        if (entity_id_create((const char *)provenance_id_text, &provenance_id) != ENTITY_ID_OK) {
            goto done;
        }
        if (provenance_reference_create_present(provenance_id, &provenance) != PROVENANCE_REFERENCE_OK) {
            entity_id_destroy(provenance_id);
            goto done;
        }
    } else {
        if (provenance_reference_create_absent(&provenance) != PROVENANCE_REFERENCE_OK) {
            goto done;
        }
    }

    if (interpretation_create(id, target_id, claim, context_id, evidence_id, framework_reference, uncertainty,
                               (const char *)author_text, (const char *)method_text, (InterpretationStatus)status,
                               provenance, &interpretation) != INTERPRETATION_OK) {
        goto done;
    }

    *out_interpretation = interpretation;
    result = PERSISTENCE_INTERPRETATION_OK;
    sqlite3_finalize(stmt);
    return result;

done:
    sqlite3_finalize(stmt);
    if (id != NULL) {
        entity_id_destroy(id);
    }
    if (target_id != NULL) {
        entity_id_destroy(target_id);
    }
    if (claim != NULL) {
        value_destroy(claim);
    }
    if (context_id != NULL) {
        entity_id_destroy(context_id);
    }
    if (evidence_id != NULL) {
        entity_id_destroy(evidence_id);
    }
    if (framework_reference != NULL) {
        framework_reference_destroy(framework_reference);
    }
    if (uncertainty != NULL) {
        uncertainty_destroy(uncertainty);
    }
    if (provenance != NULL) {
        provenance_reference_destroy(provenance);
    }
    return result;
}
