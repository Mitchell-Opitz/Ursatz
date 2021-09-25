#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_THIRD_SPECIES_EXERCISE_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_THIRD_SPECIES_EXERCISE_H

#include <stdbool.h>
#include <stddef.h>

#include "counterpoint_interval.h"
#include "counterpoint_line.h"
#include "counterpoint_rules.h"
#include "counterpoint_slots.h"
#include "pitch/pitch_spelling.h"
#include "semantics/beat_strength.h"

/*
 * A third-species exercise for standard-counterpoint-v1: the counterpoint
 * moves four times as fast as the cantus firmus, four quarter notes against
 * each whole note. The counterpoint is a list of slots (counterpoint_slots.h),
 * four per bar (SLOTS_PER_BAR), one bar for each note of the cantus firmus.
 * All four slots of bar i sound against cantus firmus note i. The four
 * beats of a bar are, in order, strong (the downbeat), weak, medium, and
 * weak (BeatStrength).
 *
 * A valid exercise has these shapes: the cantus firmus is a valid line of at
 * least two and at most MAX_BARS notes; the counterpoint has exactly four
 * slots per cantus firmus note; and every bar is one of
 *
 *   four quarter notes   four attacks; the first bar may instead be a
 *                        quarter rest followed by three attacks
 *   a whole note         an attack on the first beat and three ties, each
 *                        with the pitch of the slot before it
 *
 * No other rhythm (half notes, dotted notes, a rest anywhere else) belongs
 * to third species. Which part is the cantus firmus is stated by
 * cantus_part, as for the common check (counterpoint_rules.h).
 *
 * Every accessor below assumes a valid exercise
 * (third_species_exercise_valid) and a bar and beat in range, and returns a
 * neutral value (NULL, false, 0) otherwise.
 */

#define STANDARD_COUNTERPOINT_THIRD_SPECIES_SLOTS_PER_BAR 4
#define STANDARD_COUNTERPOINT_THIRD_SPECIES_MAX_BARS 64

typedef struct {
    const StandardCounterpointLine *cantus;
    const StandardCounterpointSlotLine *counterpoint;
    StandardCounterpointCantusPart cantus_part;
} StandardCounterpointThirdSpeciesExercise;

/* True when the exercise has the shapes described above. */
bool standard_counterpoint_third_species_exercise_valid(const StandardCounterpointThirdSpeciesExercise *exercise);

/* The number of bars (cantus firmus notes); 0 for an invalid exercise. */
size_t standard_counterpoint_third_species_bar_count(const StandardCounterpointThirdSpeciesExercise *exercise);

/* The metric strength of a slot: slot 0 of a bar strong, 1 weak, 2 medium,
   3 weak. */
BeatStrength standard_counterpoint_third_species_slot_strength(size_t slot);

/* The cantus firmus note of the bar. */
const PitchSpelling *standard_counterpoint_third_species_cantus(const StandardCounterpointThirdSpeciesExercise *exercise,
                                                                size_t bar);

/* The counterpoint pitch on beat `beat` (0 to 3, beat 1 being 0) of the bar,
   whether it is a new note or a tie; NULL when it is a rest. */
const PitchSpelling *standard_counterpoint_third_species_beat(const StandardCounterpointThirdSpeciesExercise *exercise,
                                                              size_t bar, size_t beat);

/* True when that beat is a new note (not a tie, not a rest). */
bool standard_counterpoint_third_species_beat_is_attack(const StandardCounterpointThirdSpeciesExercise *exercise,
                                                        size_t bar, size_t beat);

/* The interval between the cantus firmus note of the bar and the
   counterpoint on that beat. False (*out untouched) when the beat is a rest
   or the input is invalid. */
bool standard_counterpoint_third_species_beat_interval(const StandardCounterpointThirdSpeciesExercise *exercise,
                                                       size_t bar, size_t beat, StandardCounterpointInterval *out);

/*
 * The counterpoint's attacks in order: its new notes, with rests and ties
 * left out. Writes up to `capacity` pitches to `notes` and the slot of each
 * to `slots`, and the number of attacks to *count. A buffer of
 * 4 * MAX_BARS entries is always enough. False (outputs untouched) for an
 * invalid exercise or a capacity that is too small.
 */
bool standard_counterpoint_third_species_attacks(const StandardCounterpointThirdSpeciesExercise *exercise,
                                                 const PitchSpelling **notes, size_t *slots, size_t capacity,
                                                 size_t *count);

#endif
