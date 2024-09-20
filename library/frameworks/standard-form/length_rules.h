#ifndef URSATZ_FRAMEWORKS_STANDARD_FORM_LENGTH_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_FORM_LENGTH_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "rules/constraint.h"
#include "rules/rule.h"
#include "expansion_placement.h"
#include "phrase_length_change.h"

/*
 * Length rules for standard-form-v1: expansion and contraction, the ways a composer plays with
 * the expected length of a phrase.
 *
 *   LENGTH_CHANGE     expansion makes a phrase longer than expected and contraction makes it
 *                     shorter; a phrase of the expected length has neither
 *   PLACEMENT         contraction always happens inside a phrase; expansion happens inside
 *                     it (internal) or outside it (external)
 *   EXPECTED_LENGTH   a phrase has an expected length when its cadential motion lets a
 *                     cadence be anticipated where the phrase evades it, or when the piece
 *                     states an unexpanded version of the phrase to compare against
 *   BALANCE           the proportional balance of a sentence or hybrid: an ending is as long as
 *                     its beginning, so a longer ending is expanded and a shorter one is
 *                     contracted. A contraction usually brings an abrupt-feeling cadence
 *
 * The expected length is supplied by the caller: finding it is a judgment of an analysis, with
 * its own uncertainty. This is a judgment against an expected length, not the event-level
 * reference edit of transform/expansion.h and transform/contraction.h. Lengths are in a single
 * consistent unit chosen by the caller (measures in the examples).
 */

typedef enum {
    STANDARD_FORM_LENGTH_RULE_LENGTH_CHANGE = 0,
    STANDARD_FORM_LENGTH_RULE_PLACEMENT,
    STANDARD_FORM_LENGTH_RULE_EXPECTED_LENGTH,
    STANDARD_FORM_LENGTH_RULE_BALANCE,
    STANDARD_FORM_LENGTH_RULE_COUNT
} StandardFormLengthRule;

const Rule *standard_form_length_rule(StandardFormLengthRule which);
const Constraint *standard_form_length_constraint(StandardFormLengthRule which);

/*
 * LENGTH_CHANGE: expansion when actual is longer than expected, contraction when it is shorter.
 * False, leaving *out untouched, when the lengths are equal (no change) or either is zero.
 */
bool standard_form_classify_length_change(size_t expected, size_t actual, PhraseLengthChange *out);

/* PLACEMENT: true when the change may take the placement. Contraction is internal only;
   expansion is internal or external. False for any invalid value. */
bool standard_form_placement_allowed(PhraseLengthChange change, ExpansionPlacement placement);

/* EXPECTED_LENGTH: true when at least one basis gives the phrase an expected length. */
bool standard_form_expected_length_available(bool cadence_can_be_anticipated, bool unexpanded_model_stated);

/* BALANCE: the expected length of an ending, which is the length of its beginning. 0 for a
   zero-length beginning. */
size_t standard_form_expected_ending_length(size_t beginning_length);

/* BALANCE: classifies an ending against its beginning. False, leaving *out untouched, when the
   ending is as long as the beginning or either is zero. */
bool standard_form_ending_length_change(size_t beginning_length, size_t ending_length, PhraseLengthChange *out);

#endif
