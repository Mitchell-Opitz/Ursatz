#ifndef MUSIC_SEMANTICS_PHRASE_ENDING_H
#define MUSIC_SEMANTICS_PHRASE_ENDING_H

#include <stdbool.h>

/*
 * PhraseEnding (Value): how the second part of a phrase-level form ends. A plain enum: it names a value and carries no EntityID, no
 * container, and no framework claim.
 *
 *   CONSEQUENT            ends with a strong cadence; a basic idea then a contrasting idea
 *   CONTINUATION          ends with a cadence; unstable and often fragmented
 *   CADENTIAL             harmonizes a particular bass pattern and ends with a strong cadence
 *
 * Which characteristics tell the endings apart is a convention of a Framework
 * (frameworks/standard-form), not of this type.
 */

typedef enum {
    PHRASE_ENDING_CONSEQUENT = 0,
    PHRASE_ENDING_CONTINUATION,
    PHRASE_ENDING_CADENTIAL,
    PHRASE_ENDING_COUNT
} PhraseEnding;

/* True for the 3 values above; false for any other value. */
bool phrase_ending_valid(PhraseEnding value);

/* "consequent" "continuation" "cadential"; NULL for an invalid
   value. */
const char *phrase_ending_name(PhraseEnding value);

/* Looks a value up by its name (the strings phrase_..._name gives). False, with *out untouched,
   for an unknown name or NULL. */
bool phrase_ending_from_name(const char *name, PhraseEnding *out);

#endif
