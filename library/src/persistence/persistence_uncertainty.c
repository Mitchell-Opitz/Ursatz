#include "persistence/persistence_uncertainty.h"

#include <stdlib.h>

static PersistenceUncertaintyResult store_scalar_confidence(sqlite3 *db, const Uncertainty *uncertainty,
                                                              sqlite3_int64 *out_uncertainty_id) {
    const char *sql = "INSERT INTO uncertainties (kind, confidence_numerator, confidence_denominator) "
                       "VALUES (?, ?, ?)";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return PERSISTENCE_UNCERTAINTY_ERR_DB;
    }

    sqlite3_bind_int(stmt, 1, UNCERTAINTY_KIND_SCALAR_CONFIDENCE);
    sqlite3_bind_int64(stmt, 2, (sqlite3_int64)uncertainty_scalar_confidence_numerator(uncertainty));
    sqlite3_bind_int64(stmt, 3, (sqlite3_int64)uncertainty_scalar_confidence_denominator(uncertainty));

    PersistenceUncertaintyResult result = PERSISTENCE_UNCERTAINTY_OK;
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        result = PERSISTENCE_UNCERTAINTY_ERR_DB;
    } else {
        *out_uncertainty_id = sqlite3_last_insert_rowid(db);
    }

    sqlite3_finalize(stmt);
    return result;
}

static PersistenceUncertaintyResult store_candidate_set(sqlite3 *db, const Uncertainty *uncertainty,
                                                         sqlite3_int64 *out_uncertainty_id) {
    const char *sql = "INSERT INTO uncertainties (kind, confidence_numerator, confidence_denominator) "
                       "VALUES (?, NULL, NULL)";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return PERSISTENCE_UNCERTAINTY_ERR_DB;
    }
    sqlite3_bind_int(stmt, 1, UNCERTAINTY_KIND_CANDIDATE_SET);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return PERSISTENCE_UNCERTAINTY_ERR_DB;
    }
    sqlite3_int64 uncertainty_id = sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);

    const char *candidate_sql = "INSERT INTO uncertainty_candidates "
                                 "(uncertainty_id, label, weight_numerator, weight_denominator) "
                                 "VALUES (?, ?, ?, ?)";
    sqlite3_stmt *candidate_stmt = NULL;
    if (sqlite3_prepare_v2(db, candidate_sql, -1, &candidate_stmt, NULL) != SQLITE_OK) {
        return PERSISTENCE_UNCERTAINTY_ERR_DB;
    }

    PersistenceUncertaintyResult result = PERSISTENCE_UNCERTAINTY_OK;
    size_t count = uncertainty_candidate_count(uncertainty);
    for (size_t i = 0; i < count; i++) {
        sqlite3_reset(candidate_stmt);
        sqlite3_clear_bindings(candidate_stmt);

        sqlite3_bind_int64(candidate_stmt, 1, uncertainty_id);
        sqlite3_bind_text(candidate_stmt, 2, uncertainty_candidate_label_at(uncertainty, i), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(candidate_stmt, 3, (sqlite3_int64)uncertainty_candidate_weight_numerator_at(uncertainty, i));
        sqlite3_bind_int64(candidate_stmt, 4,
                            (sqlite3_int64)uncertainty_candidate_weight_denominator_at(uncertainty, i));

        if (sqlite3_step(candidate_stmt) != SQLITE_DONE) {
            result = PERSISTENCE_UNCERTAINTY_ERR_DB;
            break;
        }
    }

    sqlite3_finalize(candidate_stmt);
    if (result == PERSISTENCE_UNCERTAINTY_OK) {
        *out_uncertainty_id = uncertainty_id;
    }
    return result;
}

static PersistenceUncertaintyResult store_unknown(sqlite3 *db, sqlite3_int64 *out_uncertainty_id) {
    const char *sql = "INSERT INTO uncertainties (kind, confidence_numerator, confidence_denominator) "
                       "VALUES (?, NULL, NULL)";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return PERSISTENCE_UNCERTAINTY_ERR_DB;
    }
    sqlite3_bind_int(stmt, 1, UNCERTAINTY_KIND_UNKNOWN);

    PersistenceUncertaintyResult result = PERSISTENCE_UNCERTAINTY_OK;
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        result = PERSISTENCE_UNCERTAINTY_ERR_DB;
    } else {
        *out_uncertainty_id = sqlite3_last_insert_rowid(db);
    }

    sqlite3_finalize(stmt);
    return result;
}

