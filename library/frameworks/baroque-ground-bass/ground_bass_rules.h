#ifndef URSATZ_FRAMEWORKS_BAROQUE_GROUND_BASS_GROUND_BASS_RULES_H
#define URSATZ_FRAMEWORKS_BAROQUE_GROUND_BASS_GROUND_BASS_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "ground_piece.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Ground rules for baroque-ground-bass-v1: the bass line that repeats
 * throughout, usually exactly or nearly so. The ground is the first iteration
 * of the bass voice, `slots_per_iteration` slots long. A note belongs to the
 * iteration it begins in; an iteration repeats the ground when it has the same
 * number of notes, each beginning at the same offset and lasting as long, and
 * the number of those whose pitch differs (the altered notes) is within the
 * limit (GroundLimits).
 *
 * Hard:
 *
 *   ITERATIONS_WHOLE   the piece is a whole number of iterations long
 *   GROUND_REPEATS     every whole iteration repeats the ground, with at most
 *                      `max_altered_notes` altered notes
 *   BASS_LOWEST        the bass is never above another sounding voice
 *   TRIPLE_METER       a chaconne or a passacaglia has three beats to the
 *                      bar (holds for the other kinds)
 *
 * Soft:
 *
 *   EXACT_REPEAT       every whole iteration repeats the ground exactly: no
 *                      altered notes
 *
 * Every check returns false for an invalid piece.
 */

typedef enum {
    GROUND_BASS_ITERATIONS_WHOLE = 0,
    GROUND_BASS_GROUND_REPEATS,
    GROUND_BASS_BASS_LOWEST,
    GROUND_BASS_TRIPLE_METER,
    GROUND_BASS_EXACT_REPEAT,
    GROUND_BASS_RULE_COUNT
} GroundBassRule;

const Rule *ground_bass_rule(GroundBassRule which);
const Constraint *ground_bass_constraint(GroundBassRule which);

/* "iterations-whole" "ground-repeats" "bass-lowest" "triple-meter" "exact-repeat"; NULL
   for an invalid rule. */
const char *ground_bass_rule_name(GroundBassRule which);

/* The number of notes of whole iteration `iteration` (0 is the ground itself)
   that differ from the ground: a note with another pitch, another place in the
   iteration, or another length counts as one, and an iteration with another
   number of notes counts as every note of the longer one. False (*out
   untouched) for an invalid piece or an iteration out of range. */
bool ground_bass_altered_notes(const GroundBassPiece *piece, size_t iteration, size_t *out);

/* True when the piece satisfies the rule. False when it does not, and for an
   invalid piece or an invalid rule. */
bool ground_bass_rule_holds(GroundBassRule which, const GroundBassPiece *piece);

/* The rules the piece breaks: hard_violations has bit (1u << rule) set for each
   hard rule that fails, soft_violations for each soft rule. False (both
   untouched) for an invalid piece. */
bool ground_bass_check(const GroundBassPiece *piece, unsigned *hard_violations, unsigned *soft_violations);

#endif
