#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_CONSONANCE_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_CONSONANCE_RULES_H

#include <stdbool.h>

#include "counterpoint_interval.h"
#include "pitch/pitch_spelling.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Consonance and dissonance rules for standard-counterpoint-v1. Counterpoint
 * sorts the harmonic intervals between two parts into three categories:
 *
 *   PERFECT CONSONANCE    perfect unison, fifth, and octave
 *   IMPERFECT CONSONANCE  major and minor thirds and sixths
 *   DISSONANCE            all seconds and sevenths, all augmented and
 *                         diminished intervals, and the perfect fourth
 *
 * The perfect fourth is dissonant when it involves the lowest part of the
 * texture and consonant between two upper parts. This framework is two
 * parts only, so every fourth involves the lowest part and is a
 * dissonance.
 *
 * Compound intervals are classified as their simple counterparts (a tenth
 * is a third, a twelfth a fifth). Direction is ignored.
 *
 * Melodic intervals, the steps and leaps within one part, have their own
 * categories: consonant melodic intervals are the perfect intervals
 * (unison, fourth, fifth, octave) and the major and minor seconds,
 * thirds, and sixths; dissonant ones are every augmented and diminished
 * interval and the major and minor sevenths. Whether a consonant melodic
 * interval is also a good one to write is a separate matter, taken up by
 * the melodic writing rules.
 */

typedef enum {
    STANDARD_COUNTERPOINT_CONSONANCE_RULE_PERFECT = 0,
    STANDARD_COUNTERPOINT_CONSONANCE_RULE_IMPERFECT,
    STANDARD_COUNTERPOINT_CONSONANCE_RULE_DISSONANCE,
    STANDARD_COUNTERPOINT_CONSONANCE_RULE_PERFECT_FOURTH,
    STANDARD_COUNTERPOINT_CONSONANCE_RULE_MELODIC,
    STANDARD_COUNTERPOINT_CONSONANCE_RULE_COUNT
} StandardCounterpointConsonanceRule;

const Rule *standard_counterpoint_consonance_rule(StandardCounterpointConsonanceRule which);
const Constraint *standard_counterpoint_consonance_constraint(StandardCounterpointConsonanceRule which);

typedef enum {
    STANDARD_COUNTERPOINT_PERFECT_CONSONANCE = 0,
    STANDARD_COUNTERPOINT_IMPERFECT_CONSONANCE,
    STANDARD_COUNTERPOINT_DISSONANCE
} StandardCounterpointIntervalCategory;

/* The category of a harmonic interval. False (*out untouched) for NULL. */
bool standard_counterpoint_classify_interval(const StandardCounterpointInterval *interval,
                                             StandardCounterpointIntervalCategory *out);

/* The category of the harmonic interval between two pitches. False (*out
   untouched) for NULL or an unclassifiable pair. */
bool standard_counterpoint_classify_pitches(const PitchSpelling *a, const PitchSpelling *b,
                                            StandardCounterpointIntervalCategory *out);

/* True when the interval between the two pitches is a perfect or an
   imperfect consonance. False for a dissonance, NULL, or an unclassifiable
   pair. */
bool standard_counterpoint_pitches_consonant(const PitchSpelling *a, const PitchSpelling *b);

/* True when the interval is a consonant melodic interval (see above).
   False for a dissonant one and for NULL. */
bool standard_counterpoint_interval_melodic_consonant(const StandardCounterpointInterval *interval);

#endif
