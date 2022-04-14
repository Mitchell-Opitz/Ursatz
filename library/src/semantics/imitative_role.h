#ifndef MUSIC_SEMANTICS_IMITATIVE_ROLE_H
#define MUSIC_SEMANTICS_IMITATIVE_ROLE_H

#include <stdbool.h>

/*
 * ImitativeRole (Value): the part a voice plays in an imitation between two
 * voices. A plain enum: it names a role and carries no EntityID, no pitches,
 * and no framework claim.
 *
 *   DUX    the leader: the voice that states the melody first
 *   COMES  the follower: the voice that imitates it
 *
 * It is not ImitationAnswer (how closely an entry follows) nor FugueRole
 * (the part of a fugue a passage plays). How closely a comes follows its dux,
 * and where it enters, is a convention of a Framework.
 */

typedef enum {
    IMITATIVE_ROLE_DUX = 0,
    IMITATIVE_ROLE_COMES,
    IMITATIVE_ROLE_COUNT
} ImitativeRole;

/* True for the two roles above; false for any other value. */
bool imitative_role_valid(ImitativeRole role);

/* "dux" "comes"; NULL for an invalid role. */
const char *imitative_role_name(ImitativeRole role);

#endif
