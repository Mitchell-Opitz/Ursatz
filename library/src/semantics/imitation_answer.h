#ifndef MUSIC_SEMANTICS_IMITATION_ANSWER_H
#define MUSIC_SEMANTICS_IMITATION_ANSWER_H

#include <stdbool.h>

/*
 * ImitationAnswer (Value): how a later entry of a melody answers the one
 * that came before it in imitative writing. A plain enum: it names a
 * relationship between two entries and carries no EntityID, no pitches, and
 * no framework claim.
 *
 *   REAL    the answer repeats the melody at a fixed interval, every
 *           interval of it kept
 *   TONAL   the answer is adjusted so that it stays within the key, so some
 *           of its intervals differ from the melody by a step (a fifth
 *           answered by a fourth, say)
 *   FREE    the answer keeps the rhythm and the opening shape of the
 *           melody and then goes its own way
 *
 * Which answer a style expects, and how closely it is kept, is a convention
 * of a Framework (frameworks/sixteenth-century-style), not of this type.
 */

typedef enum {
    IMITATION_ANSWER_REAL = 0,
    IMITATION_ANSWER_TONAL,
    IMITATION_ANSWER_FREE,
    IMITATION_ANSWER_COUNT
} ImitationAnswer;

/* True for the three answers above; false for any other value. */
bool imitation_answer_valid(ImitationAnswer answer);

/* "real" "tonal" "free"; NULL for an invalid answer. */
const char *imitation_answer_name(ImitationAnswer answer);

/* True for the answers that keep every interval of the melody (the real
   answer). False for the others and for an invalid answer. */
bool imitation_answer_keeps_intervals(ImitationAnswer answer);

#endif
