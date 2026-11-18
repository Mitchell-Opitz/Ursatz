#ifndef URSATZ_REALIZATION_PIANO_REDUCTION_H
#define URSATZ_REALIZATION_PIANO_REDUCTION_H

#include "realization/movement.h"
#include "realization/realization.h"
#include "theory/knowledge/instrument.h"

/*
 * The piano reduction (R-599): every line of a movement assigned to one piano part, the rule the Composer's export uses. It is the
 * only realization header that includes the K12 instrument.
 *
 * Refused with BAD_VALUE, and nothing changed, when the movement already holds parts or assignments or the instrument has fewer than
 * 2 staves. Otherwise it makes the part `piano` (named as the instrument, number 1, one player, the instrument's id and General MIDI
 * program) and one whole-line assignment `a-<line id>` per line, in line order: staff 1 for a melody line, staff 2 for a bass line,
 * otherwise staff 1 when the mean MIDI number of the line's notes is 60 or more (a line with no notes: staff 1) and staff 2 below
 * that; the voice is the line's index plus 1. The part has 2 staves when any line is on staff 2, else 1 (a motif stays on one staff).
 * Then it scores the movement with `run`.
 */
RealizationResult movement_reduce_to_piano(Movement *movement, const Instrument *piano, const char *run);

#endif
