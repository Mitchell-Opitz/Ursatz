#ifndef MUSIC_TRANSFORM_RETROGRADE_INVERSION_H
#define MUSIC_TRANSFORM_RETROGRADE_INVERSION_H

#include <stdbool.h>

#include "core/provenance_reference.h"
#include "identity/entity_id.h"
#include "pitch/pitch_identity.h"
#include "transform/inversion.h"
#include "transform/retrograde.h"
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

#endif