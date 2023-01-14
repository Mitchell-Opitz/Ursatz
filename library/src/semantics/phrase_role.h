#ifndef MUSIC_SEMANTICS_PHRASE_ROLE_H
#define MUSIC_SEMANTICS_PHRASE_ROLE_H

#include <stdbool.h>

/*
 * PhraseRole (Value): the part a phrase plays inside a period. A plain enum: it names a value and carries no EntityID, no
 * container, and no framework claim.
 *
 *   ANTECEDENT            the first phrase, ending with the weaker cadence
 *   CONSEQUENT            the second phrase, ending with the stronger cadence
 *
 * Which cadences each part ends with is a convention of a Framework (frameworks/standard-form),
 * not of this type.
 */

typedef enum {
    PHRASE_ROLE_ANTECEDENT = 0,
    PHRASE_ROLE_CONSEQUENT,
    PHRASE_ROLE_COUNT
} PhraseRole;

/* True for the 2 values above; false for any other value. */
bool phrase_role_valid(PhraseRole value);

/* "antecedent" "consequent"; NULL for an invalid
   value. */
const char *phrase_role_name(PhraseRole value);

#endif
