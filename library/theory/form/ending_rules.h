#ifndef URSATZ_FRAMEWORKS_STANDARD_FORM_ENDING_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_FORM_ENDING_RULES_H

#include <stdbool.h>
#include <stddef.h>
#include "theory/form/phrase_rules.h"
#include "theory/vocabulary/cadence_type.h"
#include "theory/form/phrase_ending.h"

/*
 * Ending rules for standard-form-v1. A hybrid phrase-level form pairs the beginning of one
 * archetype with the ending of another. There are three endings:
 *
 *   consequent     PAC or IAC, never HC; a basic idea then a contrasting idea; any bass;
 *                  begins stable
 *   continuation   HC, IAC or PAC; ideas vary but are often divided 1+1+2 or 2+2; begins
 *                  unstable
 *   cadential      PAC or IAC, never HC; harmonizes the core bass line;
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
