#ifndef URSATZ_FRAMEWORKS_HIGH_BAROQUE_FUGUE_FUGUE_COUNTERSUBJECT_RULES_H
#define URSATZ_FRAMEWORKS_HIGH_BAROQUE_FUGUE_FUGUE_COUNTERSUBJECT_RULES_H

#include <stdbool.h>

#include "fugue_exposition.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Countersubject rules for high-baroque-fugue-v1. A countersubject is a
 * melodic line sounded with, and complementing, the subject or answer. It
 * first appears in a two-voice passage with the answer, then recurs against
 * the subject. The statements of one countersubject share a number
 * (fugue_exposition.h). A countersubject statement accompanies the first
 * entry in another voice that begins at or after its own start; the rules
 * that compare the two look at the slots where both sound.
 *
 * Hard:
 *
 *   RHYTHM_CONSTANT        every statement of a countersubject keeps the
 *                          number of notes and the rhythm of its first
 *
 * Soft:
 *
 *   INVERTIBLE_INTERVALS   when the countersubject is declared invertible at
 *                          the octave, its voice and the entry it
 *                          accompanies make only unisons or octaves, thirds or
 *                          sixths, and seconds or sevenths at every slot where
 *                          either begins a note: no fifths, which invert to
 *                          fourths, and no fourths. That also rules out the
 *                          4-3 suspension, whose fourth inverts to a fifth;
 *                          the 7-6 and the 2-3 (which invert to each other)
 *                          remain. Holds when it is not declared invertible
 *   COMPLEMENTARY_RHYTHM   one voice moves while the other is stationary: no
 *                          more than half of the slots where either voice
 *                          begins a note are slots where both do
 *   CHANGES_ONLY_WITH_ANSWER  a later statement of a countersubject differs
 *                          from its first in no more intervals (in generic
 *                          size) than the answer differs from the subject.
 *                          This counts the changes; it does not check that
 *                          they fall where the answer's do. A countersubject
 *                          with no answer to explain a change must not
 *                          change
 *
 * Every check returns false for an invalid exposition.
 */

typedef enum {
    FUGUE_COUNTERSUBJECT_RHYTHM_CONSTANT = 0,
    FUGUE_COUNTERSUBJECT_INVERTIBLE_INTERVALS,
    FUGUE_COUNTERSUBJECT_COMPLEMENTARY_RHYTHM,
    FUGUE_COUNTERSUBJECT_CHANGES_ONLY_WITH_ANSWER,
    FUGUE_COUNTERSUBJECT_RULE_COUNT
} FugueCountersubjectRule;

const Rule *fugue_countersubject_rule(FugueCountersubjectRule which);
const Constraint *fugue_countersubject_constraint(FugueCountersubjectRule which);

/* "rhythm-constant" "invertible-intervals" "complementary-rhythm"
   "changes-only-with-answer"; NULL for an invalid rule. */
const char *fugue_countersubject_rule_name(FugueCountersubjectRule which);

/* True when the exposition satisfies the rule. False when it does not, and
   for an invalid exposition or an invalid rule. */
bool fugue_countersubject_rule_holds(FugueCountersubjectRule which, const FugueExposition *exposition);

/* The rules the exposition breaks: hard_violations has bit (1u << rule) set for
   each hard rule that fails, soft_violations for each soft rule. False (both
   untouched) for an invalid exposition. */
bool fugue_countersubject_check(const FugueExposition *exposition, unsigned *hard_violations,
                                unsigned *soft_violations);

#endif
