#ifndef URSATZ_FRAMEWORKS_STANDARD_FORM_SEGMENTATION_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_FORM_SEGMENTATION_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Segmentation analysis rules for standard-form-v1. A segmentation analysis shows the
 * grouping units of a passage, from phrase endings down to the idea level, using square
 * brackets above the staff. It is a procedure over existing types (Phrase, Idea, Cadence),
 * not a stored entity:
 *
 *   STEP_ORDER    the four steps run in order: identify points of closure, divide each
 *                 phrase into smaller units, bracket those units, then verify and label
 *                 the cadences
 *   IDEA_LENGTH   an idea is often two measures long but may be longer or shorter
 *   IDEA_DIVISION the ideas of a phrase are non-empty and together fill the phrase exactly
 *
 * Lengths are whole measures.
 */

typedef enum {
    STANDARD_FORM_SEGMENTATION_RULE_STEP_ORDER = 0,
    STANDARD_FORM_SEGMENTATION_RULE_IDEA_LENGTH,
    STANDARD_FORM_SEGMENTATION_RULE_IDEA_DIVISION,
    STANDARD_FORM_SEGMENTATION_RULE_COUNT
} StandardFormSegmentationRule;

const Rule *standard_form_segmentation_rule(StandardFormSegmentationRule which);
const Constraint *standard_form_segmentation_constraint(StandardFormSegmentationRule which);

typedef enum {
    STANDARD_FORM_SEGMENTATION_STEP_IDENTIFY_CLOSURE = 0,
    STANDARD_FORM_SEGMENTATION_STEP_DIVIDE_INTO_IDEAS,
    STANDARD_FORM_SEGMENTATION_STEP_BRACKET_IDEAS,
    STANDARD_FORM_SEGMENTATION_STEP_VERIFY_CADENCES,
    STANDARD_FORM_SEGMENTATION_STEP_COUNT
} StandardFormSegmentationStep;

/* True for the four steps above; false for any other value. */
bool standard_form_segmentation_step_valid(StandardFormSegmentationStep step);

/* "identify-closure" "divide-into-ideas" "bracket-ideas" "verify-cadences"; NULL for an
   invalid step. */
const char *standard_form_segmentation_step_name(StandardFormSegmentationStep step);

/* STEP_ORDER: the step after step. False, leaving *out untouched, for an invalid step or the
   last step. */
bool standard_form_segmentation_step_next(StandardFormSegmentationStep step, StandardFormSegmentationStep *out);

/*
 * STEP_ORDER, step 1: the indices i in 0..count-1 where cadence_after[i] is true, written to
 * out in order up to out_capacity entries. Returns how many points of closure there are, which
 * can exceed out_capacity (nothing past the capacity is written). Each is a candidate phrase
 * ending; a phrase needs a beginning, middle, and end, so a caller should not take every
 * candidate as a phrase of its own without checking.
 */
size_t standard_form_closure_points(const bool *cadence_after, size_t count, size_t *out, size_t out_capacity);

/* IDEA_LENGTH: the usual length of an idea, two measures. */
size_t standard_form_idea_typical_measures(void);

/* IDEA_LENGTH: true for any length of at least one measure; ideas may be longer or shorter
   than the usual two. */
bool standard_form_idea_length_valid(size_t measures);

/* IDEA_DIVISION: true when idea_measures has at least one entry, every entry is a valid idea
   length, and the entries add up to phrase_measures. */
bool standard_form_ideas_fill_phrase(const size_t *idea_measures, size_t idea_count, size_t phrase_measures);

#endif
