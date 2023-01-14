#ifndef MUSIC_SEMANTICS_SUBPHRASE_ROLE_H
#define MUSIC_SEMANTICS_SUBPHRASE_ROLE_H

#include <stdbool.h>

/*
 * SubphraseRole (Value): the part a subphrase plays inside a sentence. A plain enum: it names a value and carries no EntityID, no
 * container, and no framework claim.
 *
 *   PRESENTATION          a basic idea and its repetition; the stable first half
 *   CONTINUATION          the less stable second half that drives toward the cadence
 *
 * Which traits mark each part is a convention of a Framework (frameworks/standard-form), not of
 * this type.
 */

typedef enum {
    SUBPHRASE_ROLE_PRESENTATION = 0,
    SUBPHRASE_ROLE_CONTINUATION,
    SUBPHRASE_ROLE_COUNT
} SubphraseRole;

/* True for the 2 values above; false for any other value. */
bool subphrase_role_valid(SubphraseRole value);

/* "presentation" "continuation"; NULL for an invalid
   value. */
const char *subphrase_role_name(SubphraseRole value);

#endif
