#ifndef MUSIC_TRANSFORM_RETROGRADE_H
#define MUSIC_TRANSFORM_RETROGRADE_H

#include <stdbool.h>

#include "core/provenance_reference.h"
#include "identity/entity_id.h"
#include "transform/transformation.h"

/*
 * Retrograde (Transformation subtype): reverses one or more explicit
 * dimensions of a sequence (TDD §143).
 *
 * TDD §143 requires Retrograde to explicitly state which dimension(s)
 * are reversed (event order / pitch / rhythm / articulation / all) --
 * it MUST NOT default to "reverse everything" silently. reversed_dimensions
 * is therefore a required, non-zero bitmask of RetrogradeDimension flags
 * the caller must OR together explicitly; there is no default value and
 * no implicit "reverse everything" behavior -- reversing all four
 * dimensions requires the caller to pass all four flags (or the
 * RETROGRADE_DIMENSION_ALL convenience constant, which is defined as
 * their union, not a distinct silent default).
 *
 * Owns a Transformation (transform/transformation.h) holding the base
 * §139 contract fields. input_type/output_type are "Structure"
 * (Retrograde reverses a sequence, not a single event -- Structure is
 * the Registry's ordered-material container, structure/structure.h).
 * preserved_dimensions/modified_dimensions on the owned Transformation
 * are derived from reversed_dimensions at construction (a human-readable
 * listing, e.g. "pitch, rhythm" for modified_dimensions when only those
 * two flags are set) so the base contract and this type's own explicit
 * flag never disagree. mapping_behavior states one-to-one over the
 * reversed sequence; deterministic=true. retrograde_id forwards to the
 * owned Transformation rather than duplicating identity on Retrograde.
 */

typedef enum {
    RETROGRADE_DIMENSION_EVENT_ORDER = 1u << 0,
    RETROGRADE_DIMENSION_PITCH = 1u << 1,
    RETROGRADE_DIMENSION_RHYTHM = 1u << 2,
    RETROGRADE_DIMENSION_ARTICULATION = 1u << 3,
    RETROGRADE_DIMENSION_ALL = RETROGRADE_DIMENSION_EVENT_ORDER | RETROGRADE_DIMENSION_PITCH |
                                RETROGRADE_DIMENSION_RHYTHM | RETROGRADE_DIMENSION_ARTICULATION
} RetrogradeDimension;

typedef struct Retrograde Retrograde;

typedef enum {
    RETROGRADE_OK = 0,
    RETROGRADE_ERR_NULL_ID,
    RETROGRADE_ERR_NO_DIMENSIONS,
    RETROGRADE_ERR_UNKNOWN_DIMENSION_BITS,
    RETROGRADE_ERR_NULL_PROVENANCE,
    RETROGRADE_ERR_ALLOC
} RetrogradeResult;

/*
 * Takes ownership of id and provenance on success (both destroyed
 * transitively via the owned Transformation, by retrograde_destroy).
 * reversed_dimensions MUST be a non-zero combination of
 * RetrogradeDimension flags and no other bits -- 0 (no dimension
 * declared) fails with RETROGRADE_ERR_NO_DIMENSIONS, and any bit
 * outside RETROGRADE_DIMENSION_ALL fails with
 * RETROGRADE_ERR_UNKNOWN_DIMENSION_BITS.
 */
RetrogradeResult retrograde_create(EntityID *id, unsigned reversed_dimensions, ProvenanceReference *provenance,
                                    Retrograde **out);
void retrograde_destroy(Retrograde *retrograde);

const EntityID *retrograde_id(const Retrograde *retrograde);
/* The owned base Transformation -- input/output type, dimensions, mapping behavior, determinism, provenance. */
const Transformation *retrograde_transformation(const Retrograde *retrograde);
unsigned retrograde_reversed_dimensions(const Retrograde *retrograde);

bool retrograde_identity_equals(const Retrograde *a, const Retrograde *b);

#endif