PersistenceUncertaintyResult persistence_store_uncertainty(PersistenceHandle *handle, const Uncertainty *uncertainty,
                                                            sqlite3_int64 *out_uncertainty_id) {
    if (handle == NULL || uncertainty == NULL || out_uncertainty_id == NULL) {
        return PERSISTENCE_UNCERTAINTY_ERR_NULL_ARG;
    }

    sqlite3 *db = persistence_db(handle);
    switch (uncertainty_kind(uncertainty)) {
        case UNCERTAINTY_KIND_UNKNOWN:
            return store_unknown(db, out_uncertainty_id);
        case UNCERTAINTY_KIND_SCALAR_CONFIDENCE:
            return store_scalar_confidence(db, uncertainty, out_uncertainty_id);
        case UNCERTAINTY_KIND_CANDIDATE_SET:
            return store_candidate_set(db, uncertainty, out_uncertainty_id);
        default:
            return PERSISTENCE_UNCERTAINTY_ERR_UNSUPPORTED_KIND;
    }
}

static PersistenceUncertaintyResult load_candidate_set(sqlite3 *db, sqlite3_int64 uncertainty_id,
                                                        Uncertainty **out_uncertainty) {
    const char *sql = "SELECT label, weight_numerator, weight_denominator FROM uncertainty_candidates "
                       "WHERE uncertainty_id = ? ORDER BY id ASC";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return PERSISTENCE_UNCERTAINTY_ERR_DB;
    }
    sqlite3_bind_int64(stmt, 1, uncertainty_id);

    UncertaintyCandidate *candidates = NULL;
    size_t count = 0;
    size_t capacity = 0;
    PersistenceUncertaintyResult result = PERSISTENCE_UNCERTAINTY_OK;

    int rc;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (count == capacity) {
            capacity = capacity == 0 ? 4 : capacity * 2;
            UncertaintyCandidate *resized = realloc(candidates, capacity * sizeof(UncertaintyCandidate));
            if (resized == NULL) {
                result = PERSISTENCE_UNCERTAINTY_ERR_ALLOC;
                break;
            }
            candidates = resized;
        }
        candidates[count].label = (char *)sqlite3_column_text(stmt, 0);
        candidates[count].weight_numerator = (unsigned long)sqlite3_column_int64(stmt, 1);
        candidates[count].weight_denominator = (unsigned long)sqlite3_column_int64(stmt, 2);
        count++;
    }

    if (rc != SQLITE_DONE && result == PERSISTENCE_UNCERTAINTY_OK) {
        result = PERSISTENCE_UNCERTAINTY_ERR_DB;
    }

    if (result == PERSISTENCE_UNCERTAINTY_OK) {
        if (uncertainty_create_candidate_set(candidates, count, out_uncertainty) != UNCERTAINTY_OK) {
            result = PERSISTENCE_UNCERTAINTY_ERR_DB;
        }
    }

    free(candidates);
    sqlite3_finalize(stmt);
    return result;
}

PersistenceUncertaintyResult persistence_load_uncertainty(PersistenceHandle *handle, sqlite3_int64 uncertainty_id,
                                                           Uncertainty **out_uncertainty) {
    if (handle == NULL || out_uncertainty == NULL) {
        return PERSISTENCE_UNCERTAINTY_ERR_NULL_ARG;
    }

    sqlite3 *db = persistence_db(handle);
    const char *sql = "SELECT kind, confidence_numerator, confidence_denominator FROM uncertainties WHERE id = ?";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return PERSISTENCE_UNCERTAINTY_ERR_DB;
    }
    sqlite3_bind_int64(stmt, 1, uncertainty_id);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return PERSISTENCE_UNCERTAINTY_ERR_DB;
    }

    int kind = sqlite3_column_int(stmt, 0);
    sqlite3_int64 confidence_num = sqlite3_column_int64(stmt, 1);
    sqlite3_int64 confidence_den = sqlite3_column_int64(stmt, 2);
    sqlite3_finalize(stmt);

    switch (kind) {
        case UNCERTAINTY_KIND_UNKNOWN:
            return uncertainty_create_unknown(out_uncertainty) == UNCERTAINTY_OK ? PERSISTENCE_UNCERTAINTY_OK
                                                                                  : PERSISTENCE_UNCERTAINTY_ERR_DB;
        case UNCERTAINTY_KIND_SCALAR_CONFIDENCE:
            return uncertainty_create_scalar_confidence((unsigned long)confidence_num, (unsigned long)confidence_den,
                                                         out_uncertainty) == UNCERTAINTY_OK
                       ? PERSISTENCE_UNCERTAINTY_OK
                       : PERSISTENCE_UNCERTAINTY_ERR_DB;
        case UNCERTAINTY_KIND_CANDIDATE_SET:
            return load_candidate_set(db, uncertainty_id, out_uncertainty);
        default:
            return PERSISTENCE_UNCERTAINTY_ERR_UNSUPPORTED_KIND;
    }
}
