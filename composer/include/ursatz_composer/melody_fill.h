#ifndef URSATZ_COMPOSER_MELODY_FILL_H
#define URSATZ_COMPOSER_MELODY_FILL_H

#include <stdbool.h>

#include <ursatz_composer/fill_operator.h>
#include <ursatz_composer/lanes.h>

/*
 * The melody operator: a Fill operator for the melody lane. It reads a harmony lane and a bass lane that are
 * already written (Realized or Given, never open) and writes the melody over them, carrying a motive's rhythm and
 * contour against the harmony.
 *
 * A target is a Constrained melody lane span (voice 0) that stands for the phrase and holds one placeholder note.
 * It must cover exactly the chord spans under it, which the bass spans tile, lie inside one meter lane span, have
 * a key lane span or key constraint over it, and carry a motive_ref constraint (constraint.h) on the span, or on
 * its owner, naming a motive node whose melody lane span is Given or Realized. The motive is the original when
 * that span is Given and a development (motive_develop.h) when it is Realized; the candidate's label says which.
 * Anything else, the operator does not apply to. The loop never offers a Given span, so Given melody is never
 * overwritten, and the harmony and bass lanes are never touched.
 *
 * The motive is placed at the start of the phrase (params.start_cell), keeping its rhythm. Each of its notes takes
 * a pitch that keeps the motive's interval to the note before within params.tolerance, and a note on a strong
 * beat (the start of a cell: the places a chord may start) is a chord tone of the harmony lane's chord. The
 * motive is transposed so that its first note is a chord tone, keeping its scale degree when that degree is one.
 * The rest of the phrase is filled with notes that are chord tones. The melody is written as one Realized span
 * over the phrase.
 */

#define MELODY_FILL_OPERATOR_NAME "melody-fill"

/*
 * tolerance is how far, in semitones, an interval of the placed motive may differ from the motive's own interval
 * between the same two notes. range_low and range_high are MIDI pitches, both included. start_cell is the cell of
 * the phrase the motive starts on (0 is the first), the time before it a rest.
 */
typedef struct {
    int tolerance;
    int range_low;
    int range_high;
    unsigned start_cell;
} MelodyFillParams;

/* The defaults: tolerance 2, MIDI 60 to 79 (C4 to G5), start_cell 0. */
MelodyFillParams melody_fill_params_default(void);

/*
 * Registers the operator with these parameters. The registry keeps the pointer, not a copy, so params must
 * outlive it (NULL means the defaults).
 */
FillResult melody_fill_register(FillOperatorRegistry *registry, const MelodyFillParams *params);

#endif
