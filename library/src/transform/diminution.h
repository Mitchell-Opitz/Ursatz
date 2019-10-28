#ifndef MUSIC_TRANSFORM_DIMINUTION_H
#define MUSIC_TRANSFORM_DIMINUTION_H

#include <stdbool.h>

#include "core/provenance_reference.h"
#include "events/note_event.h"
#include "identity/entity_id.h"
#include "transform/augmentation.h"
#include "transform/transform_result.h"
#include "transform/transformation.h"

/*
 * Diminution (Transformation subtype): shrinks a timing dimension by
 * an exact rational factor (TDD §144) -- the mirror of Augmentation
 * (transform/augmentation.h), which this type shares its dimension
 * vocabulary (AugmentationDimension) with rather than redefining an
 * identical enum.
 *
 * TDD §144 requires Diminution to explicitly specify which dimension
 * is modified, and the default MUST NOT touch performance timing
 * unless explicitly requested -- the same explicit-dimension treatment
 * Augmentation gives it (AUGMENTATION_DIMENSION_DURATIONS is enum
 * value 0, not PERFORMANCE_TIMING).
 *
 * scale_numerator/scale_denominator is an exact rational factor (never
 * float/double, Law 19). Diminution shrinks, so numerator MUST be
 * strictly less than denominator (both positive) -- a factor >= 1 is
 * Augmentation's job, not this type's.
 *
 * Owns a Transformation (transform/transformation.h) holding the base
 * §139 contract fields. input_type=output_type="NoteEvent";
 * preserved_dimensions is fixed ("pitch, event order, articulation");
 * modified_dimensions is derived from the chosen dimension.
 * diminution_id forwards to the owned Transformation.
 */

typedef struct Diminution Diminution;

typedef enum {
    DIMINUTION_OK = 0,
    DIMINUTION_ERR_NULL_ID,
    DIMINUTION_ERR_UNKNOWN_DIMENSION,
    DIMINUTION_ERR_NON_POSITIVE_DENOMINATOR,
    DIMINUTION_ERR_FACTOR_NOT_LESS_THAN_ONE,
    DIMINUTION_ERR_NULL_PROVENANCE,
    DIMINUTION_ERR_ALLOC
} DiminutionResult;

/*
 * Takes ownership of id and provenance on success (both destroyed
 * transitively via the owned Transformation, by diminution_destroy).
 * scale_numerator/scale_denominator MUST both be > 0, and
 * scale_numerator MUST be strictly less than scale_denominator (net
 * shrinking).
 */
DiminutionResult diminution_create(EntityID *id, AugmentationDimension dimension, unsigned long scale_numerator,
                                    unsigned long scale_denominator, ProvenanceReference *provenance,
                                    Diminution **out);
void diminution_destroy(Diminution *diminution);

const EntityID *diminution_id(const Diminution *diminution);
const Transformation *diminution_transformation(const Diminution *diminution);
AugmentationDimension diminution_dimension(const Diminution *diminution);
unsigned long diminution_scale_numerator(const Diminution *diminution);
unsigned long diminution_scale_denominator(const Diminution *diminution);

bool diminution_identity_equals(const Diminution *a, const Diminution *b);

typedef enum {
    DIMINUTION_APPLY_OK = 0,
    DIMINUTION_APPLY_ERR_NULL_DIMINUTION,
    DIMINUTION_APPLY_ERR_NULL_EVENT,
    DIMINUTION_APPLY_ERR_DIMENSION_NOT_IMPLEMENTED,
    DIMINUTION_APPLY_ERR_ALLOC
} DiminutionApplyResult;

/*
 * Applies diminution to event (a borrowed NoteEvent): only
 * AUGMENTATION_DIMENSION_DURATIONS has real logic in this pass -- the
 * event's Duration is scaled by scale_numerator/scale_denominator,
 * exact rational multiplication, reduced to lowest terms (Law 19).
 * Every other field is copied unchanged onto the output event. The
 * other four dimension values fail with
 * DIMINUTION_APPLY_ERR_DIMENSION_NOT_IMPLEMENTED, for the same reason
 * documented on augmentation_apply (transform/augmentation.h) -- this
 * type shares that reasoning along with the dimension vocabulary.
 *
 * event is never mutated; *out's event carries a real
 * ProvenanceReference pointing back to event.
 */
DiminutionApplyResult diminution_apply(const Diminution *diminution, const NoteEvent *event, TransformResult **out);

#endif