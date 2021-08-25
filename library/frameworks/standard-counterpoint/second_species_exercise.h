#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_SECOND_SPECIES_EXERCISE_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_SECOND_SPECIES_EXERCISE_H

#include <stdbool.h>
#include <stddef.h>

#include "counterpoint_interval.h"
#include "counterpoint_line.h"
#include "counterpoint_rules.h"
#include "counterpoint_slots.h"
#include "pitch/pitch_spelling.h"
#include "semantics/beat_strength.h"

/*
 * A second-species exercise for standard-counterpoint-v1: the counterpoint
 * moves twice as fast as the cantus firmus, two half notes against each
 * whole note. There is no rhythm model, so the counterpoint is a list of
 * slots, two per bar (SLOTS_PER_BAR), one bar for each note of the cantus
 * firmus. The first slot of a bar is the strong beat (the downbeat), the
 * second the weak beat. Both slots of bar i sound against cantus firmus
 * note i.
 *
 * A slot is one of:
 *
 *   ATTACK   a new note, with its pitch
 *   TIE      a continuation of the previous slot's note, with that same
 *            pitch: a whole note is an attack on the strong beat and a tie
 *            on the weak beat
 *   REST     silence, with no pitch
 *
 * A valid exercise has these shapes: the cantus firmus is a valid line of
 * at least two and at most MAX_BARS notes; the counterpoint has exactly two
 * slots per cantus firmus note; a REST is allowed only in the first slot (a
 * half rest on the first beat); a TIE is allowed only on a weak beat, and
 * repeats the pitch of the slot before it. Rests and ties anywhere else
 * belong to other species. Which part is the cantus firmus is stated by
 * cantus_part, as for the common check (counterpoint_rules.h).
 *
 * Pitches are borrowed, not owned. Every accessor below assumes a valid
 * exercise (second_species_exercise_valid) and a bar in range, and returns
 * a neutral value (NULL, false, 0) otherwise.
 */

#define STANDARD_COUNTERPOINT_SECOND_SPECIES_SLOTS_PER_BAR 2
#define STANDARD_COUNTERPOINT_SECOND_SPECIES_MAX_BARS 64

typedef struct {
    const StandardCounterpointLine *cantus;
    const StandardCounterpointSlotLine *counterpoint;
    StandardCounterpointCantusPart cantus_part;
} StandardCounterpointSecondSpeciesExercise;

/* True when the exercise has the shapes described above. */
bool standard_counterpoint_second_species_exercise_valid(const StandardCounterpointSecondSpeciesExercise *exercise);

/* The number of bars (cantus firmus notes); 0 for an invalid exercise. */
size_t standard_counterpoint_second_species_bar_count(const StandardCounterpointSecondSpeciesExercise *exercise);

/* The metric strength of a slot: even slots strong, odd slots weak. */
BeatStrength standard_counterpoint_second_species_slot_strength(size_t slot);

/* The cantus firmus note of the bar. */
const PitchSpelling *standard_counterpoint_second_species_cantus(const StandardCounterpointSecondSpeciesExercise *exercise,
                                                                 size_t bar);

/* The counterpoint pitch on the strong beat of the bar, or NULL when it is
   a rest. */
const PitchSpelling *standard_counterpoint_second_species_downbeat(
    const StandardCounterpointSecondSpeciesExercise *exercise, size_t bar);

/* The counterpoint pitch on the weak beat of the bar, whether it is a new
   note or a tie. */
const PitchSpelling *standard_counterpoint_second_species_weak(const StandardCounterpointSecondSpeciesExercise *exercise,
                                                               size_t bar);

/* True when the weak beat of the bar is a new note (not a tie). */
bool standard_counterpoint_second_species_weak_is_attack(const StandardCounterpointSecondSpeciesExercise *exercise,
                                                         size_t bar);

/* The interval between the cantus firmus note of the bar and the
   counterpoint on its strong beat, or on its weak beat. False (*out
   untouched) when the beat is a rest or the input is invalid. */
bool standard_counterpoint_second_species_downbeat_interval(const StandardCounterpointSecondSpeciesExercise *exercise,
                                                            size_t bar, StandardCounterpointInterval *out);
bool standard_counterpoint_second_species_weak_interval(const StandardCounterpointSecondSpeciesExercise *exercise,
                                                        size_t bar, StandardCounterpointInterval *out);

/*
 * The counterpoint's attacks in order: its new notes, with rests and ties
 * left out. Writes up to `capacity` pitches to `notes` and the slot of each
 * to `slots`, and the number of attacks to *count. A buffer of
 * 2 * MAX_BARS entries is always enough. False (outputs untouched) for an
 * invalid exercise or a capacity that is too small.
 */
bool standard_counterpoint_second_species_attacks(const StandardCounterpointSecondSpeciesExercise *exercise,
                                                  const PitchSpelling **notes, size_t *slots, size_t capacity,
                                                  size_t *count);

#endif
