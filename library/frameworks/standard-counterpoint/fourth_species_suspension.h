#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_FOURTH_SPECIES_SUSPENSION_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_FOURTH_SPECIES_SUSPENSION_H

#include <stdbool.h>
#include <stddef.h>

#include "fourth_species_exercise.h"
#include "semantics/non_chord_tone.h"

/*
 * Suspensions for standard-counterpoint-v1. In fourth species the
 * counterpoint's weak-beat note is tied across the bar line to the next
 * strong beat. Seen from a bar b whose downbeat is a tie, the figure has
 * three parts, all in the counterpoint:
 *
 *   preparation   the weak-beat note of the bar before (b - 1), consonant
 *                 with the cantus firmus note it sounds with
 *   suspension    the downbeat of bar b, tied from the preparation and so
 *                 the same pitch, now against the next cantus firmus note
 *   resolution    the weak-beat note of bar b, one step below the
 *                 suspension and consonant with the cantus firmus
 *
 * The suspension is dissonant against the cantus firmus when it is a
 * dissonant suspension, and consonant when it is only a tie. The type is
 * named by the simple sizes of the suspension and the resolution against the
 * cantus firmus (compound intervals count as simple), the counterpoint being
 * above or below it:
 *
 *   SEVEN_SIX      above the cantus firmus: 7 resolves to 6
 *   FOUR_THREE     above the cantus firmus: 4 resolves to 3
 *   NINE_EIGHT     above the cantus firmus: 9 (a second) resolves to an
 *                  octave, or 2 to 1 (a unison)
 *   TWO_THREE      below the cantus firmus: 2 resolves to 3
 *   CONSONANT_TIE  a tied note that is consonant with the cantus firmus,
 *                  resolving by step down to a consonance (5-6 below, 6-5
 *                  above, and the like): a consonant suspension
 *
 * A dissonant suspension of any other kind is not a proper suspension, and
 * neither is one whose preparation or resolution is dissonant, or whose
 * resolution is not a step down. Only a middle bar can hold a suspension: the
 * last bar is a whole note, and the first has nothing before it.
 *
 * The dissonant types map to NON_CHORD_TONE_SUSPENSION
 * (semantics/non_chord_tone.h).
 */

typedef enum {
    STANDARD_COUNTERPOINT_SUSPENSION_SEVEN_SIX = 0,
    STANDARD_COUNTERPOINT_SUSPENSION_FOUR_THREE,
    STANDARD_COUNTERPOINT_SUSPENSION_NINE_EIGHT,
    STANDARD_COUNTERPOINT_SUSPENSION_TWO_THREE,
    STANDARD_COUNTERPOINT_SUSPENSION_CONSONANT_TIE,
    STANDARD_COUNTERPOINT_SUSPENSION_COUNT
} StandardCounterpointSuspensionType;

/* "7-6" "4-3" "9-8" "2-3" "consonant-tie"; NULL for an invalid type. */
const char *standard_counterpoint_suspension_name(StandardCounterpointSuspensionType type);

/* True for the four dissonant suspensions, false for the consonant tie and for
   an invalid type. */
bool standard_counterpoint_suspension_is_dissonant(StandardCounterpointSuspensionType type);

/* The NonChordTone of a type: NON_CHORD_TONE_SUSPENSION for a dissonant
   suspension, NON_CHORD_TONE_COUNT for the consonant tie (no dissonance) and
   for an invalid type. */
NonChordTone standard_counterpoint_suspension_non_chord_tone(StandardCounterpointSuspensionType type);

/* The suspension at the downbeat of the bar. True and *type set when the bar's
   downbeat is a tie and the bar is a proper suspension or consonant tie as
   described above; false (*type untouched) otherwise, and for invalid input. */
bool standard_counterpoint_fourth_species_suspension(const StandardCounterpointFourthSpeciesExercise *exercise,
                                                     size_t bar, StandardCounterpointSuspensionType *type);

#endif
