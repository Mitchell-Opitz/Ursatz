#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_FIFTH_SPECIES_FIGURE_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_FIFTH_SPECIES_FIGURE_H

#include <stdbool.h>
#include <stddef.h>

#include "fifth_species_exercise.h"
#include "fourth_species_suspension.h"
#include "semantics/non_chord_tone.h"

/*
 * Dissonance figures for fifth species in standard-counterpoint-v1. A
 * dissonance never falls on a new note on the downbeat, and is only part of
 * one of these figures. A note is "consonant" or "dissonant" against the
 * cantus firmus note of the bar it sounds in (a perfect fourth is a
 * dissonance). The notes before and after a figure are the neighboring notes
 * of the counterpoint in time, and "consonant" for them is read at the slot
 * just before a figure and at the attack after it. A "step" is a major or
 * minor second; every melodic interval in a figure is a consonant melodic
 * interval (consonance_rules.h). Except for the suspension, no note of a
 * figure crosses the bar line.
 *
 *   PASSING_TONE         one dissonant note between two consonant notes,
 *                        approached and left by step in the same direction;
 *                        a quarter note on the second, third, or fourth beat,
 *                        or the second eighth of a pair
 *   DOUBLE_PASSING_TONE  two dissonant notes in a row between two consonant
 *                        notes, all three steps in the same direction
 *   NEIGHBOR_TONE        one dissonant note stepped to from a consonant note
 *                        and left by step back to the same pitch
 *   DOUBLE_NEIGHBOR      a consonant note, its upper and lower neighbors (in
 *                        either order, a third apart), both dissonant, and
 *                        the first note again
 *   NOTA_CAMBIATA_DOWN   five notes: a consonance, a dissonance a step below
 *                        it, a leap down a third to a consonance, a step up,
 *                        and a step up to a consonance that is the pitch of
 *                        the dissonance. Recorded on the dissonance
 *   NOTA_CAMBIATA_UP     the same figure inverted
 *   SUSPENSION           a note tied across the bar line that is dissonant
 *                        against the cantus firmus note after the bar line:
 *                        a consonant preparation, and a resolution one step
 *                        below it (the next note) that is consonant. It is a
 *                        7-6, 4-3, or 9-8 above the cantus firmus or a 2-3
 *                        below it (fourth_species_suspension.h). The held
 *                        note may last a quarter note, a half note, or a
 *                        dotted half note
 *   ANTICIPATION         a quarter note on the fourth beat, reached by step
 *                        down, that is dissonant against the cantus firmus
 *                        and is followed on the next downbeat by a note of
 *                        the same pitch that is consonant against the next
 *                        cantus firmus note
 *   ANTICIPATION_TURN    the same with eighth notes: the pair of eighths on
 *                        the fourth beat, the first a consonance and the
 *                        second the dissonant anticipation, reached by step
 *                        down
 *
 * Each figure maps to a NonChordTone (semantics/non_chord_tone.h): the two
 * passing figures to PASSING, the neighbor tone to NEIGHBOR, the double
 * neighbor to DOUBLE_NEIGHBOR, both nota cambiata forms to CHANGING_TONE,
 * the suspension to SUSPENSION, and both anticipations to ANTICIPATION.
 */

typedef enum {
    STANDARD_COUNTERPOINT_FIFTH_FIGURE_PASSING_TONE = 0,
    STANDARD_COUNTERPOINT_FIFTH_FIGURE_DOUBLE_PASSING_TONE,
    STANDARD_COUNTERPOINT_FIFTH_FIGURE_NEIGHBOR_TONE,
    STANDARD_COUNTERPOINT_FIFTH_FIGURE_DOUBLE_NEIGHBOR,
    STANDARD_COUNTERPOINT_FIFTH_FIGURE_NOTA_CAMBIATA_DOWN,
    STANDARD_COUNTERPOINT_FIFTH_FIGURE_NOTA_CAMBIATA_UP,
    STANDARD_COUNTERPOINT_FIFTH_FIGURE_SUSPENSION,
    STANDARD_COUNTERPOINT_FIFTH_FIGURE_ANTICIPATION,
    STANDARD_COUNTERPOINT_FIFTH_FIGURE_ANTICIPATION_TURN,
    STANDARD_COUNTERPOINT_FIFTH_FIGURE_COUNT
} StandardCounterpointFifthSpeciesFigure;

/* "passing-tone" "double-passing-tone" "neighbor-tone" "double-neighbor"
   "nota-cambiata-down" "nota-cambiata-up" "suspension" "anticipation"
   "anticipation-turn"; NULL for an invalid figure. */
const char *standard_counterpoint_fifth_species_figure_name(StandardCounterpointFifthSpeciesFigure figure);

/* The NonChordTone a figure is. NON_CHORD_TONE_COUNT for an invalid figure. */
NonChordTone standard_counterpoint_fifth_species_figure_non_chord_tone(StandardCounterpointFifthSpeciesFigure figure);

/* True when the counterpoint sounds a dissonance against the cantus firmus in
   that slot (a new note or a tie). False for a consonance, a rest, and invalid
   input. */
bool standard_counterpoint_fifth_species_slot_is_dissonant(const StandardCounterpointFifthSpeciesExercise *exercise,
                                                           size_t slot);

/* The figure a dissonance belongs to. True and *figure set when the slot
   sounds a dissonance that is part of a figure: a new note not on the
   downbeat, one of its ties within the same bar, or the tie of a suspension on
   the downbeat. False (*figure untouched) when the slot is consonant, a new
   note on a downbeat, part of no figure, or the input is invalid. */
bool standard_counterpoint_fifth_species_dissonance_figure(const StandardCounterpointFifthSpeciesExercise *exercise,
                                                           size_t slot, StandardCounterpointFifthSpeciesFigure *figure);

/* The suspension type (fourth_species_suspension.h) of the suspension whose
   tie is the downbeat slot. True and *type set for a dissonant suspension;
   false otherwise. */
bool standard_counterpoint_fifth_species_suspension_type(const StandardCounterpointFifthSpeciesExercise *exercise,
                                                         size_t slot, StandardCounterpointSuspensionType *type);

#endif
