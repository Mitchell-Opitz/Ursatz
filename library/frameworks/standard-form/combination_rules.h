#ifndef URSATZ_FRAMEWORKS_STANDARD_FORM_COMBINATION_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_FORM_COMBINATION_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "rules/constraint.h"
#include "rules/rule.h"
#include "period_rules.h"
#include "semantics/cadence_type.h"
#include "phrase_level_form.h"

/*
 * Combination rules for standard-form-v1: the ways two phrases combine, beyond the period
 * (period_rules.h):
 *
 *   REPEATED_PHRASE   a phrase followed by a written-out repeat, which may be varied; the
 *                     first phrase can end with any kind of cadence and the second must end
 *                     with the same one, unlike a period
 *   WRITTEN_OUT       a phrase between repeat signs is not a repeated phrase; the repeat has
 *                     to be written out and add measures to the piece
 *   COMPOUND_PERIOD   a period whose antecedent and consequent are each a sentence; one form
 *                     containing another this way is a compound form
 *   PAIR_FORM         the form of two consecutive phrases, from their cadences: the same
 *                     cadence on a written-out repeat is a repeated phrase, a stronger
 *                     second cadence is a period (a compound period when both phrases are
 *                     sentences), and a weaker second cadence is neither
 */

typedef enum {
    STANDARD_FORM_COMBINATION_RULE_REPEATED_PHRASE = 0,
    STANDARD_FORM_COMBINATION_RULE_WRITTEN_OUT,
    STANDARD_FORM_COMBINATION_RULE_COMPOUND_PERIOD,
    STANDARD_FORM_COMBINATION_RULE_PAIR_FORM,
    STANDARD_FORM_COMBINATION_RULE_COUNT
} StandardFormCombinationRule;

const Rule *standard_form_combination_rule(StandardFormCombinationRule which);
const Constraint *standard_form_combination_constraint(StandardFormCombinationRule which);

/* REPEATED_PHRASE: true when the second phrase is a written-out repeat that ends with the same
   cadence as the first. Any valid cadence type counts, a subverted one included. */
bool standard_form_is_repeated_phrase(bool written_out_repeat, CadenceType first, CadenceType second);

/* WRITTEN_OUT: true when a repeat adds measures to the piece. A repeat shown with repeat signs
   adds none, so it is not a repeated phrase. */
bool standard_form_repeat_is_written_out(bool adds_measures);

/* COMPOUND_PERIOD: true when the cadences make a period and both phrases are sentences. */
bool standard_form_is_compound_period(CadenceType antecedent, CadenceType consequent, PhraseLevelForm first_form,
                                      PhraseLevelForm second_form);

/*
 * PAIR_FORM: classifies two consecutive phrases. second_repeats_first says the second is a
 * written-out repeat of the first; first_form and second_form give each phrase's own form (pass
 * PHRASE_LEVEL_FORM_UNIQUE when unknown). Writes PHRASE_LEVEL_FORM_REPEATED_PHRASE,
 * PHRASE_LEVEL_FORM_COMPOUND_PERIOD, or PHRASE_LEVEL_FORM_PERIOD and returns true. Returns
 * false, leaving *out untouched, when the pair is neither (a weaker second cadence, or a
 * cadence that is not a phrase goal).
 */
bool standard_form_classify_phrase_pair(CadenceType first, CadenceType second, bool second_repeats_first,
                                        PhraseLevelForm first_form, PhraseLevelForm second_form,
                                        PhraseLevelForm *out);

#endif
