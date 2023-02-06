#ifndef URSATZ_FRAMEWORKS_STANDARD_FORM_PHRASE_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_FORM_PHRASE_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "rules/constraint.h"
#include "rules/rule.h"
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

typedef enum {
    STANDARD_FORM_PHRASE_RULE_COMPLETE = 0,
    STANDARD_FORM_PHRASE_RULE_CLOSURE,
    STANDARD_FORM_PHRASE_RULE_LENGTH,
    STANDARD_FORM_PHRASE_RULE_GOAL,
    STANDARD_FORM_PHRASE_RULE_DIAGRAM,
    STANDARD_FORM_PHRASE_RULE_COUNT
} StandardFormPhraseRule;

const Rule *standard_form_phrase_rule(StandardFormPhraseRule which);
const Constraint *standard_form_phrase_constraint(StandardFormPhraseRule which);

/* PHRASE_COMPLETE: true only when all three parts are present. */
bool standard_form_phrase_complete(bool has_beginning, bool has_middle, bool has_end);

/* PHRASE_CLOSURE: true when a cadence signals closure. False means this rule cannot say the
   phrase is closed, not that it is open. */
bool standard_form_cadence_signals_closure(bool has_cadence);

/* PHRASE_LENGTH: true for any length of at least one measure. */
bool standard_form_phrase_length_valid(size_t measures);

/* PHRASE_LENGTH: true for the particularly common lengths 4, 8, and 16. Any other valid
   length is unusual, not wrong. */
bool standard_form_phrase_length_common(size_t measures);

/* PHRASE_GOAL: true for the perfect authentic, imperfect authentic, and half cadence. False
   for a subverted cadence and for an invalid type. */
bool standard_form_cadence_is_phrase_goal(CadenceType type);

typedef enum {
    STANDARD_FORM_DIAGRAM_MARK_BRACKET = 0,
    STANDARD_FORM_DIAGRAM_MARK_ARC,
    STANDARD_FORM_DIAGRAM_MARK_COUNT
} StandardFormDiagramMark;

/*
 * PHRASE_DIAGRAM: the mark a level takes in a diagram: a bracket for idea and subphrase, an
 * arc for phrase, theme, section, movement, and piece. False, leaving *out untouched, for
 * the motive level (a motive is circled, not bracketed) and for an invalid level.
 */
bool standard_form_level_diagram_mark(FormLevel level, StandardFormDiagramMark *out);

#endif
