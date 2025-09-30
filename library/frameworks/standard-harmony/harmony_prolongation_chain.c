#include "harmony_prolongation_chain.h"

#include "harmony_rule_table.h"

static const char *const NAMES[HARMONY_CHAIN_RULE_COUNT] = {"links-valid", "linked", "same-mode"};

static bool table_is_hard(int which) {
    (void)which;
    return true;
}

static HarmonyRulePair g_pairs[HARMONY_CHAIN_RULE_COUNT];
static bool g_built[HARMONY_CHAIN_RULE_COUNT];
static ConstraintEvalResult evaluate_candidate(const Constraint *constraint, const Value *candidate,
                                                    const Context *context);
static const HarmonyRuleTable TABLE = {"chain", NAMES, HARMONY_CHAIN_RULE_COUNT, table_is_hard, g_pairs, g_built,
                                       evaluate_candidate};

const Rule *harmony_chain_rule(HarmonyChainRule which) {
    return harmony_rule_table_rule(&TABLE, (int)which);
}

const Constraint *harmony_chain_constraint(HarmonyChainRule which) {
    return harmony_rule_table_constraint(&TABLE, (int)which);
}

const char *harmony_chain_rule_name(HarmonyChainRule which) {
    return harmony_rule_table_name(&TABLE, (int)which);
}

bool harmony_chain_valid(const HarmonyProlongationChain *chain) {
    if (chain == NULL || chain->link_count < 1 || chain->link_count > HARMONY_PROLONGATION_CHAIN_MAX) {
        return false;
    }
    for (size_t i = 0; i < chain->link_count; i++) {
        if (!harmony_prolongation_valid(&chain->links[i])) {
            return false;
        }
    }
    return true;
}

static bool same_chord(const HarmonyChord *a, const HarmonyChord *b) {
    return a->degree == b->degree && a->inversion == b->inversion && a->leading_tone == b->leading_tone &&
           a->seventh == b->seventh && a->cadential == b->cadential;
}

/* The last chord of `a` is the first chord of `b`: the same chord, notes, and beat. */
static bool linked(const HarmonyTonicProlongation *a, const HarmonyTonicProlongation *b) {
    if (!same_chord(&a->chords[2], &b->chords[0]) || a->beat[2] != b->beat[0]) {
        return false;
    }
    for (int v = 0; v < VOICE_PART_COUNT; v++) {
        if (a->steps[2][v] != b->steps[0][v]) {
            return false;
        }
    }
    return true;
}

bool harmony_chain_rule_holds(HarmonyChainRule which, const HarmonyProlongationChain *chain) {
    if (!harmony_rule_table_valid(&TABLE, (int)which) || !harmony_chain_valid(chain)) {
        return false;
    }
    for (size_t i = 0; i < chain->link_count; i++) {
        unsigned hard = 0, soft = 0;
        switch (which) {
            case HARMONY_CHAIN_LINKS_VALID:
                if (!harmony_prolongation_check(&chain->links[i], &hard, &soft) || hard != 0) {
                    return false;
                }
                break;
            case HARMONY_CHAIN_LINKED:
                if (i + 1 < chain->link_count && !linked(&chain->links[i], &chain->links[i + 1])) {
                    return false;
                }
                break;
            case HARMONY_CHAIN_SAME_MODE:
                if (chain->links[i].mode != chain->links[0].mode) {
                    return false;
                }
                break;
            default:
                return false;
        }
    }
    return true;
}

static bool holds_adapter(int which, const void *subject) {
    return harmony_chain_rule_holds((HarmonyChainRule)which, subject);
}

HARMONY_RULE_TABLE_EVALUATOR(evaluate_candidate, TABLE, holds_adapter, HarmonyProlongationChain)

bool harmony_chain_check(const HarmonyProlongationChain *chain, unsigned *hard_violations, unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL || !harmony_chain_valid(chain)) {
        return false;
    }
    harmony_rule_table_check(&TABLE, holds_adapter, chain, hard_violations, soft_violations);
    return true;
}
