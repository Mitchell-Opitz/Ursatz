#ifndef URSATZ_FRAMEWORKS_STANDARD_FORM_ENDING_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_FORM_ENDING_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "rules/constraint.h"
#include "rules/rule.h"
#include "phrase_rules.h"
#include "semantics/cadence_type.h"
#include "phrase_ending.h"

/*
 * Ending rules for standard-form-v1. A hybrid phrase-level form pairs the beginning of one
 * archetype with the ending of another. There are three endings:
 *
 *   consequent     PAC or IAC, never HC; a basic idea then a contrasting idea; any bass;
 *                  begins stable
 *   continuation   HC, IAC or PAC; ideas vary but are often divided 1+1+2 or 2+2; begins
 *                  unstable
 *   cadential      PAC or IAC, never HC; harmonizes the core bass line (cadential_bass_rules.h);
 *                  divided 2+2 or not divisible at all; never fragmented
 *
 * Rules:
 *
 *   ENDING_CADENCE     the cadences each ending may end with
 *   ENDING_DIVISION    how the ending usually divides into units
 *   ENDING_FRAGMENTATION  a cadential ending never has fragmentation
 *   ENDING_STABILITY   how stable the start of an ending feels; a cadential ending is not
 *                      characterized
 *   ENDING_CLASSIFY    an ending is told apart by its cadence, bass, ideas and fragmentation
 *
 * Division lengths are in a single consistent unit chosen by the caller (measures in the
 * examples).
 */

typedef enum {
    STANDARD_FORM_ENDING_RULE_CADENCE = 0,
    STANDARD_FORM_ENDING_RULE_DIVISION,
    STANDARD_FORM_ENDING_RULE_FRAGMENTATION,
    STANDARD_FORM_ENDING_RULE_STABILITY,
    STANDARD_FORM_ENDING_RULE_CLASSIFY,
    STANDARD_FORM_ENDING_RULE_COUNT
} StandardFormEndingRule;

const Rule *standard_form_ending_rule(StandardFormEndingRule which);
const Constraint *standard_form_ending_constraint(StandardFormEndingRule which);

/* ENDING_CADENCE: true when the ending may end with this cadence type. Consequent and
   cadential take the authentic cadences only, a continuation also takes the half cadence. A
   subverted cadence is no ending's goal, and an invalid type is never allowed. */
bool standard_form_ending_cadence_allowed(PhraseEnding ending, CadenceType type);

/* ENDING_DIVISION: true when units, in order, are the ending's usual division: 1+1+2 or 2+2
   for a continuation; 2+2 or one undivided unit for a cadential ending; for a consequent, its
   two ideas (a basic idea then a contrasting idea), whatever their lengths. False for any other
   division and for an invalid ending. */
bool standard_form_ending_division_usual(PhraseEnding ending, const size_t *units, size_t unit_count);

/* ENDING_FRAGMENTATION: false for a cadential ending, which never has fragmentation, and for an
   invalid ending; true for the others. */
bool standard_form_ending_allows_fragmentation(PhraseEnding ending);

typedef enum {
    STANDARD_FORM_STABILITY_STABLE = 0,
    STANDARD_FORM_STABILITY_UNSTABLE,
    STANDARD_FORM_STABILITY_COUNT
} StandardFormStability;

/* ENDING_STABILITY: stable for a consequent (the stability of its basic idea), unstable for a
   continuation (the dominant carried on past the half cadence of an antecedent). False, leaving
   *out untouched, for a cadential ending, which the framework does not characterize, and for an
   invalid ending. */
bool standard_form_ending_start_stability(PhraseEnding ending, StandardFormStability *out);

/*
 * ENDING_CLASSIFY: the ending with this cadence, bass, ideas, and fragmentation. A bass line
 * that is the cadential pattern with an authentic cadence and no fragmentation is a cadential
 * ending; otherwise a basic idea then a contrasting idea with an authentic cadence is a
 * consequent; otherwise any phrase goal cadence is a continuation. That order settles a
 * combination that fits more than one. False, leaving *out untouched, when the cadence is no
 * phrase goal.
 */
bool standard_form_classify_ending(CadenceType cadence, bool bass_is_cadential_pattern,
                                   bool basic_then_contrasting_ideas, bool has_fragmentation,
                                   PhraseEnding *out);

#endif
