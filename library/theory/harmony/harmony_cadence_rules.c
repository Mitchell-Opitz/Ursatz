#include "theory/harmony/harmony_cadence_rules.h"

#include "theory/harmony/harmony_cadence_kinds.h"
#include "theory/rules/rule_table.h"

static const char *const NAMES[HARMONY_CADENCE_RULE_COUNT] = {"cadential-progression", "half-without-seventh"};

static bool table_is_hard(int which) {
    return which == HARMONY_CADENCE_CADENTIAL_PROGRESSION;
}

static const RuleTable TABLE = {
    .name = "cadence", .names = NAMES, .count = HARMONY_CADENCE_RULE_COUNT, .is_hard = table_is_hard};

bool harmony_cadence_rule_soft(HarmonyCadenceRule which) {
    return rule_table_valid(&TABLE, (int)which) && !rule_table_is_hard(&TABLE, (int)which);
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
    if (!rule_table_valid(&TABLE, (int)which) || !harmony_cadence_valid(cadence)) {
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
    rule_table_check(&TABLE, holds_adapter, cadence, hard_violations, soft_violations);
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
    const HarmonyCadenceKind *kind = harmony_cadence_kind_of(type);
    return kind != NULL ? kind->strength : -1;
}

int harmony_cadence_compare(CadenceType a, CadenceType b) {
    int strength_a = harmony_cadence_strength(a);
    int strength_b = harmony_cadence_strength(b);
    return (strength_a > strength_b) - (strength_a < strength_b);
}
