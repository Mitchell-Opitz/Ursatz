#include "harmony_cadence_rules.h"

#include "harmony_rule_table.h"

static const char *const NAMES[HARMONY_CADENCE_RULE_COUNT] = {"cadential-progression", "half-without-seventh"};

static bool table_is_hard(int which) {
    return which == HARMONY_CADENCE_CADENTIAL_PROGRESSION;
}

static HarmonyRulePair g_pairs[HARMONY_CADENCE_RULE_COUNT];
static bool g_built[HARMONY_CADENCE_RULE_COUNT];
static const HarmonyRuleTable TABLE = {"cadence", NAMES, HARMONY_CADENCE_RULE_COUNT, table_is_hard, g_pairs, g_built};

const Rule *harmony_cadence_rule(HarmonyCadenceRule which) {
    return harmony_rule_table_rule(&TABLE, (int)which);
}

const Constraint *harmony_cadence_constraint(HarmonyCadenceRule which) {
    return harmony_rule_table_constraint(&TABLE, (int)which);
}

const char *harmony_cadence_rule_name(HarmonyCadenceRule which) {
    return harmony_rule_table_name(&TABLE, (int)which);
}

bool harmony_cadence_valid(const HarmonyCadence *cadence) {
    return cadence != NULL && harmony_chord_valid(&cadence->from) && harmony_chord_valid(&cadence->to) &&
           cadence->soprano_degree >= 1 && cadence->soprano_degree <= 7;
}

/* V to I, or x to V. */
static bool is_authentic(const HarmonyCadence *c) {
    return c->from.degree == 5 && c->to.degree == 1;
}

static bool is_half(const HarmonyCadence *c) {
    return c->to.degree == 5;
}

bool harmony_cadence_rule_holds(HarmonyCadenceRule which, const HarmonyCadence *cadence) {
    if (!harmony_rule_table_valid(&TABLE, (int)which) || !harmony_cadence_valid(cadence)) {
        return false;
    }
    if (which == HARMONY_CADENCE_HALF_WITHOUT_SEVENTH) {
        return !(is_half(cadence) && cadence->to.seventh);
    }
    return is_authentic(cadence) || is_half(cadence);
}

static bool holds_adapter(int which, const void *subject) {
    return harmony_cadence_rule_holds((HarmonyCadenceRule)which, subject);
}

bool harmony_cadence_check(const HarmonyCadence *cadence, unsigned *hard_violations, unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL || !harmony_cadence_valid(cadence)) {
        return false;
    }
    harmony_rule_table_check(&TABLE, holds_adapter, cadence, hard_violations, soft_violations);
    return true;
}

bool harmony_cadence_classify(const HarmonyCadence *cadence, CadenceType *out) {
    if (out == NULL || !harmony_cadence_valid(cadence)) {
        return false;
    }
    if (is_authentic(cadence)) {
        if (cadence->subverted) {
            *out = CADENCE_TYPE_SUBVERTED;
        } else if (cadence->from.inversion == 0 && cadence->to.inversion == 0 && cadence->soprano_degree == 1) {
            *out = CADENCE_TYPE_PERFECT_AUTHENTIC;
        } else {
            *out = CADENCE_TYPE_IMPERFECT_AUTHENTIC;
        }
        return true;
    }
    if (is_half(cadence)) {
        *out = cadence->subverted ? CADENCE_TYPE_SUBVERTED : CADENCE_TYPE_HALF;
        return true;
    }
    return false;
}

int harmony_cadence_strength(CadenceType type) {
    switch (type) {
        case CADENCE_TYPE_SUBVERTED:
            return 0;
        case CADENCE_TYPE_HALF:
            return 1;
        case CADENCE_TYPE_IMPERFECT_AUTHENTIC:
            return 2;
        case CADENCE_TYPE_PERFECT_AUTHENTIC:
            return 3;
        default:
            return -1;
    }
}
