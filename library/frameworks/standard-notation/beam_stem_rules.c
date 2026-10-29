#include "beam_stem_rules.h"

#include "rule_pair.h"

static const char *const NAMES[STANDARD_NOTATION_BEAM_STEM_RULE_COUNT] = {"beam-stem-farthest-note"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_notation_framework, "standard-notation",
    NAMES, STANDARD_NOTATION_BEAM_STEM_RULE_COUNT);

static bool valid_rule(StandardNotationBeamStemRule which) {
    return which >= 0 && which < STANDARD_NOTATION_BEAM_STEM_RULE_COUNT;
}

const Rule *standard_notation_beam_stem_rule(StandardNotationBeamStemRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->rule : NULL;
}

const Constraint *standard_notation_beam_stem_constraint(StandardNotationBeamStemRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->constraint : NULL;
}

/*
 * Greatest distance above and below the middle line among the notes.
 * False for an empty group or a NULL entry.
 */
static bool extremes(const StaffPosition *const *positions, size_t count, int *out_above, int *out_below) {
    if (positions == NULL || count == 0) {
        return false;
    }
    int above = 0;
    int below = 0;
    for (size_t i = 0; i < count; i++) {
        if (positions[i] == NULL) {
            return false;
        }
        int offset = staff_position_steps(positions[i]) - STAFF_POSITION_MIDDLE_LINE;
        if (offset > above) {
            above = offset;
        }
        if (-offset > below) {
            below = -offset;
        }
    }
    *out_above = above;
    *out_below = below;
    return true;
}

int standard_notation_beamed_stem_direction_option_count(const StaffPosition *const *positions, size_t count) {
    int above;
    int below;
    if (!extremes(positions, count, &above, &below)) {
        return 0;
    }
    return above == below ? 2 : 1;
}

StemDirection standard_notation_beamed_stem_direction_default(const StaffPosition *const *positions, size_t count) {
    int above;
    int below;
    if (extremes(positions, count, &above, &below) && below > above) {
        return STEM_DIRECTION_UP;
    }
    return STEM_DIRECTION_DOWN;
}

bool standard_notation_beamed_stem_direction_valid(const StaffPosition *const *positions, size_t count,
                                                   StemDirection direction) {
    int above;
    int below;
    if (!extremes(positions, count, &above, &below)) {
        return false;
    }
    if (above == below) {
        return true;
    }
    return direction == (above > below ? STEM_DIRECTION_DOWN : STEM_DIRECTION_UP);
}
