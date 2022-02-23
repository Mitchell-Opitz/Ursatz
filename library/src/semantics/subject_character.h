#ifndef MUSIC_SEMANTICS_SUBJECT_CHARACTER_H
#define MUSIC_SEMANTICS_SUBJECT_CHARACTER_H

#include <stdbool.h>

/*
 * SubjectCharacter (Value): the character of a fugue subject. A plain enum:
 * it names a style and carries no EntityID, no pitches, and no framework
 * claim.
 *
 *   TOCCATA   a motor-rhythm feel, often chromatic, and may have irregular
 *             rhythmic placement
 *   RICERCAR  derived from the plainsong and motet tradition: slow moving,
 *             an antiquated feel, and extensive use of suspensions
 *
 * Assigning a character is an analytic judgment; no type here makes it.
 */

typedef enum {
    SUBJECT_CHARACTER_TOCCATA = 0,
    SUBJECT_CHARACTER_RICERCAR,
    SUBJECT_CHARACTER_COUNT
} SubjectCharacter;

/* True for the two characters above; false for any other value. */
bool subject_character_valid(SubjectCharacter character);

/* "toccata" "ricercar"; NULL for an invalid character. */
const char *subject_character_name(SubjectCharacter character);

#endif
