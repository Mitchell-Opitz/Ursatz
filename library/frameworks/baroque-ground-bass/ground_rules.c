#include "ground_rules.h"

#include <stddef.h>

#include "ground_bass_rules.h"
#include "ground_imitation_rules.h"
#include "ground_variety_rules.h"

static const char *const NAMES[GROUND_GROUP_COUNT] = {"ground", "variety", "imitation"};

const char *ground_group_name(GroundGroup group) {
    return group >= 0 && group < GROUND_GROUP_COUNT ? NAMES[group] : NULL;
}

static void record(GroundResult *result, unsigned group, unsigned hard, unsigned soft) {
    if (hard != 0) {
        result->hard_violations |= 1u << group;
    }
    if (soft != 0) {
        result->soft_violations |= 1u << group;
    }
}

bool ground_check(const GroundBassPiece *piece, GroundResult *out) {
    if (out == NULL || !ground_piece_valid(piece)) {
        return false;
    }
    GroundResult result = {0, 0};
    unsigned hard, soft;

    if (!ground_bass_check(piece, &hard, &soft)) {
        return false;
    }
    record(&result, GROUND_GROUP_GROUND, hard, soft);

    if (!ground_variety_check(piece, &hard, &soft)) {
        return false;
    }
    record(&result, GROUND_GROUP_VARIETY, hard, soft);

    if (!ground_imitation_check(piece, &hard, &soft)) {
        return false;
    }
    record(&result, GROUND_GROUP_IMITATION, hard, soft);

    *out = result;
    return true;
}

bool ground_valid(const GroundBassPiece *piece) {
    GroundResult result;
    return ground_check(piece, &result) && result.hard_violations == 0;
}
