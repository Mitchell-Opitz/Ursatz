#ifndef MUSIC_TRANSFORM_RETROGRADE_INVERSION_H
#define MUSIC_TRANSFORM_RETROGRADE_INVERSION_H

#include <stdbool.h>
#include <stddef.h>

#include "core/provenance_reference.h"
#include "events/note_event.h"
#include "identity/entity_id.h"
#include "pitch/pitch_identity.h"
#include "pitch/pitch_numeric_value.h"
#include "transform/inversion.h"
#include "transform/retrograde.h"
#include "transform/transform_result.h"
#include "transform/transformation.h"

/*
 * RetrogradeInversion (Transformation subtype): Retrograde composed
 * with Inversion. Per the Registry, this is built BY COMPOSING the two
 * (transform/retrograde.h, transform/inversion.h), not as an
 * independent reimplementation -- retrograde_inversion_create
 * constructs and owns one Retrograde and one Inversion internally,
 * using the same reversed_dimensions/axis/pitch_space/spelling_policy
 * arguments a caller would hand to each component directly, and every
 * validation rule/error each component enforces on its own (TDD §142,
 * §143) applies here unchanged -- this type adds no rules of its own
 * beyond composing them.
 *
 * The component Retrograde and Inversion each get their own id
 * (derived by suffixing the id passed here with "-retrograde"/
 * "-inversion") and their own absent ProvenanceReference -- they are
 * implementation details of this composition, not independently
 * provenanced operations; the ProvenanceReference passed to this
 * constructor covers the composed operation as a whole and is the one
 * attached to the owned outer Transformation.
 *
 * Also owns its own outer Transformation (input_type=output_type=
 * "Structure", matching Retrograde's own choice since the composed
 * operation acts on a sequence) so RetrogradeInversion satisfies the
 * base §139 contract in its own right, the same pattern every other
 * concrete Transformation in this module follows.
 * retrograde_inversion_id forwards to it.
 */

typedef struct RetrogradeInversion RetrogradeInversion;

typedef enum {
    RETROGRADE_INVERSION_OK = 0,
    RETROGRADE_INVERSION_ERR_NULL_ID,
    RETROGRADE_INVERSION_ERR_NO_DIMENSIONS,
    RETROGRADE_INVERSION_ERR_UNKNOWN_DIMENSION_BITS,
    RETROGRADE_INVERSION_ERR_NULL_AXIS,
    RETROGRADE_INVERSION_ERR_NULL_PITCH_SPACE,
    RETROGRADE_INVERSION_ERR_NULL_SPELLING_POLICY,
    RETROGRADE_INVERSION_ERR_NULL_PROVENANCE,
    RETROGRADE_INVERSION_ERR_ALLOC
} RetrogradeInversionResult;

/*
 * Takes ownership of id, axis, and provenance on success (destroyed by
 * retrograde_inversion_destroy). reversed_dimensions follows Retrograde's
 * own rules (transform/retrograde.h): non-zero, no bit outside
 * RETROGRADE_DIMENSION_ALL. pitch_space and spelling_policy follow
 * Inversion's own rules (transform/inversion.h): required, non-empty.
 * Tuning behavior/semantics is always declared absent for both
 * components, the same explicitly-absent-stub treatment they each give
 * it on their own.
 */
RetrogradeInversionResult retrograde_inversion_create(EntityID *id, unsigned reversed_dimensions, PitchIdentity *axis,
                                                        const char *pitch_space, const char *spelling_policy,
                                                        ProvenanceReference *provenance, RetrogradeInversion **out);
void retrograde_inversion_destroy(RetrogradeInversion *retrograde_inversion);

const EntityID *retrograde_inversion_id(const RetrogradeInversion *retrograde_inversion);
/* The owned outer Transformation -- input/output type, dimensions, mapping behavior, determinism, provenance. */
const Transformation *retrograde_inversion_transformation(const RetrogradeInversion *retrograde_inversion);
const Retrograde *retrograde_inversion_retrograde(const RetrogradeInversion *retrograde_inversion);
const Inversion *retrograde_inversion_inversion(const RetrogradeInversion *retrograde_inversion);

bool retrograde_inversion_identity_equals(const RetrogradeInversion *a, const RetrogradeInversion *b);

typedef enum {
    RETROGRADE_INVERSION_APPLY_OK = 0,
    RETROGRADE_INVERSION_APPLY_ERR_NULL_RETROGRADE_INVERSION,
    RETROGRADE_INVERSION_APPLY_ERR_NULL_EVENTS,
    RETROGRADE_INVERSION_APPLY_ERR_EMPTY,
    RETROGRADE_INVERSION_APPLY_ERR_NULL_PITCH_VALUES,
    RETROGRADE_INVERSION_APPLY_ERR_NULL_AXIS_VALUE,
    RETROGRADE_INVERSION_APPLY_ERR_DIMENSION_NOT_IMPLEMENTED,
    RETROGRADE_INVERSION_APPLY_ERR_TUNING_SYSTEM_MISMATCH,
    RETROGRADE_INVERSION_APPLY_ERR_ALLOC
} RetrogradeInversionApplyResult;

/*
 * Applies the composed operation to events (a borrowed array of
 * event_count borrowed NoteEvent pointers): calls retrograde_apply
 * (transform/retrograde.h) on the composed Retrograde to reverse
 * temporal order, then calls inversion_apply (transform/inversion.h) on
 * the composed Inversion for each resulting event to reflect its pitch
 * about axis_value -- CALLING the other two Transformations' real apply
 * logic, never reimplementing reversal or reflection arithmetic here.
 * transform_result_take_event_at (transform/transform_result.h) moves
 * each component's actual output object into this composition's own
 * result, so the final events are the same objects Retrograde/Inversion
 * produced, not re-derived copies.
 *
 * event_pitch_values[i] is the caller-supplied numeric resolution of
 * events[i]'s PitchIdentity (same array length/order as events) --
 * required for the same reason inversion_apply requires one: pitch
 * arithmetic needs a numeric value this layer cannot derive from an
 * opaque PitchIdentity. Because Retrograde changes only temporal
 * position, not pitch, event_pitch_values[i] is still the correct
 * numeric pitch for whatever event ends up at index i after
 * retrograding -- it is passed straight through to that event's
 * inversion_apply call.
 *
 * axis_value is the caller-supplied numeric resolution of the composed
 * Inversion's own axis (retrograde_inversion_inversion(retrograde_inversion)
 * -> inversion_axis()), under the same TuningSystem as every
 * event_pitch_values[i]. A mismatch fails with
 * RETROGRADE_INVERSION_APPLY_ERR_TUNING_SYSTEM_MISMATCH, forwarded from
 * the underlying inversion_apply call.
 *
 * If the composed Retrograde's reversed_dimensions carries any bit
 * besides RETROGRADE_DIMENSION_EVENT_ORDER, this fails with
 * RETROGRADE_INVERSION_APPLY_ERR_DIMENSION_NOT_IMPLEMENTED, forwarded
 * from the underlying retrograde_apply call (see that function's own
 * documentation for why).
 *
 * events are never mutated; every event in *out carries a real
 * ProvenanceReference (the retrograded-then-inverted event's provenance
 * points back to the retrograded intermediate event, which itself
 * points back to the original input event -- a two-hop provenance
 * chain reflecting the two real operations actually applied).
 */
RetrogradeInversionApplyResult retrograde_inversion_apply(const RetrogradeInversion *retrograde_inversion,
                                                           NoteEvent **events,
                                                           const PitchNumericValue **event_pitch_values,
                                                           size_t event_count, const PitchNumericValue *axis_value,
                                                           TransformResult **out);

#endif