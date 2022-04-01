#ifndef URSATZ_FRAMEWORKS_BAROQUE_GROUND_BASS_GROUND_VARIETY_RULES_H
#define URSATZ_FRAMEWORKS_BAROQUE_GROUND_BASS_GROUND_VARIETY_RULES_H

#include <stdbool.h>

#include "ground_piece.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Variety rules for baroque-ground-bass-v1. A bass that repeats throughout
 * could be tedious, so the variety is made above it: the upper voices (every
 * voice but the bass) change from one iteration to the next, in their notes,
 * in the metrical levels they use, and in the imitation between them. Only the
 * whole iterations are looked at; a note belongs to the iteration it begins
 * in. All are soft.
 *
 *   UPPER_PARTS_VARY       the upper voices of each iteration are not the
 *                          same, slot for slot, as in the iteration before
 *   UPPER_RHYTHM_VARIES    in a piece of three or more iterations with upper
 *                          notes, the shortest upper note is not the same
 *                          length in every iteration: the metrical level
 *                          changes (eighth notes, sixteenth notes, and so on)
 *   EVERY_ITERATION_IMITATES  every iteration contains the beginning of a
 *                          dux entry: almost every iteration is accompanied
 *                          by a new imitative relationship in the upper parts
 *
 * Every check returns false for an invalid piece.
 */

typedef enum {
    GROUND_VARIETY_UPPER_PARTS_VARY = 0,
    GROUND_VARIETY_UPPER_RHYTHM_VARIES,
    GROUND_VARIETY_EVERY_ITERATION_IMITATES,
    GROUND_VARIETY_RULE_COUNT
} GroundVarietyRule;

const Rule *ground_variety_rule(GroundVarietyRule which);
const Constraint *ground_variety_constraint(GroundVarietyRule which);

/* "upper-parts-vary" "upper-rhythm-varies" "every-iteration-imitates"; NULL for
   an invalid rule. */
const char *ground_variety_rule_name(GroundVarietyRule which);

/* True when the piece satisfies the rule. False when it does not, and for an
   invalid piece or an invalid rule. */
bool ground_variety_rule_holds(GroundVarietyRule which, const GroundBassPiece *piece);

/* The rules the piece breaks: hard_violations has bit (1u << rule) set for each
   hard rule that fails (none: every rule is soft), soft_violations for each
   soft rule. False (both untouched) for an invalid piece. */
bool ground_variety_check(const GroundBassPiece *piece, unsigned *hard_violations, unsigned *soft_violations);

#endif
