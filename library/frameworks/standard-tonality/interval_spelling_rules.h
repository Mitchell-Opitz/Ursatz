#ifndef URSATZ_FRAMEWORKS_STANDARD_TONALITY_INTERVAL_SPELLING_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_TONALITY_INTERVAL_SPELLING_RULES_H

#include <stdbool.h>

#include "interval_quality_rules.h"
#include "pitch/pitch_spelling.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Interval spelling rule for standard-tonality-v1, the inverse of
 * identifying an interval (interval_quality_rules.h):
 *
 *   SPELLING_ABOVE   given a lower note, a size, and a quality, the
 *                    upper note is the letter size - 1 letters above
 *                    the lower, with whatever accidental gives the
 *                    interval its quality. A perfect fifth above F is
 *                    C; a minor sixth above E-flat is C-flat; a
 *                    diminished fifth above F is C-flat
 *
 * Works for compound sizes too (the octave number follows the letter).
 * The result is checked by identification: identifying the interval from
 * the lower note to the spelled note returns the size and quality
 * asked for.
 */

typedef enum {
    STANDARD_TONALITY_INTERVAL_SPELLING_RULE_SPELLING_ABOVE = 0,
    STANDARD_TONALITY_INTERVAL_SPELLING_RULE_COUNT
} StandardTonalityIntervalSpellingRule;

const Rule *standard_tonality_interval_spelling_rule(StandardTonalityIntervalSpellingRule which);
const Constraint *standard_tonality_interval_spelling_constraint(StandardTonalityIntervalSpellingRule which);

/*
 * SPELLING_ABOVE: builds the note a given interval above `lower` into
 * *out (the caller destroys it with pitch_spelling_destroy). False,
 * leaving *out untouched, for NULL, a size below 1, a quality that does
 * not fit the size (a perfect third, a major fifth), allocation failure,
 * or a note that would need more than a double accidental.
 */
bool standard_tonality_spell_above(const PitchSpelling *lower, int size, StandardTonalityIntervalQuality quality,
                                   PitchSpelling **out);

#endif
