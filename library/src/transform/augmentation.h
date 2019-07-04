#ifndef MUSIC_TRANSFORM_AUGMENTATION_H
#define MUSIC_TRANSFORM_AUGMENTATION_H

#include <stdbool.h>

#include "core/provenance_reference.h"
#include "identity/entity_id.h"
#include "transform/transformation.h"

/*
 * Augmentation (Transformation subtype): stretches a timing dimension
 * by an exact rational factor (TDD §144).
 *
 * TDD §144 requires Augmentation to explicitly specify which dimension
 * is modified (durations / inter-onset intervals / structural timing /
 * notation values / performance timing), and the default MUST NOT
 * touch performance timing unless explicitly requested. dimension is
 * therefore a required AugmentationDimension the caller must choose
 * explicitly; AUGMENTATION_DIMENSION_DURATIONS is enum value 0 (not
 * PERFORMANCE_TIMING), so an accidentally zero-initialized/uninitialized
 * caller-side variable can never silently select performance timing --
 * touching it always requires the caller to spell out
 * AUGMENTATION_DIMENSION_PERFORMANCE_TIMING.
 *
 * scale_numerator/scale_denominator is an exact rational factor (never
 * float/double, Law 19), the same numerator/denominator-in-lowest-terms
 * shape Duration uses (time/duration.h). Augmentation lengthens, so
 * numerator MUST be strictly greater than denominator (both positive) --
 * a factor <= 1 is Diminution's job (transform/diminution.h, a later
 * commit), not this type's.
 *
 * Owns a Transformation (transform/transformation.h) holding the base
 * §139 contract fields. input_type=output_type="NoteEvent";
 * preserved_dimensions is fixed ("pitch, event order, articulation");
 * modified_dimensions is derived from the chosen dimension.
 * augmentation_id forwards to the owned Transformation.
 */

typedef enum {
    AUGMENTATION_DIMENSION_DURATIONS = 0,
    AUGMENTATION_DIMENSION_INTER_ONSET_INTERVALS,
    AUGMENTATION_DIMENSION_STRUCTURAL_TIMING,
    AUGMENTATION_DIMENSION_NOTATION_VALUES,
    AUGMENTATION_DIMENSION_PERFORMANCE_TIMING
} AugmentationDimension;

typedef struct Augmentation Augmentation;

typedef enum {
    AUGMENTATION_OK = 0,
    AUGMENTATION_ERR_NULL_ID,
    AUGMENTATION_ERR_UNKNOWN_DIMENSION,
    AUGMENTATION_ERR_NON_POSITIVE_DENOMINATOR,
    AUGMENTATION_ERR_FACTOR_NOT_GREATER_THAN_ONE,
    AUGMENTATION_ERR_NULL_PROVENANCE,
    AUGMENTATION_ERR_ALLOC
} AugmentationResult;

/*
 * Takes ownership of id and provenance on success (both destroyed
 * transitively via the owned Transformation, by augmentation_destroy).
 * scale_denominator MUST be > 0; scale_numerator MUST be strictly
 * greater than scale_denominator (both positive, net lengthening).
 */
AugmentationResult augmentation_create(EntityID *id, AugmentationDimension dimension, unsigned long scale_numerator,
                                        unsigned long scale_denominator, ProvenanceReference *provenance,
                                        Augmentation **out);
void augmentation_destroy(Augmentation *augmentation);

const EntityID *augmentation_id(const Augmentation *augmentation);
/* The owned base Transformation -- input/output type, dimensions, mapping behavior, determinism, provenance. */
const Transformation *augmentation_transformation(const Augmentation *augmentation);
AugmentationDimension augmentation_dimension(const Augmentation *augmentation);
unsigned long augmentation_scale_numerator(const Augmentation *augmentation);
unsigned long augmentation_scale_denominator(const Augmentation *augmentation);

bool augmentation_identity_equals(const Augmentation *a, const Augmentation *b);

#endif