#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_FOURTH_SPECIES_EXERCISE_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_FOURTH_SPECIES_EXERCISE_H

#include <stdbool.h>
#include <stddef.h>

#include "counterpoint_interval.h"
#include "counterpoint_line.h"
#include "counterpoint_rules.h"
#include "counterpoint_slots.h"
#include "pitch/pitch_spelling.h"
#include "semantics/beat_strength.h"

/*
 * A fourth-species exercise for standard-counterpoint-v1: the counterpoint
 * and the cantus firmus both move once a bar, but the counterpoint is offset
 * from the cantus firmus by a half note, in syncopation: half notes, each
 * weak-beat note tied across the bar line to the next strong beat. The
 * counterpoint is a list of slots (counterpoint_slots.h), two per bar
 * (SLOTS_PER_BAR), one bar for each note of the cantus firmus. Both slots of
 * bar i sound against cantus firmus note i. The first slot of a bar is the
 * strong beat (the downbeat), the second the weak beat.
 *
 * A valid exercise has these shapes:
 *
 *   the cantus firmus   a valid line of at least three and at most MAX_BARS
 *                       notes (the first bar, a penultimate bar, and a last
 *                       bar at the least)
 *   the first bar       a half rest and then an attack on the weak beat
 *   a middle bar        an attack on the weak beat, and on the strong beat
 *                       either a tie from the weak beat before it (the
 *                       syncopation of this species, with that pitch) or a
 *                       new note (a bar in which species is broken)
 *   the last bar        a whole note: an attack on the strong beat and a tie
 *                       on the weak beat, with that pitch
 *
 * Which part is the cantus firmus is stated by cantus_part, as for the common
 * check (counterpoint_rules.h).
 *
 * Every accessor below assumes a valid exercise
 * (fourth_species_exercise_valid) and a bar in range, and returns a neutral
 * value (NULL, false, 0) otherwise.
 */

#define STANDARD_COUNTERPOINT_FOURTH_SPECIES_SLOTS_PER_BAR 2
#define STANDARD_COUNTERPOINT_FOURTH_SPECIES_MAX_BARS 64

typedef struct {
    const StandardCounterpointLine *cantus;
    const StandardCounterpointSlotLine *counterpoint;
    StandardCounterpointCantusPart cantus_part;
} StandardCounterpointFourthSpeciesExercise;

/* True when the exercise has the shapes described above. */
bool standard_counterpoint_fourth_species_exercise_valid(const StandardCounterpointFourthSpeciesExercise *exercise);

/* The number of bars (cantus firmus notes); 0 for an invalid exercise. */
size_t standard_counterpoint_fourth_species_bar_count(const StandardCounterpointFourthSpeciesExercise *exercise);

/* The metric strength of a slot: even slots strong, odd slots weak. */
BeatStrength standard_counterpoint_fourth_species_slot_strength(size_t slot);

/* The cantus firmus note of the bar. */
const PitchSpelling *standard_counterpoint_fourth_species_cantus(const StandardCounterpointFourthSpeciesExercise *exercise,
                                                                 size_t bar);

/* The counterpoint pitch on the strong beat of the bar, whether a tie or a
   new note, or NULL for the first bar (a half rest). */
const PitchSpelling *standard_counterpoint_fourth_species_downbeat(
    const StandardCounterpointFourthSpeciesExercise *exercise, size_t bar);

/* The counterpoint pitch on the weak beat of the bar: a new note, or the
   tie of the last bar. */
const PitchSpelling *standard_counterpoint_fourth_species_weak(const StandardCounterpointFourthSpeciesExercise *exercise,
                                                               size_t bar);

/* True when the strong beat of the bar is a tie from the weak beat before
   it. False for the first bar, for a new note, and for invalid input. */
bool standard_counterpoint_fourth_species_downbeat_is_tie(const StandardCounterpointFourthSpeciesExercise *exercise,
                                                          size_t bar);

/* True when species is broken in the bar: a middle bar whose strong beat is a
   new note, not a tie. False for the first and last bar, for a tied bar, and
   for invalid input. */
bool standard_counterpoint_fourth_species_bar_is_broken(const StandardCounterpointFourthSpeciesExercise *exercise,
                                                        size_t bar);

/* The interval between the cantus firmus note of the bar and the counterpoint
   on its strong beat, or on its weak beat. False (*out untouched) when the
   beat is a rest or the input is invalid. */
bool standard_counterpoint_fourth_species_downbeat_interval(const StandardCounterpointFourthSpeciesExercise *exercise,
                                                            size_t bar, StandardCounterpointInterval *out);
bool standard_counterpoint_fourth_species_weak_interval(const StandardCounterpointFourthSpeciesExercise *exercise,
                                                        size_t bar, StandardCounterpointInterval *out);

/*
 * The counterpoint's attacks in order: its new notes, with the rest and the
 * ties left out. Writes up to `capacity` pitches to `notes` and the slot of
 * each to `slots`, and the number of attacks to *count. A buffer of
 * 2 * MAX_BARS entries is always enough. False (outputs untouched) for an
 * invalid exercise or a capacity that is too small.
 */
bool standard_counterpoint_fourth_species_attacks(const StandardCounterpointFourthSpeciesExercise *exercise,
                                                  const PitchSpelling **notes, size_t *slots, size_t capacity,
                                                  size_t *count);

#endif
