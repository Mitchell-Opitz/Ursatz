#ifndef MUSIC_SEMANTICS_SCALE_DIRECTION_H
#define MUSIC_SEMANTICS_SCALE_DIRECTION_H

#include <stdbool.h>

/*
 * ScaleDirection (Value): the direction a scale or a bass line runs. A plain
 * enum: it names a direction and carries no EntityID, no pitches, and no
 * framework claim. The Rule of the Octave and the sequential harmonizations of
 * a bass scale each have an ascending and a descending form, and some patterns
 * are different in the two (frameworks/scale-harmonization).
 *
 *   ASCENDING    the scale rises, from the tonic up to the octave
 *   DESCENDING   the scale falls, from the octave down to the tonic
 *
 * It is not ContourDirection (semantics/contour.h), which also has a "same"
 * direction and describes the change between two pitches of a Contour.
 */

typedef enum {
    SCALE_DIRECTION_ASCENDING = 0,
    SCALE_DIRECTION_DESCENDING,
    SCALE_DIRECTION_COUNT
} ScaleDirection;

/* True for the two directions above; false for any other value. */
bool scale_direction_valid(ScaleDirection direction);

/* "ascending" "descending"; NULL for an invalid direction. */
const char *scale_direction_name(ScaleDirection direction);

/* The other direction. Returns the direction unchanged when it is invalid. */
ScaleDirection scale_direction_opposite(ScaleDirection direction);

#endif
