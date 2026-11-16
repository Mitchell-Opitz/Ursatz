#ifndef URSATZ_FRAMEWORKS_STANDARD_FORM_HYBRID_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_FORM_HYBRID_RULES_H

#include <stdbool.h>
#include <stddef.h>
#include "phrase_beginning.h"
#include "phrase_ending.h"
#include "phrase_level_form.h"
#include "semantics/subphrase_role.h"

/*
 * Hybrid rules for standard-form-v1. A hybrid form pairs the beginning of one archetype with
 * the ending of another. Any of the three beginnings (beginning_rules.h) can theoretically be
 * paired with any of the three endings (ending_rules.h), nine pairings in all:
 *
 *   PAIRING_FORM   presentation + continuation is a sentence and antecedent + consequent is a
 *                  period; the other seven pairings are hybrids
 *   COMMONALITY    how common each pairing is:
 *                    very common          presentation + continuation (sentence),
 *                                         antecedent + consequent (period)
 *                    slightly less common compound basic idea + consequent,
 *                                         compound basic idea + continuation,
 *                                         antecedent + continuation
 *                    occasional           compound basic idea + cadential,
 *                                         antecedent + cadential
 *                    rare                 presentation + cadential, presentation +
 *                                         consequent (rare or non-existent, not invalid)
 *   SUBPHRASE_ROLE a SubphraseRole is a presentation beginning or a continuation ending
 *
 * The forms are not limited to eight measures: the chapter's examples are eight and the rules
 * here carry no length.
 */

typedef enum {
    STANDARD_FORM_COMMONALITY_VERY_COMMON = 0,
    STANDARD_FORM_COMMONALITY_SLIGHTLY_LESS_COMMON,
    STANDARD_FORM_COMMONALITY_OCCASIONAL,
    STANDARD_FORM_COMMONALITY_RARE,
    STANDARD_FORM_COMMONALITY_COUNT
} StandardFormCommonality;

/* True for the four values above; false for any other value. */
bool standard_form_commonality_valid(StandardFormCommonality commonality);

/* "very-common" "slightly-less-common" "occasional" "rare"; NULL for an invalid value. */
const char *standard_form_commonality_name(StandardFormCommonality commonality);

/* PAIRING_FORM: the form of a pairing: PHRASE_LEVEL_FORM_SENTENCE, PHRASE_LEVEL_FORM_PERIOD, or
   PHRASE_LEVEL_FORM_HYBRID. False, leaving *out untouched, for an invalid beginning or ending. */
bool standard_form_pairing_form(PhraseBeginning beginning, PhraseEnding ending, PhraseLevelForm *out);

/* PAIRING_FORM: true for the seven pairings that are neither sentence nor period. False for
   those two and for an invalid beginning or ending. */
bool standard_form_pairing_is_hybrid(PhraseBeginning beginning, PhraseEnding ending);

/* COMMONALITY: how common a pairing is. False, leaving *out untouched, for an invalid beginning
   or ending. */
bool standard_form_pairing_commonality(PhraseBeginning beginning, PhraseEnding ending,
                                       StandardFormCommonality *out);

#endif
