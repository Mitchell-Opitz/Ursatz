#ifndef MUSIC_TRANSFORM_DISPLACEMENT_H
#define MUSIC_TRANSFORM_DISPLACEMENT_H

#include <stdbool.h>

#include "core/provenance_reference.h"
#include "identity/entity_id.h"
#include "time/duration.h"
#include "transform/transformation.h"

/*
 * Displacement (Transformation subtype): a temporal shift, applied via
 * TemporalAnchor (time/temporal_anchor.h, L2/L3) -- it shifts the
 * anchor, not the event's other fields (pitch, duration, articulation
 * all stay untouched).
 *
 * direction/magnitude together describe the shift: magnitude is a
 * required, owned Duration (time/duration.h, always non-negative by
 * Duration's own invariant) and direction says which way it moves the
 * anchor's position. There is no signed-offset field here -- Duration
 * cannot represent a negative magnitude, and MusicalTime (the type
 * TemporalAnchor wraps) exposes no time-minus-duration operation, only
 * time-plus-duration (musical_time_add) and time-minus-time
 * (musical_time_subtract) -- so direction+magnitude is the
 * representation this layer's arithmetic can actually support, not a
 * single signed delta.
 *
 * Owns a Transformation (transform/transformation.h) holding the base
 * §139 contract fields: input_type=output_type="NoteEvent";
 * preserved_dimensions="pitch, duration, articulation";
 * modified_dimensions="temporal position (TemporalAnchor)".
 * displacement_id forwards to the owned Transformation.
 */

typedef enum {
    DISPLACEMENT_DIRECTION_FORWARD = 0,
    DISPLACEMENT_DIRECTION_BACKWARD
} DisplacementDirection;

typedef struct Displacement Displacement;

typedef enum {
    DISPLACEMENT_OK = 0,
    DISPLACEMENT_ERR_NULL_ID,
    DISPLACEMENT_ERR_UNKNOWN_DIRECTION,
    DISPLACEMENT_ERR_NULL_MAGNITUDE,
    DISPLACEMENT_ERR_NULL_PROVENANCE,
    DISPLACEMENT_ERR_ALLOC
} DisplacementResult;

/*
 * Takes ownership of id, magnitude, and provenance on success (id and
 * provenance transitively via the owned Transformation; magnitude
 * directly), all destroyed by displacement_destroy.
 */
DisplacementResult displacement_create(EntityID *id, DisplacementDirection direction, Duration *magnitude,
                                        ProvenanceReference *provenance, Displacement **out);
void displacement_destroy(Displacement *displacement);

const EntityID *displacement_id(const Displacement *displacement);
const Transformation *displacement_transformation(const Displacement *displacement);
DisplacementDirection displacement_direction(const Displacement *displacement);
const Duration *displacement_magnitude(const Displacement *displacement);

bool displacement_identity_equals(const Displacement *a, const Displacement *b);

#endif