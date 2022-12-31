#ifndef MUSIC_TRANSFORM_INTERVALLIC_MANIPULATION_H
#define MUSIC_TRANSFORM_INTERVALLIC_MANIPULATION_H

#include <stdbool.h>
#include <stddef.h>

#include "core/provenance_reference.h"
#include "events/note_event.h"
#include "identity/entity_id.h"
#include "pitch/pitch_numeric_value.h"
#include "transform/transform_result.h"
#include "transform/transformation.h"

/*
 * IntervallicManipulation (Transformation subtype): changes the size of the intervals between
 * successive notes while keeping each interval's direction (e.g. a minor second becomes a
 * major second, whether it rises or falls). Rhythm, event order, articulation and the contour
 * (direction of every step) are preserved; only pitch changes.
 *
 * adjustments is a required, owned array of signed semitone amounts, one per interval -- so a
 * sequence of n notes needs n - 1 adjustments. A positive amount widens that interval by that
 * many semitones in its own direction, a negative amount narrows it, and 0 leaves it
 * unchanged. The first note stays where it is and every later note moves by the running sum
 * of the amounts before it, so a change to one interval carries on to the notes after it.
 *
 * Owns a Transformation (transform/transformation.h) holding the base contract fields:
 * input_type=output_type="NoteEvent"; preserved_dimensions="rhythm, event order,
 * articulation, contour"; modified_dimensions="pitch (interval sizes)".
 * intervallic_manipulation_id forwards to the owned Transformation.
 */

typedef struct IntervallicManipulation IntervallicManipulation;

typedef enum {
    INTERVALLIC_MANIPULATION_OK = 0,
    INTERVALLIC_MANIPULATION_ERR_NULL_ID,
    INTERVALLIC_MANIPULATION_ERR_NULL_ADJUSTMENTS,
    INTERVALLIC_MANIPULATION_ERR_NULL_PROVENANCE,
    INTERVALLIC_MANIPULATION_ERR_ALLOC
} IntervallicManipulationResult;

/*
 * Takes ownership of id and provenance on success (both destroyed, transitively via the owned
 * Transformation, by intervallic_manipulation_destroy). adjustments is copied; it must be
 * non-NULL with adjustment_count > 0.
 */
IntervallicManipulationResult intervallic_manipulation_create(EntityID *id, const long *adjustments,
                                                                size_t adjustment_count,
                                                                ProvenanceReference *provenance,
                                                                IntervallicManipulation **out);
void intervallic_manipulation_destroy(IntervallicManipulation *manipulation);

const EntityID *intervallic_manipulation_id(const IntervallicManipulation *manipulation);
const Transformation *intervallic_manipulation_transformation(const IntervallicManipulation *manipulation);
size_t intervallic_manipulation_adjustment_count(const IntervallicManipulation *manipulation);
long intervallic_manipulation_adjustment_at(const IntervallicManipulation *manipulation, size_t index);

bool intervallic_manipulation_identity_equals(const IntervallicManipulation *a, const IntervallicManipulation *b);

typedef enum {
    INTERVALLIC_MANIPULATION_APPLY_OK = 0,
    INTERVALLIC_MANIPULATION_APPLY_ERR_NULL_MANIPULATION,
    INTERVALLIC_MANIPULATION_APPLY_ERR_NULL_EVENTS,
    INTERVALLIC_MANIPULATION_APPLY_ERR_NULL_PITCH_VALUES,
    INTERVALLIC_MANIPULATION_APPLY_ERR_EMPTY,
    INTERVALLIC_MANIPULATION_APPLY_ERR_ADJUSTMENT_COUNT_MISMATCH,
    INTERVALLIC_MANIPULATION_APPLY_ERR_ALLOC
} IntervallicManipulationApplyResult;

/*
 * Applies the manipulation to events (a borrowed array of event_count borrowed NoteEvent
 * pointers, in melodic order). pitch_values is a parallel borrowed array of each event's
 * PitchNumericValue, supplied by the caller because PitchIdentity is opaque (same reason
 * transposition_apply takes one). The adjustment count must equal event_count - 1. Each
 * output event is newly constructed with a ProvenanceReference pointing back to its input
 * event; inputs are never mutated.
 */
IntervallicManipulationApplyResult intervallic_manipulation_apply(const IntervallicManipulation *manipulation,
                                                                    NoteEvent **events,
                                                                    const PitchNumericValue **pitch_values,
                                                                    size_t event_count, TransformResult **out);

#endif
