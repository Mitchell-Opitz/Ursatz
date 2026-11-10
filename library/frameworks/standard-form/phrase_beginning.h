#ifndef MUSIC_SEMANTICS_PHRASE_BEGINNING_H
#define MUSIC_SEMANTICS_PHRASE_BEGINNING_H

#include <stdbool.h>

/*
 * PhraseBeginning (Value): how the first part of a phrase-level form begins. A plain enum: it names a value and carries no EntityID, no
 * container, and no framework claim.
 *
 *   ANTECEDENT            ends with a cadence; a basic idea then a contrasting idea
 *   COMPOUND_BASIC_IDEA   an antecedent without a cadence; a basic idea then a contrasting idea
 *   PRESENTATION          ends with no cadence; a basic idea then a repetition of it
 *
 * Which characteristics tell the beginnings apart is a convention of a Framework
 * (frameworks/standard-form), not of this type.
 */

typedef enum {
    PHRASE_BEGINNING_ANTECEDENT = 0,
    PHRASE_BEGINNING_COMPOUND_BASIC_IDEA,
    PHRASE_BEGINNING_PRESENTATION,
    PHRASE_BEGINNING_COUNT
} PhraseBeginning;

/* True for the 3 values above; false for any other value. */
bool phrase_beginning_valid(PhraseBeginning value);

/* "antecedent" "compound-basic-idea" "presentation"; NULL for an invalid
   value. */
const char *phrase_beginning_name(PhraseBeginning value);

/* Looks a value up by its name (the strings phrase_..._name gives). False, with *out untouched,
   for an unknown name or NULL. */
bool phrase_beginning_from_name(const char *name, PhraseBeginning *out);

#endif
