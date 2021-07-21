#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_SECOND_SPECIES_PATTERN_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_SECOND_SPECIES_PATTERN_H

#include <stdbool.h>
#include <stddef.h>

#include "second_species_exercise.h"

/*
 * Weak-beat patterns for standard-counterpoint-v1. In second species the
 * counterpoint passes from one downbeat to the next by way of its weak
 * beat. Seen from the downbeat d1 of a bar, the weak beat w, and the next
 * downbeat d2, the bar is one of these patterns. Intervals are melodic,
 * counted in letter names (a step is a second, a third is a leap of a
 * third), and every melodic interval must be a consonant one
 * (consonance_rules.h). "Consonant" for the notes means consonant with the
 * cantus firmus.
 *
 *   CONSONANT_PASSING_TONE   two steps in the same direction outline a
 *                            third; d1, w, and d2 are all consonant
 *   SUBSTITUTION             a leap of a fourth then a step in the
 *                            opposite direction outline a third; all
 *                            three consonant
 *   SKIPPED_PASSING_TONE     a leap of a third and a step in the same
 *                            direction, in either order, outline a fourth;
 *                            all three consonant
 *   INTERVAL_SUBDIVISION     two leaps in the same direction divide a fifth
 *                            (two thirds) or a sixth (a third and a
 *                            fourth, in either order); all three consonant
 *   CHANGE_OF_REGISTER       a leap of a fifth, a sixth, or an octave from
 *                            d1 to w, then a step in the opposite
 *                            direction; all three consonant
 *   DELAY_OF_MELODIC_PROGRESSION
 *                            a leap of a third then a step in the opposite
 *                            direction outline a step; all three
 *                            consonant
 *   CONSONANT_NEIGHBOR_TONE  a step away from d1 and a step back to the
 *                            same pitch; all three consonant
 *   DISSONANT_PASSING_TONE   as the consonant passing tone, but w is a
 *                            dissonance against the cantus firmus; d1 and
 *                            d2 are consonant
 *
 * The patterns are exclusive. Only a bar with a downbeat that sounds (not
 * a half rest), a weak beat that is a new note (not a tie), and a next bar
 * has one. The last bar and a bar of a whole note do not.
 */

typedef enum {
    STANDARD_COUNTERPOINT_WEAK_BEAT_CONSONANT_PASSING_TONE = 0,
    STANDARD_COUNTERPOINT_WEAK_BEAT_SUBSTITUTION,
    STANDARD_COUNTERPOINT_WEAK_BEAT_SKIPPED_PASSING_TONE,
    STANDARD_COUNTERPOINT_WEAK_BEAT_INTERVAL_SUBDIVISION,
    STANDARD_COUNTERPOINT_WEAK_BEAT_CHANGE_OF_REGISTER,
    STANDARD_COUNTERPOINT_WEAK_BEAT_DELAY_OF_MELODIC_PROGRESSION,
    STANDARD_COUNTERPOINT_WEAK_BEAT_CONSONANT_NEIGHBOR_TONE,
    STANDARD_COUNTERPOINT_WEAK_BEAT_DISSONANT_PASSING_TONE,
    STANDARD_COUNTERPOINT_WEAK_BEAT_PATTERN_COUNT
} StandardCounterpointWeakBeatPattern;

/* "consonant-passing-tone" "substitution" "skipped-passing-tone"
   "interval-subdivision" "change-of-register"
   "delay-of-melodic-progression" "consonant-neighbor-tone"
   "dissonant-passing-tone"; NULL for an invalid pattern. */
const char *standard_counterpoint_weak_beat_pattern_name(StandardCounterpointWeakBeatPattern pattern);

/* True for the one pattern with a dissonant weak beat. */
bool standard_counterpoint_weak_beat_pattern_is_dissonant(StandardCounterpointWeakBeatPattern pattern);

/* The pattern of the bar. True and *pattern set when the bar is one of the
   patterns; false (*pattern untouched) when it matches none, has no pattern
   (see above), or the exercise or bar is invalid. */
bool standard_counterpoint_second_species_weak_beat_pattern(const StandardCounterpointSecondSpeciesExercise *exercise,
                                                            size_t bar, StandardCounterpointWeakBeatPattern *pattern);

#endif
