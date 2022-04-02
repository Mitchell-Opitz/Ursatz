#ifndef URSATZ_FRAMEWORKS_BAROQUE_GROUND_BASS_GROUND_RULES_H
#define URSATZ_FRAMEWORKS_BAROQUE_GROUND_BASS_GROUND_RULES_H

#include <stdbool.h>

#include "ground_piece.h"

/*
 * Checking a whole piece against the rule groups of baroque-ground-bass-v1:
 *
 *   GROUND      ground_bass_rules.h
 *   VARIETY     ground_variety_rules.h (soft only)
 *   IMITATION   ground_imitation_rules.h
 *
 * The result sets a bit (1u << group) in hard_violations for each group with a
 * hard rule broken, and in soft_violations for each group with a soft rule
 * broken. A piece is valid when no hard rule is broken; soft violations are
 * advice. To learn exactly which rule failed, ask the group's own check.
 */

typedef enum {
    GROUND_GROUP_GROUND = 0,
    GROUND_GROUP_VARIETY,
    GROUND_GROUP_IMITATION,
    GROUND_GROUP_COUNT
} GroundGroup;

/* "ground" "variety" "imitation"; NULL for an invalid group. */
const char *ground_group_name(GroundGroup group);

typedef struct {
    unsigned hard_violations;
    unsigned soft_violations;
} GroundResult;

/* Runs the groups over the piece. False (*out untouched) for an invalid piece
   or NULL. */
bool ground_check(const GroundBassPiece *piece, GroundResult *out);

/* True when the piece is valid and breaks no hard rule. */
bool ground_valid(const GroundBassPiece *piece);

#endif
