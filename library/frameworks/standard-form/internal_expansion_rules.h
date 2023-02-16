#ifndef URSATZ_FRAMEWORKS_STANDARD_FORM_INTERNAL_EXPANSION_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_FORM_INTERNAL_EXPANSION_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "rules/constraint.h"
#include "rules/rule.h"
#include "semantics/alternative_path_kind.h"
#include "semantics/internal_expansion_technique.h"

/*
 * Internal expansion rules for standard-form-v1. An internal expansion adds length inside a
 * phrase, after its beginning and before its cadence, with one of four techniques:
 *
 *   LOCATION          an internal expansion falls after the beginning and before the cadence
 *   COMBINATION       techniques are often combined; a phrase with only one is relatively
 *                     rare
 *   REPETITION        a repetition expands a phrase only when it is unexpected; a predictable
 *                     one, such as the repeated basic idea of a presentation, does not
 *   ONE_MORE_TIME     the one-more-time technique has three steps: the music tries to
 *                     cadence, the attempt is evaded, and the cadence is retried. The retry
 *                     often repeats the same material, as if backing up, but may use
 *                     different cadential material
 *   ALTERNATIVE_PATH  unexpected new material takes the phrase off course. A detour is
 *                     temporary: it returns to previous material from within the phrase
 *                     before the cadence. A reroute is permanent: it reaches the cadence
 *                     without returning to previous material from within the phrase
 *   TECHNIQUE_CHOICE  one-more-time and alternative path are told apart by whether the
 *                     passage has already tried and failed to cadence: one-more-time has,
 *                     and feels like backing up; alternative path has not, and feels like
 *                     going off course
 *
 * Stretching has no separate rule: it is a harmony or melody made to last longer than
 * expected, which length_rules.h can classify. Positions are in a single consistent unit chosen
 * by the caller (measures in the examples).
 */

typedef enum {
    STANDARD_FORM_INTERNAL_EXPANSION_RULE_LOCATION = 0,
    STANDARD_FORM_INTERNAL_EXPANSION_RULE_COMBINATION,
    STANDARD_FORM_INTERNAL_EXPANSION_RULE_REPETITION,
    STANDARD_FORM_INTERNAL_EXPANSION_RULE_ONE_MORE_TIME,
    STANDARD_FORM_INTERNAL_EXPANSION_RULE_ALTERNATIVE_PATH,
    STANDARD_FORM_INTERNAL_EXPANSION_RULE_TECHNIQUE_CHOICE,
    STANDARD_FORM_INTERNAL_EXPANSION_RULE_COUNT
} StandardFormInternalExpansionRule;

const Rule *standard_form_internal_expansion_rule(StandardFormInternalExpansionRule which);
const Constraint *standard_form_internal_expansion_constraint(StandardFormInternalExpansionRule which);

/* LOCATION: true when position is at or after beginning_end and before cadence_start. */
bool standard_form_internal_expansion_inside(size_t beginning_end, size_t cadence_start, size_t position);

/* COMBINATION: the bit for a technique in a technique mask. Zero for an invalid technique. */
unsigned standard_form_internal_technique_bit(InternalExpansionTechnique technique);

/* COMBINATION: how many distinct techniques a mask holds. */
size_t standard_form_internal_technique_count(unsigned techniques);

/* COMBINATION: true when a mask holds more than one technique. */
bool standard_form_internal_expansion_is_combined(unsigned techniques);

/* REPETITION: true when a repetition expands the phrase: it is unexpected and is not the
   repeated basic idea of a presentation. */
bool standard_form_repetition_creates_expansion(bool unexpected, bool is_presentation_basic_idea_repeat);

typedef enum {
    STANDARD_FORM_OMT_STEP_TRY_CADENCE = 0,
    STANDARD_FORM_OMT_STEP_EVADE_CADENCE,
    STANDARD_FORM_OMT_STEP_RETRY_CADENCE,
    STANDARD_FORM_OMT_STEP_COUNT
} StandardFormOneMoreTimeStep;

/* True for the three steps above; false for any other value. */
bool standard_form_omt_step_valid(StandardFormOneMoreTimeStep step);

/* "try-cadence" "evade-cadence" "retry-cadence"; NULL for an invalid step. */
const char *standard_form_omt_step_name(StandardFormOneMoreTimeStep step);

/* ONE_MORE_TIME: the step after step. False, leaving *out untouched, for an invalid step or the
   last step. */
bool standard_form_omt_step_next(StandardFormOneMoreTimeStep step, StandardFormOneMoreTimeStep *out);

/* ONE_MORE_TIME: true only when all three steps happened: a cadence tried, evaded, and retried. */
bool standard_form_is_one_more_time(bool tried, bool evaded, bool retried);

/* ALTERNATIVE_PATH: a detour when the passage returns to previous material from within the
   phrase before the cadence, otherwise a reroute. Always succeeds. */
AlternativePathKind standard_form_classify_alternative_path(bool returns_to_previous_material);

/* TECHNIQUE_CHOICE: one-more-time when the passage has already tried and failed to cadence,
   alternative path when it has not. */
InternalExpansionTechnique standard_form_one_more_time_or_alternative_path(bool cadence_tried_and_failed);

#endif
