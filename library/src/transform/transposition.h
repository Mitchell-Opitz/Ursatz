#ifndef MUSIC_TRANSFORM_TRANSPOSITION_H
#define MUSIC_TRANSFORM_TRANSPOSITION_H

#include <stdbool.h>

#include "core/provenance_reference.h"
#include "identity/entity_id.h"
#include "transform/transformation.h"

/*
 * Transposition (Transformation subtype): shifts pitch by a fixed
 * displacement (TDD §141).
 *
 * Owns a Transformation (transform/transformation.h) holding the base
 * §139 contract fields (input_type="NoteEvent", output_type="NoteEvent",
 * parameters=semitones packed as a Value, preserved_dimensions=
 * "rhythm, event order, articulation", modified_dimensions="pitch",
 * mapping_behavior="one-to-one, per-event pitch displacement",
 * deterministic=true, and the supplied provenance) built internally by
 * transposition_create -- a caller never constructs the Transformation
 * itself. transposition_id/transposition_provenance forward to it
 * rather than duplicating identity/provenance on Transposition.
 *
 * TDD §141 requires Transposition to explicitly define: pitch
 * displacement, spelling behavior, tuning behavior, range policy, and
 * instrument context. Tuning system, range policy, and instrument
 * context (Deliverable D / Instrument, deferred at L3) have no
 * representation in this codebase yet -- this constructor requires
 * has_tuning_behavior/has_range_policy/has_instrument_context to be
 * passed explicitly false (rejecting true, since there is nothing yet
 * to back a true value with) so those dimensions are explicitly
 * declared absent rather than silently missing from the struct.
 * pitch_displacement (semitones) and spelling_behavior are implemented
 * now.
 *
 * semitones is a signed directed displacement (positive = up, negative
 * = down), the same directed-semitone convention Interval uses
 * (semantics/interval.h).
 *
 * spelling_behavior is a required descriptor string (e.g. "diatonic,
 * preserve letter-name interval" or "enharmonic respell to nearest
 * simple spelling") -- no dedicated SpellingPolicy type exists yet, the
 * same placeholder treatment Transformation gives its own dimension
 * fields.
 */

typedef struct Transposition Transposition;

typedef enum {
    TRANSPOSITION_OK = 0,
    TRANSPOSITION_ERR_NULL_ID,
    TRANSPOSITION_ERR_NULL_SPELLING_BEHAVIOR,
    TRANSPOSITION_ERR_TUNING_BEHAVIOR_NOT_SUPPORTED,
    TRANSPOSITION_ERR_RANGE_POLICY_NOT_SUPPORTED,
    TRANSPOSITION_ERR_INSTRUMENT_CONTEXT_NOT_SUPPORTED,
    TRANSPOSITION_ERR_NULL_PROVENANCE,
    TRANSPOSITION_ERR_ALLOC
} TranspositionResult;

/*
 * Takes ownership of id and provenance on success (both destroyed,
 * transitively via the owned Transformation, by transposition_destroy).
 * spelling_behavior is required (copied, non-empty). has_tuning_behavior,
 * has_range_policy, and has_instrument_context MUST all be passed false
 * -- passing true for any of them fails with the corresponding
 * *_NOT_SUPPORTED result, since no tuning/range/instrument
 * representation exists yet to back a true value.
 */
TranspositionResult transposition_create(EntityID *id, long semitones, const char *spelling_behavior,
                                          bool has_tuning_behavior, bool has_range_policy,
                                          bool has_instrument_context, ProvenanceReference *provenance,
                                          Transposition **out);
void transposition_destroy(Transposition *transposition);

const EntityID *transposition_id(const Transposition *transposition);
const Transformation *transposition_transformation(const Transposition *transposition);
long transposition_semitones(const Transposition *transposition);
const char *transposition_spelling_behavior(const Transposition *transposition);
bool transposition_has_tuning_behavior(const Transposition *transposition);
bool transposition_has_range_policy(const Transposition *transposition);
bool transposition_has_instrument_context(const Transposition *transposition);

bool transposition_identity_equals(const Transposition *a, const Transposition *b);

#endif