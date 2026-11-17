#ifndef URSATZ_FRAMEWORKS_STANDARD_FORM_INTERNAL_EXPANSION_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_FORM_INTERNAL_EXPANSION_RULES_H

#include <stdbool.h>
#include <stddef.h>
#include "theory/form/internal_expansion_technique.h"

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

/* COMBINATION: the bit for a technique in a technique mask. Zero for an invalid technique. */
unsigned standard_form_internal_technique_bit(InternalExpansionTechnique technique);

/* REPETITION: true when a repetition expands the phrase: it is unexpected and is not the
   repeated basic idea of a presentation. */
bool standard_form_repetition_creates_expansion(bool unexpected, bool is_presentation_basic_idea_repeat);

/* ONE_MORE_TIME: true only when all three steps happened: a cadence tried, evaded, and retried. */
bool standard_form_is_one_more_time(bool tried, bool evaded, bool retried);

#endif
