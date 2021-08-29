#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_THIRD_SPECIES_FIGURE_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_THIRD_SPECIES_FIGURE_H

#include <stdbool.h>
#include <stddef.h>

#include "semantics/non_chord_tone.h"
#include "third_species_exercise.h"

/*
 * Dissonance figures for standard-counterpoint-v1. In third species a
 * dissonance may fall on beat 2, 3, or 4 of a bar, never on the downbeat,
 * and only as part of one of these figures. Beats are numbered from 0 here
 * (beat 1 of the bar is 0). A note is "consonant" or "dissonant" against the
 * cantus firmus note of its bar. A "step" is a major or minor second, a
 * "leap of a third" a major or minor third, and every melodic interval in a
 * figure is a consonant melodic interval (consonance_rules.h).
 *
 *   PASSING_TONE         one dissonant note between two consonant notes,
 *                        approached and left by step in the same direction,
 *                        so it fills in a third
 *   DOUBLE_PASSING_TONE  two dissonant notes in a row between two consonant
 *                        notes (a fourth and a diminished fifth over the
 *                        cantus firmus, say), all three steps in the same
 *                        direction; neither is on a downbeat
 *   NEIGHBOR_TONE        one dissonant note stepped to from a consonant note
 *                        and left by step back to the same pitch, which is
 *                        consonant
 *   DOUBLE_NEIGHBOR      beats 1 and 4 of the bar are the same consonant
 *                        pitch, and beats 2 and 3 are its upper and lower
 *                        neighbors, in either order, both dissonant, a leap
 *                        of a third apart; the direction from beat 3 to beat
 *                        4 is the direction from beat 4 to the next
 *                        downbeat, which is a step
 *   NOTA_CAMBIATA_DOWN   a five-note figure over a bar and the next downbeat
 *                        that fills in a step down: the downbeat note n1, a
 *                        dissonance n2 a step below it, a leap down a third to
 *                        n3, a step up to n4, a step up to the next downbeat
 *                        n5, which is the pitch of n2. n1, n3, and n5 are
 *                        consonant; n4 may be either. The figure is
 *                        recorded on the dissonance n2
 *   NOTA_CAMBIATA_UP     the same figure inverted: a step up to the
 *                        dissonance, a leap up a third, two steps down, to a
 *                        pitch a step above n1
 *
 * Each figure maps to a NonChordTone (semantics/non_chord_tone.h): the two
 * passing figures to PASSING, the neighbor tone to NEIGHBOR, the double
 * neighbor to DOUBLE_NEIGHBOR, and both nota cambiata forms to
 * CHANGING_TONE.
 */

typedef enum {
    STANDARD_COUNTERPOINT_FIGURE_PASSING_TONE = 0,
    STANDARD_COUNTERPOINT_FIGURE_DOUBLE_PASSING_TONE,
    STANDARD_COUNTERPOINT_FIGURE_NEIGHBOR_TONE,
    STANDARD_COUNTERPOINT_FIGURE_DOUBLE_NEIGHBOR,
    STANDARD_COUNTERPOINT_FIGURE_NOTA_CAMBIATA_DOWN,
    STANDARD_COUNTERPOINT_FIGURE_NOTA_CAMBIATA_UP,
    STANDARD_COUNTERPOINT_FIGURE_COUNT
} StandardCounterpointDissonanceFigure;

/* "passing-tone" "double-passing-tone" "neighbor-tone" "double-neighbor"
   "nota-cambiata-down" "nota-cambiata-up"; NULL for an invalid figure. */
const char *standard_counterpoint_dissonance_figure_name(StandardCounterpointDissonanceFigure figure);

/* The NonChordTone a figure is. NON_CHORD_TONE_COUNT for an invalid figure. */
NonChordTone standard_counterpoint_dissonance_figure_non_chord_tone(StandardCounterpointDissonanceFigure figure);

/* True when the counterpoint sounds a dissonance against the cantus firmus on
   that beat of that bar (a new note or a tie). False for a consonance, a
   rest, and invalid input. */
bool standard_counterpoint_third_species_beat_is_dissonant(const StandardCounterpointThirdSpeciesExercise *exercise,
                                                           size_t bar, size_t beat);

/* The figure a dissonant note belongs to. True and *figure set when the
   note is a dissonant new note on beat 2, 3, or 4 (beat 1, 2, or 3 here) that
   is part of a figure; false (*figure untouched) when it is consonant, on a
   downbeat, part of no figure, or the input is invalid. */
bool standard_counterpoint_third_species_dissonance_figure(const StandardCounterpointThirdSpeciesExercise *exercise,
                                                           size_t bar, size_t beat,
                                                           StandardCounterpointDissonanceFigure *figure);

#endif
