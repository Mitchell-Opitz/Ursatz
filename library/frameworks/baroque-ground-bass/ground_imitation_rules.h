#ifndef URSATZ_FRAMEWORKS_BAROQUE_GROUND_BASS_GROUND_IMITATION_RULES_H
#define URSATZ_FRAMEWORKS_BAROQUE_GROUND_BASS_GROUND_IMITATION_RULES_H

#include <stdbool.h>

#include "ground_piece.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Imitation rules for baroque-ground-bass-v1. Imitation and interplay between
 * the upper parts is the other source of variety over the ground: the dux
 * (leader) states a melody and the comes (follower) imitates it. The caller
 * names the statements (ground_piece.h). A comes is paired with the nearest
 * dux listed before it. "The first notes" below means the first three notes of
 * the dux, or all of them if it is shorter.
 *
 * Hard:
 *
 *   COMES_FOLLOWS_DUX   every comes has a dux listed before it and begins after
 *                       it begins
 *   COMES_IMITATES      every comes keeps the rhythm of its dux and moves in the
 *                       same direction as it through the first notes
 *
 * Soft:
 *
 *   ENTRIES_ALIGNED     every comes begins at the same position in the bar as its
 *                       dux, so the gap between them is a whole number of bars
 *                       (three or six beats in 3/4); closer imitation is
 *                       occasional
 *   VOICES_DIFFER       the comes is in another voice from its dux
 *
 * The rules hold when the piece has no comes. Every check returns false for an
 * invalid piece.
 */

#define GROUND_IMITATION_CORE_NOTES 3

typedef enum {
    GROUND_IMITATION_COMES_FOLLOWS_DUX = 0,
    GROUND_IMITATION_COMES_IMITATES,
    GROUND_IMITATION_ENTRIES_ALIGNED,
    GROUND_IMITATION_VOICES_DIFFER,
    GROUND_IMITATION_RULE_COUNT
} GroundImitationRule;

const Rule *ground_imitation_rule(GroundImitationRule which);
const Constraint *ground_imitation_constraint(GroundImitationRule which);

/* "comes-follows-dux" "comes-imitates" "entries-aligned" "voices-differ"; NULL for
   an invalid rule. */
const char *ground_imitation_rule_name(GroundImitationRule which);

/* True when the piece satisfies the rule. False when it does not, and for an
   invalid piece or an invalid rule. */
bool ground_imitation_rule_holds(GroundImitationRule which, const GroundBassPiece *piece);

/* The rules the piece breaks: hard_violations has bit (1u << rule) set for each
   hard rule that fails, soft_violations for each soft rule. False (both
   untouched) for an invalid piece. */
bool ground_imitation_check(const GroundBassPiece *piece, unsigned *hard_violations, unsigned *soft_violations);

#endif
