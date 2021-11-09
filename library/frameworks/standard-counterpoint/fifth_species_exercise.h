#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_FIFTH_SPECIES_EXERCISE_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_FIFTH_SPECIES_EXERCISE_H

#include <stdbool.h>
#include <stddef.h>

#include "counterpoint_interval.h"
#include "counterpoint_line.h"
#include "counterpoint_rules.h"
#include "counterpoint_slots.h"
#include "pitch/pitch_spelling.h"
#include "semantics/beat_strength.h"

/*
 * A fifth-species (florid) exercise for standard-counterpoint-v1: the
 * counterpoint mixes the note values of the first four species against the
 * cantus firmus, and adds eighth notes. The counterpoint is a list of slots
 * (counterpoint_slots.h) on an eighth-note grid, eight to a bar
 * (SLOTS_PER_BAR), one bar for each note of the cantus firmus. All eight slots
 * of bar i sound against cantus firmus note i. A slot's position in its bar
 * is its eighth, 0 to 7; the quarter-note beats begin at the even eighths.
 *
 * The strengths are those of four-four time: the first eighth is strong, the
 * fifth (the third beat) is medium, and all the others are weak.
 *
 * A note is an attack and the ties that follow it. A valid exercise has these
 * shapes:
 *
 *   the cantus firmus   a valid line of at least three and at most MAX_BARS
 *                       notes
 *   the first bar       may begin with a rest of one, two, or three quarters
 *                       (two, four, or six eighths); no other slot is a rest
 *   a note              lasts 1, 2, 4, 6, or 8 eighths:
 *                         an eighth note is one of a pair, filling the
 *                         second or fourth quarter beat (eighths 2 and 3, or
 *                         6 and 7), and the pair is two new notes;
 *                         every longer note starts on a quarter beat;
 *                         a note that crosses the bar line (a syncopation) is
 *                         a half note or dotted half note tied from the third
 *                         beat, a whole note tied from the third beat, or a
 *                         quarter note tied from the fourth beat that lasts a
 *                         half note or a dotted half note
 *   the last bar        a whole note: one attack on the first eighth and
 *                       seven ties; no note crosses into it
 *
 * Which part is the cantus firmus is stated by cantus_part, as for the common
 * check (counterpoint_rules.h).
 *
 * Every accessor below assumes a valid exercise
 * (fifth_species_exercise_valid) and returns a neutral value (NULL, false, 0)
 * otherwise or for a slot or bar out of range.
 */

#define STANDARD_COUNTERPOINT_FIFTH_SPECIES_SLOTS_PER_BAR 8
#define STANDARD_COUNTERPOINT_FIFTH_SPECIES_MAX_BARS 64
/* The most notes a valid exercise can have: one for each slot. */
#define STANDARD_COUNTERPOINT_FIFTH_SPECIES_MAX_NOTES \
    (STANDARD_COUNTERPOINT_FIFTH_SPECIES_SLOTS_PER_BAR * STANDARD_COUNTERPOINT_FIFTH_SPECIES_MAX_BARS)

typedef struct {
    const StandardCounterpointLine *cantus;
    const StandardCounterpointSlotLine *counterpoint;
    StandardCounterpointCantusPart cantus_part;
} StandardCounterpointFifthSpeciesExercise;

/* One note of the counterpoint: where it starts, how long it lasts in eighths,
   and its pitch. The bar is the one it starts in. */
typedef struct {
    size_t slot;
    size_t duration;
    const PitchSpelling *pitch;
} StandardCounterpointFifthSpeciesNote;

/* True when the exercise has the shapes described above. */
bool standard_counterpoint_fifth_species_exercise_valid(const StandardCounterpointFifthSpeciesExercise *exercise);

/* The number of bars (cantus firmus notes); 0 for an invalid exercise. */
size_t standard_counterpoint_fifth_species_bar_count(const StandardCounterpointFifthSpeciesExercise *exercise);

/* The metric strength of a slot: the first eighth of a bar strong, the fifth
   medium, the rest weak. */
BeatStrength standard_counterpoint_fifth_species_slot_strength(size_t slot);

/* The cantus firmus note of the bar, or NULL. */
const PitchSpelling *standard_counterpoint_fifth_species_cantus(const StandardCounterpointFifthSpeciesExercise *exercise,
                                                                size_t bar);

/* The pitch sounding in a slot, a tie included, or NULL for a rest or a slot
   out of range. */
const PitchSpelling *standard_counterpoint_fifth_species_pitch(const StandardCounterpointFifthSpeciesExercise *exercise,
                                                               size_t slot);

/* True when the slot is a new note (an attack) / a tie. */
bool standard_counterpoint_fifth_species_slot_is_attack(const StandardCounterpointFifthSpeciesExercise *exercise,
                                                        size_t slot);
bool standard_counterpoint_fifth_species_slot_is_tie(const StandardCounterpointFifthSpeciesExercise *exercise,
                                                     size_t slot);

/* The interval between the cantus firmus note of the slot's bar and the
   counterpoint sounding in the slot. False (*out untouched) for a rest or
   invalid input. */
bool standard_counterpoint_fifth_species_interval(const StandardCounterpointFifthSpeciesExercise *exercise, size_t slot,
                                                  StandardCounterpointInterval *out);

/*
 * The counterpoint's notes in order, with the leading rest left out. Writes up
 * to `capacity` notes to `notes` and their number to *count. A buffer of
 * MAX_NOTES entries is always enough. False (outputs untouched) for an invalid
 * exercise or a capacity that is too small.
 */
bool standard_counterpoint_fifth_species_notes(const StandardCounterpointFifthSpeciesExercise *exercise,
                                               StandardCounterpointFifthSpeciesNote *notes, size_t capacity,
                                               size_t *count);

/*
 * The counterpoint's attacks in order: its new notes, with the rest and the
 * ties left out. Writes up to `capacity` pitches to `pitches` and the slot of
 * each to `slots`, and the number of attacks to *count. A buffer of MAX_NOTES
 * entries is always enough. False (outputs untouched) for an invalid exercise
 * or a capacity that is too small.
 */
bool standard_counterpoint_fifth_species_attacks(const StandardCounterpointFifthSpeciesExercise *exercise,
                                                 const PitchSpelling **pitches, size_t *slots, size_t capacity,
                                                 size_t *count);

#endif
