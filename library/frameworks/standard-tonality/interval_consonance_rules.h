#ifndef URSATZ_FRAMEWORKS_STANDARD_TONALITY_INTERVAL_CONSONANCE_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_TONALITY_INTERVAL_CONSONANCE_RULES_H

#include <stdbool.h>

#include "interval_quality_rules.h"
#include "interval_size_rules.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Consonance and dissonance rules for standard-tonality-v1. A consonant
 * interval is considered stable, as if it need not resolve; a dissonant
 * one is considered unstable, as if it must. The categorization has
 * varied with milieu: these tables are this framework's, and another
 * framework may draw the line elsewhere. They differ for melodic and
 * harmonic intervals (StandardTonalityIntervalType).
 *
 *   MELODIC   consonant: perfect intervals, major and minor seconds,
 *             thirds, and sixths. Dissonant: augmented and diminished
 *             intervals, major and minor sevenths
 *   HARMONIC  consonant: major and minor thirds and sixths, perfect
 *             unison, fifth, and octave. Dissonant: major and minor
 *             seconds and sevenths, augmented and diminished
 *             intervals, and the perfect fourth
 *
 * Compound intervals are classified as their simple counterparts. Doubly
 * and triply augmented or diminished intervals are augmented or
 * diminished. An interval whose quality does not fit its size is
 * neither.
 */

typedef enum {
    STANDARD_TONALITY_INTERVAL_CONSONANCE_RULE_MELODIC = 0,
    STANDARD_TONALITY_INTERVAL_CONSONANCE_RULE_HARMONIC,
    STANDARD_TONALITY_INTERVAL_CONSONANCE_RULE_COUNT
} StandardTonalityIntervalConsonanceRule;

const Rule *standard_tonality_interval_consonance_rule(StandardTonalityIntervalConsonanceRule which);
const Constraint *standard_tonality_interval_consonance_constraint(StandardTonalityIntervalConsonanceRule which);

/* True when the interval of the size and quality is consonant as a melodic
   or harmonic interval. False if it is dissonant, the quality does not fit
   the size, or the type is invalid. */
bool standard_tonality_interval_is_consonant(StandardTonalityIntervalType type, int size,
                                             StandardTonalityIntervalQuality quality);

/* True when it is dissonant. False if it is consonant, the quality does not
   fit the size, or the type is invalid. Every valid interval is exactly
   one of consonant and dissonant. */
bool standard_tonality_interval_is_dissonant(StandardTonalityIntervalType type, int size,
                                             StandardTonalityIntervalQuality quality);

#endif
