#ifndef MUSIC_SEMANTICS_NON_CHORD_TONE_H
#define MUSIC_SEMANTICS_NON_CHORD_TONE_H

#include <stdbool.h>

/*
 * NonChordTone (Value): a way a melody embellishes the notes it moves
 * between, with a note that is dissonant against the harmony or the other
 * part. A plain enum: it names a figure and carries no EntityID, no pitches,
 * and no framework claim.
 *
 *   PASSING          fills in the space of a melodic third (or more) between
 *                    two notes by stepwise motion in one direction
 *   NEIGHBOR         steps away from a note and steps back to the same note
 *   DOUBLE_NEIGHBOR  embellishes a note with both its upper and lower
 *                    neighbors, one after the other, before returning to it
 *                    (C-D-B-C or C-B-D-C)
 *   CHANGING_TONE    the nota cambiata: a five-note figure that outlines a
 *                    step, in which the dissonance leaps to the next note
 *                    and the dissonant pitch comes back as the last note
 *   SUSPENSION       a note held over from a consonance (the preparation)
 *                    into a position where it is dissonant, on a stronger
 *                    beat than the preparation, which then resolves by
 *                    stepping down to a consonance
 *
 * Whether each figure is allowed, on which beats, and against what, is a
 * convention of a Framework (frameworks/standard-counterpoint), not of this
 * type.
 */

typedef enum {
    NON_CHORD_TONE_PASSING = 0,
    NON_CHORD_TONE_NEIGHBOR,
    NON_CHORD_TONE_DOUBLE_NEIGHBOR,
    NON_CHORD_TONE_CHANGING_TONE,
    NON_CHORD_TONE_SUSPENSION,
    NON_CHORD_TONE_COUNT
} NonChordTone;

/* True for the five figures above; false for any other value. */
bool non_chord_tone_valid(NonChordTone type);

/* "passing" "neighbor" "double-neighbor" "changing-tone" "suspension"; NULL for
   an invalid type. */
const char *non_chord_tone_name(NonChordTone type);

/* True for the figures that include a leap: the double neighbor (between its
   two neighbors) and the changing tone (out of its dissonance). False for the
   stepwise ones (the suspension resolves by step), and for an invalid
   type. */
bool non_chord_tone_involves_leap(NonChordTone type);

#endif
