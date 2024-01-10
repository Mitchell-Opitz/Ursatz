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
 * Between the notes on strong beats the notes of the weak beats are chord tones, or non-chord tones of a figure
 * params.nct_types allows, approached and left as the NonChordTone framework defines the figure. After the motive
 * the rhythm is the motive's again (params.density 0), or an even subdivision of the cell. The melody is written
 * as one Realized span over the phrase.
 *
 * The phrase is shaped as one arch: a single highest note, the climax, placed about two thirds of the way through
 * (params.climax_position), approached from below and left downward, with no other note as high. A leap of a
 * fourth or more is followed by a step the other way where it can be, notes are not repeated needlessly, and no
 * note leaves params.range_low to params.range_high.
 */

#define MELODY_FILL_OPERATOR_NAME "melody-fill"

/* The non-chord tones the operator may write, as bits of MelodyFillParams.nct_types. */
#define MELODY_NCT_PASSING (1u << 0)
#define MELODY_NCT_NEIGHBOR (1u << 1)      /* upper and lower */
#define MELODY_NCT_APPOGGIATURA (1u << 2)  /* approached by leap, left by step the other way */
#define MELODY_NCT_SUSPENSION (1u << 3)    /* held into the next chord, left by step down */
#define MELODY_NCT_ANTICIPATION (1u << 4)  /* sounds the next chord's note early, then repeats it */

/*
 * tolerance is how far, in semitones, an interval of the placed motive may differ from the motive's own interval
 * between the same two notes. range_low and range_high are MIDI pitches, both included. start_cell is the cell of
 * the phrase the motive starts on (0 is the first), the time before it a rest. nct_types is the set of
 * MELODY_NCT_ bits that may be written; none means every note is a chord tone. density is how many notes make
 * up a cell after the motive, or 0 for the motive's own rhythm over again. climax_position is how far through the
 * phrase the climax falls, in thousandths of its length (values above 1000 count as 1000).
 */
typedef struct {
    int tolerance;
    int range_low;
    int range_high;
    unsigned start_cell;
    unsigned nct_types;
    unsigned density;
    unsigned climax_position;
} MelodyFillParams;

/*
 * The defaults: tolerance 2, MIDI 60 to 79 (C4 to G5), start_cell 0, passing and neighbor tones only, the
 * motive's rhythm after the motive (density 0), and the climax 667 thousandths of the way through.
 */
MelodyFillParams melody_fill_params_default(void);

/*
 * Registers the operator with these parameters. The registry keeps the pointer, not a copy, so params must
 * outlive it (NULL means the defaults).
 */
FillResult melody_fill_register(FillOperatorRegistry *registry, const MelodyFillParams *params);

#endif
