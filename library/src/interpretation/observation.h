#ifndef MUSIC_INTERPRETATION_OBSERVATION_H
#define MUSIC_INTERPRETATION_OBSERVATION_H

#include "core/provenance_reference.h"
#include "core/value.h"
#include "identity/entity_id.h"
#include "interpretation/uncertainty.h"

/*
 * Observation (Category / base type): detected/measured information,
 * with NO interpretation implied (TDD §81-82).
 *
 * Fields: id, subject (reference), property, value_or_candidates,
 * method, source, uncertainty, timestamp, provenance
 * (ProvenanceReference).
 *
 * An Observation stores raw detected values, not conclusions: it
 * carries no claim/interpretation field (that is Assertion,
 * interpretation/assertion.h, and Interpretation, both distinct
 * types) -- only what was measured, how, and how uncertain that
 * measurement is.
 *
 * subject is an opaque EntityID reference to what was observed --
 * referenced, never embedded, the same pattern as Assertion's
 * subject.
 *
 * value_or_candidates is the detected value (or candidate set, when
 * detection is ambiguous) itself. Its schema is not yet specified at
 * this layer, so it is kept as an opaque Value (core/value.h), the
 * same treatment used elsewhere in this codebase for not-yet-specified
 * content.
 *
 * uncertainty is a real Uncertainty (interpretation/uncertainty.h,
 * TDD §84) -- required, since every Observation states how uncertain
 * its own measurement is (use uncertainty_create_unknown when that is
 * genuinely unknown, never omit the field).
 *
 * timestamp is a PLACEHOLDER: this is real-world "when was this
 * measured" wall-clock time, not a musical position -- TimeInstant
 * (time/time_instant.h) wraps MusicalTime and is the wrong domain for
 * it. No wall-clock timestamp type exists yet in this codebase, so
 * this field is an opaque, nullable Value for now. Flagged as a
 * follow-up: replace with a real timestamp type once one exists.
 *
 * provenance is required (ProvenanceReference, core/provenance_reference.h).
 */

typedef struct Observation Observation;

typedef enum {
    OBSERVATION_OK = 0,
    OBSERVATION_ERR_NULL_ID,
    OBSERVATION_ERR_NULL_SUBJECT_ID,
    OBSERVATION_ERR_NULL_PROPERTY,
    OBSERVATION_ERR_NULL_VALUE_OR_CANDIDATES,
    OBSERVATION_ERR_NULL_METHOD,
    OBSERVATION_ERR_NULL_SOURCE,
    OBSERVATION_ERR_NULL_UNCERTAINTY,
    OBSERVATION_ERR_NULL_PROVENANCE,
    OBSERVATION_ERR_ALLOC
} ObservationResult;

/*
 * Takes ownership of id, subject_id, value_or_candidates, uncertainty,
 * and provenance on success (all required, destroyed by
 * observation_destroy). property, method, and source are required
 * (copied, non-empty). timestamp is optional (may be NULL); if
 * non-NULL, ownership transfers on success.
 */
ObservationResult observation_create(EntityID *id, EntityID *subject_id, const char *property,
                                      Value *value_or_candidates, const char *method, const char *source,
                                      Uncertainty *uncertainty, Value *timestamp, ProvenanceReference *provenance,
                                      Observation **out);
void observation_destroy(Observation *observation);

const EntityID *observation_id(const Observation *observation);
const EntityID *observation_subject_id(const Observation *observation);
const char *observation_property(const Observation *observation);
const Value *observation_value_or_candidates(const Observation *observation);
const char *observation_method(const Observation *observation);
const char *observation_source(const Observation *observation);
const Uncertainty *observation_uncertainty(const Observation *observation);
/* NULL when no timestamp was supplied. */
const Value *observation_timestamp(const Observation *observation);
const ProvenanceReference *observation_provenance(const Observation *observation);

#endif