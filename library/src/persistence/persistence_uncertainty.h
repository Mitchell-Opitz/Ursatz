#ifndef MUSIC_PERSISTENCE_UNCERTAINTY_H
#define MUSIC_PERSISTENCE_UNCERTAINTY_H

#include <sqlite3.h>

#include "interpretation/uncertainty.h"
#include "persistence/persistence.h"

/*
 * Uncertainty persistence: stores/reloads an Uncertainty
 * (interpretation/uncertainty.h) into the `uncertainties` +
 * `uncertainty_candidates` tables (schema created by persistence_open).
 *
 * Only the columns/child rows valid for the Uncertainty's own
 * uncertainty_kind() are ever written -- never scalar-confidence
 * columns on a candidate-set row or vice versa. Only UNKNOWN,
 * SCALAR_CONFIDENCE, and CANDIDATE_SET exist to persist; the other
 * Registry-named shapes (interval estimate, probability distribution,
 * credible interval, qualitative uncertainty) are not implemented
 * upstream and so have no persistence here either.
 *
 * Used internally by Interpretation persistence (a Chord/Key's
 * uncertainty), but exposed as its own translation unit/header since
 * Uncertainty is itself a standalone, independently testable type.
 */

typedef enum {
    PERSISTENCE_UNCERTAINTY_OK = 0,
    PERSISTENCE_UNCERTAINTY_ERR_NULL_ARG,
    PERSISTENCE_UNCERTAINTY_ERR_DB,
    PERSISTENCE_UNCERTAINTY_ERR_UNSUPPORTED_KIND,
    PERSISTENCE_UNCERTAINTY_ERR_ALLOC
} PersistenceUncertaintyResult;

/*
 * Inserts a new uncertainties row (and, for CANDIDATE_SET, its child
 * uncertainty_candidates rows) and writes the new row's id to
 * *out_uncertainty_id, for use as another table's uncertainty_id FK.
 */
PersistenceUncertaintyResult persistence_store_uncertainty(PersistenceHandle *handle, const Uncertainty *uncertainty,
                                                            sqlite3_int64 *out_uncertainty_id);

/* Loads the uncertainties row (and any candidate children) at uncertainty_id. */
PersistenceUncertaintyResult persistence_load_uncertainty(PersistenceHandle *handle, sqlite3_int64 uncertainty_id,
                                                           Uncertainty **out_uncertainty);

#endif
