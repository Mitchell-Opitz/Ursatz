#ifndef MUSIC_PERSISTENCE_INTERPRETATION_H
#define MUSIC_PERSISTENCE_INTERPRETATION_H

#include "interpretation/interpretation.h"
#include "persistence/persistence.h"

/*
 * Interpretation persistence: stores/reloads a full Interpretation
 * (interpretation/interpretation.h) into the `interpretations` table,
 * including its embedded Uncertainty (via persistence_uncertainty.h).
 *
 * claim (a Value, core/value.h) is opaque by design -- this module
 * persists its type string and content bytes verbatim and never
 * decodes/interprets the content. context_id, evidence_id, and
 * framework_reference are optional references (stored as nullable
 * EntityID text); target_id, author, method, and provenance are
 * required.
 *
 * Used internally by Chord/Key persistence (persistence_chord.h,
 * persistence_key.h), which each own an embedded Interpretation.
 */

typedef enum {
    PERSISTENCE_INTERPRETATION_OK = 0,
    PERSISTENCE_INTERPRETATION_ERR_NULL_ARG,
    PERSISTENCE_INTERPRETATION_ERR_DB,
    PERSISTENCE_INTERPRETATION_ERR_ALLOC
} PersistenceInterpretationResult;

/* interpretation_id is the interpretation's own EntityID value, used as the row's primary key. */
PersistenceInterpretationResult persistence_store_interpretation(PersistenceHandle *handle,
                                                                  const Interpretation *interpretation);

PersistenceInterpretationResult persistence_load_interpretation(PersistenceHandle *handle,
                                                                 const char *interpretation_id,
                                                                 Interpretation **out_interpretation);

#endif
