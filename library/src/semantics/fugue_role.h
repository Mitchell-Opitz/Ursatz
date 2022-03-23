#ifndef MUSIC_SEMANTICS_FUGUE_ROLE_H
#define MUSIC_SEMANTICS_FUGUE_ROLE_H

#include <stdbool.h>

/*
 * FugueRole (Value): the part a passage of one voice plays in a fugue. A
 * plain enum: it names a role and carries no EntityID, no pitches, and no
 * framework claim.
 *
 *   SUBJECT            the main theme of the fugue, a short tune that recurs
 *                      throughout
 *   ANSWER             a transposition of the subject, sometimes slightly
 *                      altered
 *   COUNTERSUBJECT     a melodic line sounded with, and complementing, the
 *                      subject or answer
 *   FREE_COUNTERPOINT  a part that accompanies the entries and does not
 *                      necessarily recur
 *   LINK               a short interpolation between entries, often built on
 *                      a motive of the subject
 *   EPISODE            a passage between statements of the subject, longer
 *                      than a link
 *
 * Which roles appear where, and in what order, is a convention of a
 * Framework (frameworks/high-baroque-fugue), not of this type.
 */

typedef enum {
    FUGUE_ROLE_SUBJECT = 0,
    FUGUE_ROLE_ANSWER,
    FUGUE_ROLE_COUNTERSUBJECT,
    FUGUE_ROLE_FREE_COUNTERPOINT,
    FUGUE_ROLE_LINK,
    FUGUE_ROLE_EPISODE,
    FUGUE_ROLE_COUNT
} FugueRole;

/* True for the six roles above; false for any other value. */
bool fugue_role_valid(FugueRole role);

/* "subject" "answer" "countersubject" "free-counterpoint" "link" "episode";
   NULL for an invalid role. */
const char *fugue_role_name(FugueRole role);

/* True for the roles that state the main theme: the subject and the answer.
   False for the others and for an invalid role. */
bool fugue_role_is_entry(FugueRole role);

#endif
