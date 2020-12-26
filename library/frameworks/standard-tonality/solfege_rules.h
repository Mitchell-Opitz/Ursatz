#ifndef URSATZ_FRAMEWORKS_STANDARD_TONALITY_SOLFEGE_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_TONALITY_SOLFEGE_RULES_H

#include <stdbool.h>

#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Chromatic movable-do solfege rules for standard-tonality-v1. Every
 * scale degree 1..7 has a syllable when it matches the major scale
 * (do re mi fa sol la ti), and some have a raised or lowered one:
 *
 *   SYLLABLE         the syllable of a degree, altered by -1 (lowered),
 *                    0 (as in major), or +1 (raised) semitone against
 *                    the major scale on the same tonic
 *   RAISED_ENDING    raised syllables end in -i: di ri fi si li
 *   LOWERED_ENDING   lowered syllables end in -e: me se le te, ra being
 *                    the exception
 *
 *   degree   lowered  major  raised
 *     1        --      do      di
 *     2        ra      re      ri
 *     3        me      mi      --
 *     4        --      fa      fi
 *     5        se      sol     si
 *     6        le      la      li
 *     7        te      ti      --
 *
 * A dash means no syllable in this vocabulary: there is no lowered do
 * or fa (that pitch is ti or mi of another octave position), and no
 * raised mi or ti (it is fa or do).
 */

typedef enum {
    STANDARD_TONALITY_SOLFEGE_RULE_SYLLABLE = 0,
    STANDARD_TONALITY_SOLFEGE_RULE_RAISED_ENDING,
    STANDARD_TONALITY_SOLFEGE_RULE_LOWERED_ENDING,
    STANDARD_TONALITY_SOLFEGE_RULE_COUNT
} StandardTonalitySolfegeRule;

const Rule *standard_tonality_solfege_rule(StandardTonalitySolfegeRule which);
const Constraint *standard_tonality_solfege_constraint(StandardTonalitySolfegeRule which);

/* SYLLABLE: the syllable of degree 1..7 with the alteration -1, 0, +1.
   NULL for any other degree or alteration, or a combination with no
   syllable (see the table). */
const char *standard_tonality_chromatic_solfege(int degree, int alteration);

/* RAISED_ENDING / LOWERED_ENDING: true when the degree and alteration
   have a syllable that follows its ending rule: a raised syllable ends
   in 'i'; a lowered one ends in 'e', or is "ra". False for alteration 0,
   an invalid pair, or a pair with no syllable. */
bool standard_tonality_solfege_follows_ending_rule(int degree, int alteration);

/* The degree and alteration a syllable names. On true *degree is 1..7
   and *alteration -1, 0, or +1. False (outputs untouched) for NULL, an
   unknown syllable, or NULL outputs. */
bool standard_tonality_solfege_parse(const char *syllable, int *degree, int *alteration);

#endif
