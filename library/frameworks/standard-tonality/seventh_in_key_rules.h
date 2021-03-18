#ifndef URSATZ_FRAMEWORKS_STANDARD_TONALITY_SEVENTH_IN_KEY_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_TONALITY_SEVENTH_IN_KEY_RULES_H

#include <stdbool.h>

#include "pitch/pitch_spelling.h"
#include "rules/constraint.h"
#include "rules/rule.h"
#include "seventh_rules.h"

/*
 * Seventh chord qualities within a key for standard-tonality-v1. A
 * seventh chord can be built on any note of a scale, and the quality
 * follows the scale degree, the same in every key of the kind:
 *
 *   MAJOR_KEY       major-major on do and fa (1 4); major-minor (a
 *                   dominant seventh) on sol (5); minor-minor on re, mi,
 *                   and la (2 3 6); half-diminished on ti (7)
 *   MINOR_KEY       natural minor: minor-minor on do, fa, and sol
 *                   (1 4 5); major-major on me and le (3 6);
 *                   major-minor on te (7); half-diminished on re (2)
 *   RAISED_LEADING_TONE
 *                   with the leading tone raised, the seventh chord on
 *                   sol is major-minor (a dominant seventh) and the one
 *                   on ti is fully diminished; the others are as in the
 *                   natural minor. (Raising the leading tone in the
 *                   chords on do and me would give a minor triad with a
 *                   major seventh and an augmented triad with a major
 *                   seventh, which are not among the five qualities; the
 *                   source's table leaves them as in the natural minor,
 *                   and so does this one.)
 *   KEY_INDEPENDENT the qualities do not change with the key: the
 *                   seventh chord on do is major-major in every major
 *                   key
 *
 * Degrees are 1..7. The spelled chords are checked against the quality
 * table.
 */

typedef enum {
    STANDARD_TONALITY_SEVENTH_KEY_MAJOR = 0,
    STANDARD_TONALITY_SEVENTH_KEY_MINOR_NATURAL,
    STANDARD_TONALITY_SEVENTH_KEY_MINOR_RAISED_LEADING_TONE,
    STANDARD_TONALITY_SEVENTH_KEY_COUNT
} StandardTonalitySeventhKey;

typedef enum {
    STANDARD_TONALITY_SEVENTH_IN_KEY_RULE_MAJOR_KEY = 0,
    STANDARD_TONALITY_SEVENTH_IN_KEY_RULE_MINOR_KEY,
    STANDARD_TONALITY_SEVENTH_IN_KEY_RULE_RAISED_LEADING_TONE,
    STANDARD_TONALITY_SEVENTH_IN_KEY_RULE_KEY_INDEPENDENT,
    STANDARD_TONALITY_SEVENTH_IN_KEY_RULE_COUNT
} StandardTonalitySeventhInKeyRule;

const Rule *standard_tonality_seventh_in_key_rule(StandardTonalitySeventhInKeyRule which);
const Constraint *standard_tonality_seventh_in_key_constraint(StandardTonalitySeventhInKeyRule which);

/* The quality of the seventh chord on scale degree 1..7 of the kind of
   key. False (output untouched) for an invalid key kind or degree, or
   NULL output. */
bool standard_tonality_seventh_quality_in_key(StandardTonalitySeventhKey key, int degree,
                                              StandardTonalitySeventhQuality *quality);

/*
 * KEY_INDEPENDENT: spells the seventh chord on the given degree of the
 * key of `tonic`: the root is the scale note at the degree (the major
 * scale, the natural minor scale, or for the raised leading tone the
 * harmonic minor scale) and the chord is that note's seventh chord of
 * the table's quality. Fills out[0..3] (the caller provides 4 pointers
 * and destroys each with pitch_spelling_destroy). False, leaving out
 * untouched, for NULL, an invalid key kind or degree, allocation
 * failure, or a scale or chord needing more than a double accidental.
 */
bool standard_tonality_spell_diatonic_seventh(StandardTonalitySeventhKey key, const PitchSpelling *tonic, int degree,
                                              PitchSpelling *out[4]);

#endif
