#ifndef MUSIC_SEMANTICS_PHRASE_LEVEL_FORM_H
#define MUSIC_SEMANTICS_PHRASE_LEVEL_FORM_H

#include <stdbool.h>

/*
 * PhraseLevelForm (Value): how phrases are organized into a phrase-level form. A plain enum: it names a value and carries no EntityID, no
 * container, and no framework claim.
 *
 *   SENTENCE              a single phrase made of a presentation and a continuation
 *   PERIOD                two phrases, an antecedent then a consequent
 *   REPEATED_PHRASE       a phrase followed by a written-out repeat of itself
 *   COMPOUND_PERIOD       a period whose two phrases are sentences
 *   UNIQUE                organized as neither a sentence nor a period
 *
 * Sentence and period are the archetypes; archetype and unique are two ends of a spectrum, not
 * exclusive categories. What makes each form is a convention of a Framework
 * (frameworks/standard-form), not of this type.
 */

typedef enum {
    PHRASE_LEVEL_FORM_SENTENCE = 0,
    PHRASE_LEVEL_FORM_PERIOD,
    PHRASE_LEVEL_FORM_REPEATED_PHRASE,
    PHRASE_LEVEL_FORM_COMPOUND_PERIOD,
    PHRASE_LEVEL_FORM_UNIQUE,
    PHRASE_LEVEL_FORM_COUNT
} PhraseLevelForm;

/* True for the 5 values above; false for any other value. */
bool phrase_level_form_valid(PhraseLevelForm value);

/* "sentence" "period" "repeated-phrase" "compound-period" "unique"; NULL for an invalid
   value. */
const char *phrase_level_form_name(PhraseLevelForm value);

#endif
