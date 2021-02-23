#ifndef URSATZ_FRAMEWORKS_STANDARD_TONALITY_TRIAD_IN_KEY_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_TONALITY_TRIAD_IN_KEY_RULES_H

#include <stdbool.h>

#include "pitch/pitch_spelling.h"
#include "rules/constraint.h"
#include "rules/rule.h"
#include "triad_rules.h"

/*
 * Triad qualities within a key for standard-tonality-v1. Any note of a
 * scale can be the root of a triad, and the quality follows the scale
 * degree, the same in every key of the kind:
 *
 *   MAJOR_KEY       major on do, fa, sol (1 4 5); minor on re, mi, la
 *                   (2 3 6); diminished on ti (7)
 *   MINOR_KEY       natural minor: minor on do, fa, sol (1 4 5); major on
 *                   me, le, te (3 6 7); diminished on re (2)
 *   RAISED_LEADING_TONE
 *                   with the leading tone raised (harmonic minor) the
 *                   triad on sol is major (a major dominant) and the
 *                   triad on ti is diminished; the triad on re stays
 *                   diminished
 *   KEY_INDEPENDENT the qualities do not change with the key: the
 *                   triad on do is major in every major key
 *
 * Harmonic minor's triad on me, which also holds the raised leading tone,
 * is augmented. The source's example names the triads above and leaves
 * that one out; it follows from the spelled scale and is included here
 * so the table is complete.
 *
 * Degrees are 1..7. The table is checked against triads stacked in
 * thirds from the spelled scales.
 */

typedef enum {
    STANDARD_TONALITY_TRIAD_KEY_MAJOR = 0,
    STANDARD_TONALITY_TRIAD_KEY_MINOR_NATURAL,
    STANDARD_TONALITY_TRIAD_KEY_MINOR_HARMONIC,
    STANDARD_TONALITY_TRIAD_KEY_COUNT
} StandardTonalityTriadKey;

typedef enum {
    STANDARD_TONALITY_TRIAD_IN_KEY_RULE_MAJOR_KEY = 0,
    STANDARD_TONALITY_TRIAD_IN_KEY_RULE_MINOR_KEY,
    STANDARD_TONALITY_TRIAD_IN_KEY_RULE_RAISED_LEADING_TONE,
    STANDARD_TONALITY_TRIAD_IN_KEY_RULE_KEY_INDEPENDENT,
    STANDARD_TONALITY_TRIAD_IN_KEY_RULE_COUNT
} StandardTonalityTriadInKeyRule;

const Rule *standard_tonality_triad_in_key_rule(StandardTonalityTriadInKeyRule which);
const Constraint *standard_tonality_triad_in_key_constraint(StandardTonalityTriadInKeyRule which);

/* The quality of the triad on scale degree 1..7 of the kind of key. False
   (output untouched) for an invalid key kind or degree, or NULL output. */
bool standard_tonality_triad_quality_in_key(StandardTonalityTriadKey key, int degree,
                                            StandardTonalityTriadQuality *quality);

/*
 * KEY_INDEPENDENT: spells the triad on the given degree of the key of
 * `tonic`, stacking thirds from the notes of the spelled scale (major,
 * natural minor, or harmonic minor), into out[0..2] (the caller provides
 * 3 pointers and destroys each with pitch_spelling_destroy). False,
 * leaving out untouched, for NULL, an invalid key kind or degree,
 * allocation failure, or a scale needing more than a double accidental.
 */
bool standard_tonality_spell_diatonic_triad(StandardTonalityTriadKey key, const PitchSpelling *tonic, int degree,
                                            PitchSpelling *out[3]);

#endif
