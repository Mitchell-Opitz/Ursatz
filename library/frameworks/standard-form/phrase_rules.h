#ifndef URSATZ_FRAMEWORKS_STANDARD_FORM_PHRASE_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_FORM_PHRASE_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Phrase rules for standard-form-v1. A phrase is a relatively complete thought that shows
 * trajectory toward a goal and arrives at a sense of closure:
 *
 *   PHRASE_COMPLETE   relatively complete means a sense of beginning, middle, and end
 *   PHRASE_CLOSURE    in much tonal music closure is signaled by a cadence; a phrase with no
 *                     cadence may still close by other means (a later chapter), so the lack
 *                     of a cadence does not say the phrase lacks closure
 */

typedef enum {
    STANDARD_FORM_PHRASE_RULE_COMPLETE = 0,
    STANDARD_FORM_PHRASE_RULE_CLOSURE,
    STANDARD_FORM_PHRASE_RULE_COUNT
} StandardFormPhraseRule;

const Rule *standard_form_phrase_rule(StandardFormPhraseRule which);
const Constraint *standard_form_phrase_constraint(StandardFormPhraseRule which);

/* PHRASE_COMPLETE: true only when all three parts are present. */
bool standard_form_phrase_complete(bool has_beginning, bool has_middle, bool has_end);

/* PHRASE_CLOSURE: true when a cadence signals closure. False means this rule cannot say the
   phrase is closed, not that it is open. */
bool standard_form_cadence_signals_closure(bool has_cadence);

#endif
