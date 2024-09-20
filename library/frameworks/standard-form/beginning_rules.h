#ifndef URSATZ_FRAMEWORKS_STANDARD_FORM_BEGINNING_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_FORM_BEGINNING_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "rules/constraint.h"
#include "rules/rule.h"
#include "idea_label.h"
#include "phrase_beginning.h"

/*
 * Beginning rules for standard-form-v1. A hybrid phrase-level form pairs the beginning of one
 * archetype with the ending of another. There are three beginnings:
 *
 *   antecedent            ends with a cadence (a weaker one, see period_rules.h);
 *                         a basic idea then a contrasting idea
 *   compound basic idea   ends with no cadence; a basic idea then a contrasting idea. It is
 *                         an antecedent without a cadence: the melody may come to a point of
 *                         rest, but the harmony evades the half cadence that could appear
 *   presentation          ends with no cadence; a basic idea then a repetition of it
 *
 * Rules:
 *
 *   BEGINNING_CADENCE    only an antecedent ends with a cadence
 *   BEGINNING_IDEAS      the first and second idea of each beginning
 *   COMPOUND_BASIC_IDEA  the compound basic idea differs from an antecedent only in its
 *                        missing cadence, and from a presentation in its contrasting idea
 *   BEGINNING_CLASSIFY   a beginning is told apart by its cadence and its two ideas
 *
 * A repetition of a basic idea counts as a basic idea again: it is the same material, often
 * varied, and a different contour makes it contrast instead (see sentence_rules.h).
 */

typedef enum {
    STANDARD_FORM_BEGINNING_RULE_CADENCE = 0,
    STANDARD_FORM_BEGINNING_RULE_IDEAS,
    STANDARD_FORM_BEGINNING_RULE_COMPOUND_BASIC_IDEA,
    STANDARD_FORM_BEGINNING_RULE_CLASSIFY,
    STANDARD_FORM_BEGINNING_RULE_COUNT
} StandardFormBeginningRule;

const Rule *standard_form_beginning_rule(StandardFormBeginningRule which);
const Constraint *standard_form_beginning_constraint(StandardFormBeginningRule which);

/* BEGINNING_CADENCE: true only for an antecedent. False for the others and for an invalid
   beginning. */
bool standard_form_beginning_ends_with_cadence(PhraseBeginning beginning);

/* BEGINNING_IDEAS: the labels of the first and second idea. False, leaving *first and
   *second untouched, for an invalid beginning. */
bool standard_form_beginning_ideas(PhraseBeginning beginning, IdeaLabel *first, IdeaLabel *second);

/* COMPOUND_BASIC_IDEA: true for an antecedent that lacks its cadence, which is how a
   compound basic idea is built. Only valid beginnings count. */
bool standard_form_beginning_is_cadence_free_antecedent(PhraseBeginning beginning);

/*
 * BEGINNING_CLASSIFY: the beginning with this cadence and these two ideas. False, leaving
 * *out untouched, for a combination no beginning has (a cadence after a presentation's two
 * basic ideas, or a contrasting idea first).
 */
bool standard_form_classify_beginning(bool ends_with_cadence, IdeaLabel first, IdeaLabel second,
                                      PhraseBeginning *out);

#endif
