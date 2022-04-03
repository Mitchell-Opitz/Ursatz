#include "ground_imitation_rules.h"

#include "ground_rule_table.h"
#include "standard-counterpoint/counterpoint_pitch.h"

static const char *const NAMES[GROUND_IMITATION_RULE_COUNT] = {"comes-follows-dux", "comes-imitates",
                                                               "entries-aligned", "voices-differ"};

static bool table_is_hard(int which) {
    return which <= GROUND_IMITATION_COMES_IMITATES;
}

static GroundRulePair g_pairs[GROUND_IMITATION_RULE_COUNT];
static bool g_built[GROUND_IMITATION_RULE_COUNT];
static const GroundRuleTable TABLE = {"imitation", NAMES, GROUND_IMITATION_RULE_COUNT, table_is_hard, g_pairs,
                                      g_built};

const Rule *ground_imitation_rule(GroundImitationRule which) {
    return ground_rule_table_rule(&TABLE, (int)which);
}

const Constraint *ground_imitation_constraint(GroundImitationRule which) {
    return ground_rule_table_constraint(&TABLE, (int)which);
}

const char *ground_imitation_rule_name(GroundImitationRule which) {
    return ground_rule_table_name(&TABLE, (int)which);
}

/* The nearest dux listed before statement `comes`; false when there is none. */
static bool dux_of(const GroundBassPiece *piece, size_t comes, size_t *dux) {
    for (size_t j = comes; j > 0; j--) {
        if (piece->statements[j - 1].role == IMITATIVE_ROLE_DUX) {
            *dux = j - 1;
            return true;
        }
    }
    return false;
}

static bool check_one(GroundImitationRule which, const GroundBassPiece *piece, size_t comes) {
    size_t dux;
    if (!dux_of(piece, comes, &dux)) {
        return false;
    }
    const GroundStatement *d = &piece->statements[dux];
    const GroundStatement *c = &piece->statements[comes];
    switch (which) {
        case GROUND_IMITATION_COMES_FOLLOWS_DUX:
            return c->start > d->start;
        case GROUND_IMITATION_COMES_IMITATES: {
            GroundNote lead[GROUND_MAX_NOTES], follow[GROUND_MAX_NOTES];
            size_t nl, nf;
            if (!ground_statement_notes(piece, dux, lead, GROUND_MAX_NOTES, &nl) ||
                !ground_statement_notes(piece, comes, follow, GROUND_MAX_NOTES, &nf)) {
                return false;
            }
            size_t core = nl < nf ? nl : nf;
            core = core < GROUND_IMITATION_CORE_NOTES ? core : GROUND_IMITATION_CORE_NOTES;
            for (size_t i = 0; i < core; i++) {
                if (lead[i].length != follow[i].length) {
                    return false;
                }
                if (i + 1 < core && standard_counterpoint_direction(lead[i].pitch, lead[i + 1].pitch) !=
                                        standard_counterpoint_direction(follow[i].pitch, follow[i + 1].pitch)) {
                    return false;
                }
            }
            return true;
        }
        case GROUND_IMITATION_ENTRIES_ALIGNED:
            return c->start % piece->slots_per_bar == d->start % piece->slots_per_bar;
        case GROUND_IMITATION_VOICES_DIFFER:
            return c->voice != d->voice;
        default:
            return false;
    }
}

bool ground_imitation_rule_holds(GroundImitationRule which, const GroundBassPiece *piece) {
    if (!ground_rule_table_valid(&TABLE, (int)which) || !ground_piece_valid(piece)) {
        return false;
    }
    for (size_t i = 0; i < piece->statement_count; i++) {
        if (piece->statements[i].role == IMITATIVE_ROLE_COMES && !check_one(which, piece, i)) {
            return false;
        }
    }
    return true;
}

static bool holds_adapter(int which, const void *subject) {
    return ground_imitation_rule_holds((GroundImitationRule)which, subject);
}

bool ground_imitation_check(const GroundBassPiece *piece, unsigned *hard_violations, unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL || !ground_piece_valid(piece)) {
        return false;
    }
    ground_rule_table_check(&TABLE, holds_adapter, piece, hard_violations, soft_violations);
    return true;
}
