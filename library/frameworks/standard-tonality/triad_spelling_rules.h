#ifndef URSATZ_FRAMEWORKS_STANDARD_TONALITY_TRIAD_SPELLING_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_TONALITY_TRIAD_SPELLING_RULES_H

#include <stdbool.h>

#include "pitch/pitch_spelling.h"
#include "rules/constraint.h"
#include "rules/rule.h"
#include "triad_rules.h"

/*
 * Triad spelling rules for standard-tonality-v1: building a triad from
 * its root and quality (what a chord symbol gives). The steps:
 *
 *   KEY_SIGNATURE  draw the root, draw the notes a generic third and
 *                  fifth above it, and take their accidentals from the
 *                  major key signature of the root: a major triad's third
 *                  and fifth are the notes of the root's major scale
 *                  (D major: F-sharp and A; the C-sharp of the key is not
 *                  used)
 *   ALTERATION     a minor, diminished, or augmented triad is the major
 *                  triad with its third and/or fifth altered: minor
 *                  lowers the third a half step, diminished lowers both
 *                  the third and the fifth, augmented raises the fifth
 *
 * A-flat minor, for example: A-flat C E-flat is the major triad (A-flat's
 * key has E-flat); lowering the third gives A-flat C-flat E-flat.
 *
 * A triad is three PitchSpellings, root first. The result agrees with the
 * interval rules: its third and fifth are the qualities
 * standard_tonality_triad_intervals gives.
 */

typedef enum {
    STANDARD_TONALITY_TRIAD_SPELLING_RULE_KEY_SIGNATURE = 0,
    STANDARD_TONALITY_TRIAD_SPELLING_RULE_ALTERATION,
    STANDARD_TONALITY_TRIAD_SPELLING_RULE_COUNT
} StandardTonalityTriadSpellingRule;

const Rule *standard_tonality_triad_spelling_rule(StandardTonalityTriadSpellingRule which);
const Constraint *standard_tonality_triad_spelling_constraint(StandardTonalityTriadSpellingRule which);

/* ALTERATION: half steps the third and the fifth move from the major
   triad's: major 0 and 0, minor -1 and 0, diminished -1 and -1, augmented
   0 and +1. False (outputs untouched) for an invalid quality or NULL
   outputs. */
bool standard_tonality_triad_alteration(StandardTonalityTriadQuality quality, int *third_shift, int *fifth_shift);

/*
 * Spells the triad of the quality on `root` into out[0..2] (root, third,
 * fifth) by the interval rules; the caller provides 3 pointers and
 * destroys each with pitch_spelling_destroy. out[0] is a copy of root.
 * False, leaving out untouched, for NULL, an invalid quality, allocation
 * failure, or a note needing more than a double accidental.
 */
bool standard_tonality_spell_triad(const PitchSpelling *root, StandardTonalityTriadQuality quality,
                                   PitchSpelling *out[3]);

/*
 * KEY_SIGNATURE and ALTERATION as the steps say: the major triad's notes
 * from the root's major scale, then altered. Same result as
 * standard_tonality_spell_triad wherever both exist; false (also) when
 * the root has no major scale (its key signature is imaginary).
 */
bool standard_tonality_spell_triad_by_key_signature(const PitchSpelling *root, StandardTonalityTriadQuality quality,
                                                    PitchSpelling *out[3]);

#endif
