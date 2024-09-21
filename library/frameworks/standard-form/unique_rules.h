#ifndef URSATZ_FRAMEWORKS_STANDARD_FORM_UNIQUE_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_FORM_UNIQUE_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "rules/constraint.h"
#include "rules/rule.h"
#include "phrase_level_form.h"

/*
 * Unique form rules for standard-form-v1. Sentences and periods are the archetypes of
 * phrase-level form, but just as common are forms that are not in dialogue with them:
 *
 *   UNIQUE_FORM   a phrase-level form organized as neither a sentence nor a period is unique
 *   ARCHETYPAL    sentence, period, and compound period are archetypal forms; unique is
 *                 not. A repeated phrase has no category of its own, it takes the category of
 *                 the phrase repeated
 *   SPECTRUM      archetype and unique are two ends of a spectrum, not exclusive categories:
 *                 a form showing traits of both is closer to one without clearly belonging to
 *                 either
 *   LABELS        labels are applied flexibly on a unique form, and dropped where they do not
 *                 support the interpretation; a segmentation analysis still applies
 *
 * Neither a category nor a closeness score is stored: how close a form is to an archetype is
 * a judgment of an analysis, with its own Uncertainty.
 */

typedef enum {
    STANDARD_FORM_UNIQUE_RULE_UNIQUE_FORM = 0,
    STANDARD_FORM_UNIQUE_RULE_ARCHETYPAL,
    STANDARD_FORM_UNIQUE_RULE_SPECTRUM,
    STANDARD_FORM_UNIQUE_RULE_LABELS,
    STANDARD_FORM_UNIQUE_RULE_COUNT
} StandardFormUniqueRule;

const Rule *standard_form_unique_rule(StandardFormUniqueRule which);
const Constraint *standard_form_unique_constraint(StandardFormUniqueRule which);

/* UNIQUE_FORM: true when the form is organized as neither a sentence nor a period. */
bool standard_form_organized_as_unique(bool organized_as_sentence, bool organized_as_period);

/* ARCHETYPAL: true for sentence, period, and compound period; false for repeated phrase, unique,
   and an invalid form. */
bool standard_form_form_is_archetypal(PhraseLevelForm form);

/* ARCHETYPAL: true only for the unique form. */
bool standard_form_form_is_unique(PhraseLevelForm form);

/* SPECTRUM: true when a form shows both archetypal and non-archetypal traits, so it sits
   between the two ends. */
bool standard_form_form_is_mixed(bool shows_archetype_traits, bool shows_non_archetype_traits);

/* LABELS: true when a label should be kept, which is when it supports the interpretation. */
bool standard_form_label_should_apply(bool supports_interpretation);

#endif
