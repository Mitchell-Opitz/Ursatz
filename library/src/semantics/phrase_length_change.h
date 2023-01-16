#ifndef MUSIC_SEMANTICS_PHRASE_LENGTH_CHANGE_H
#define MUSIC_SEMANTICS_PHRASE_LENGTH_CHANGE_H

#include <stdbool.h>

/*
 * PhraseLengthChange (Value): how a phrase differs from its expected length. A plain enum: it names a value and carries no EntityID, no
 * container, and no framework claim.
 *
 *   EXPANSION             the phrase lasts longer than expected
 *   CONTRACTION           the phrase lasts shorter than expected
 *
 * Which phrases count as expanded or contracted is a judgment against an expected length, and the
 * conventions for it belong to a Framework (frameworks/standard-form), not to this type.
 */

typedef enum {
    PHRASE_LENGTH_CHANGE_EXPANSION = 0,
    PHRASE_LENGTH_CHANGE_CONTRACTION,
    PHRASE_LENGTH_CHANGE_COUNT
} PhraseLengthChange;

/* True for the 2 values above; false for any other value. */
bool phrase_length_change_valid(PhraseLengthChange value);

/* "expansion" "contraction"; NULL for an invalid
   value. */
const char *phrase_length_change_name(PhraseLengthChange value);

#endif
