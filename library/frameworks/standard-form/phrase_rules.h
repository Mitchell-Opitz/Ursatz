#ifndef URSATZ_FRAMEWORKS_STANDARD_FORM_PHRASE_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_FORM_PHRASE_RULES_H

#include <stdbool.h>
#include <stddef.h>
#include "semantics/cadence_type.h"
#include "semantics/form_level.h"

/*
 * Phrase rules for standard-form-v1. A phrase is a relatively complete thought that shows
 * trajectory toward a goal and arrives at a sense of closure:
 *
 *   PHRASE_COMPLETE   relatively complete means a sense of beginning, middle, and end
 *   PHRASE_CLOSURE    in much tonal music closure is signaled by a cadence; a phrase with no
 *                     cadence may still close by other means, so the lack of a cadence does
 *                     not say the phrase lacks closure
 *   PHRASE_LENGTH     a phrase can be any length, but 4, 8, or 16 measures are particularly
 *                     common
 *   PHRASE_GOAL       in tonal classical music the goal of a phrase is almost always a
 *                     perfect authentic, imperfect authentic, or half cadence; a subverted
 *                     cadence is a cadence that is avoided, so it is not a goal
 *   PHRASE_DIAGRAM    square brackets mark ideas and subphrases (anything that need not end
 *                     with a cadence); arcs mark the phrase level and above (anything that
 *                     must end with a cadence)
 *
 * Lengths are whole measures.
 */

/* PHRASE_GOAL: true for the perfect authentic, imperfect authentic, and half cadence. False
   for a subverted cadence and for an invalid type. */
bool standard_form_cadence_is_phrase_goal(CadenceType type);

#endif
