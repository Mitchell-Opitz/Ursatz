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
 *   ANTICIPATION     a note that arrives early: it sounds the pitch of the
 *                    note that follows it, on a weaker beat before that note,
 *                    and is dissonant against the harmony or the other part
 *                    until the harmony moves to it
 *   APPOGGIATURA     a note approached by leap and left by step in the opposite
 *                    direction, typically on a stronger part of the beat than
 *                    the notes around it; more often left downward than upward
 *   ESCAPE_TONE      a note approached by step and left by leap in the opposite
 *                    direction, typically on a weaker part of the beat than the
 *                    notes around it; more often left downward than upward
 *   RETARDATION      like the suspension, a note held over (approached by a
 *                    static note) into a position on a stronger beat, but it
 *                    resolves by stepping up
 *   PEDAL            a static note, often in the bass, held under a series of
 *                    chord changes that do not include it; it is approached and
 *                    left by static notes, and labeled by its scale degree
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
    NON_CHORD_TONE_ANTICIPATION,
    NON_CHORD_TONE_APPOGGIATURA,
    NON_CHORD_TONE_ESCAPE_TONE,
    NON_CHORD_TONE_RETARDATION,
    NON_CHORD_TONE_PEDAL,
    NON_CHORD_TONE_COUNT
} NonChordTone;

/* True for the ten figures above; false for any other value. */
bool non_chord_tone_valid(NonChordTone type);

/* "passing" "neighbor" "double-neighbor" "changing-tone" "suspension" "anticipation"
   "appoggiatura" "escape-tone" "retardation" "pedal"; NULL for an invalid type. */
const char *non_chord_tone_name(NonChordTone type);

/* True for the figures that include a leap: the double neighbor (between its
   two neighbors), the changing tone (out of its dissonance), the appoggiatura
   (into it), and the escape tone (out of it). False for the stepwise ones (the
   suspension and retardation resolve by step, and the anticipation repeats a
   pitch), for the pedal, and for an invalid type. */
bool non_chord_tone_involves_leap(NonChordTone type);

/* True for the figures that involve static notes (notes that do not move): the
   suspension, retardation, pedal, and anticipation. False for the others and for
   an invalid type. */
bool non_chord_tone_involves_static_note(NonChordTone type);

#endif
