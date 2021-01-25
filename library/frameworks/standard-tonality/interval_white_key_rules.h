#ifndef URSATZ_FRAMEWORKS_STANDARD_TONALITY_INTERVAL_WHITE_KEY_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_TONALITY_INTERVAL_WHITE_KEY_RULES_H

#include <stdbool.h>

#include "interval_quality_rules.h"
#include "pitch/pitch_spelling.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * White-key interval rules for standard-tonality-v1. The white-key method
 * learns the intervals between the white keys of the piano (the notes
 * of C major) and reads any other interval as an alteration of one. The
 * white-key intervals repeat, so memorize the most frequent type and the
 * exceptions:
 *
 *   SECONDS      all seconds are major except E-F and B-C, which are
 *                minor
 *   THIRDS       all thirds are minor except C-E, F-A, and G-B, which are
 *                major
 *   FOURTHS      all fourths are perfect except F-B, an augmented fourth
 *                (the tritone)
 *   BY_INVERSION the rest follow by inversion: sevenths (2 + 7 = 9) are
 *                minor except F-E and C-B, which are major; sixths
 *                invert the thirds and fifths the fourths (so the
 *                fifths are perfect except B-F, a diminished fifth)
 *   ALTERATION   an interval between altered notes is its white-key
 *                interval made larger or smaller by the accidentals:
 *                D to F is a minor third, so D to F-sharp, one half
 *                step larger, is a major third
 *
 * Unisons and octaves are perfect. A white-key interval is named by the
 * letter of its lower note and its size (1..8): the upper note is the
 * letter size - 1 letters above.
 */

typedef enum {
    STANDARD_TONALITY_WHITE_KEY_RULE_SECONDS = 0,
    STANDARD_TONALITY_WHITE_KEY_RULE_THIRDS,
    STANDARD_TONALITY_WHITE_KEY_RULE_FOURTHS,
    STANDARD_TONALITY_WHITE_KEY_RULE_BY_INVERSION,
    STANDARD_TONALITY_WHITE_KEY_RULE_ALTERATION,
    STANDARD_TONALITY_WHITE_KEY_RULE_COUNT
} StandardTonalityWhiteKeyRule;

const Rule *standard_tonality_white_key_rule(StandardTonalityWhiteKeyRule which);
const Constraint *standard_tonality_white_key_constraint(StandardTonalityWhiteKeyRule which);

/* The quality of the white-key interval of `size` (1..8) above the
   natural note `lower`, from the memorized rules. False (output
   untouched) for a size outside 1..8 or a NULL output. */
bool standard_tonality_white_key_quality(PitchLetter lower, int size, StandardTonalityIntervalQuality *quality);

/* The most frequent quality among the white-key intervals of a size 1..8:
   major for seconds and sixths, minor for thirds and sevenths, perfect
   for unisons, fourths, fifths, and octaves. False for another size or
   a NULL output. */
bool standard_tonality_white_key_default_quality(int size, StandardTonalityIntervalQuality *quality);

/* True when the white-key interval of `size` above `lower` is an
   exception to its size's default quality (E-F and B-C among seconds,
   C-E F-A G-B among thirds, F-B among fourths, ...). False otherwise or
   for an invalid size. */
bool standard_tonality_white_key_is_exception(PitchLetter lower, int size);

/*
 * ALTERATION: the quality of the interval of `size` (1..8) from the
 * lower letter with accidental lower_accidental up to the letter
 * size - 1 above it with upper_accidental: the white-key quality,
 * shifted by upper_accidental minus lower_accidental half steps. False
 * (output untouched) for an invalid size, a NULL output, a result
 * beyond triply augmented or diminished, or an interval that would be
 * smaller than a unison.
 */
bool standard_tonality_white_key_altered_quality(PitchLetter lower, int lower_accidental, int size,
                                                 int upper_accidental, StandardTonalityIntervalQuality *quality);

#endif
