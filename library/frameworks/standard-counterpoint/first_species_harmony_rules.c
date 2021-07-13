#include "first_species_harmony_rules.h"

#include <stddef.h>
#include <stdio.h>

#include "consonance_rules.h"
#include "counterpoint_interval.h"
#include "harmonic_rules.h"
#include "rule_pair.h"

static const char *const NAMES[STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_COUNT] = {
    "all-consonant",        "no-repeated-perfect",    "no-direct-perfects", "max-same-imperfect-run",
    "imperfect-preferred",  "perfect-then-imperfect", "interval-variety"};

static bool rule_is_soft(StandardCounterpointFirstSpeciesHarmonyRule which) {
    return which == STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_IMPERFECT_PREFERRED ||
           which == STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_PERFECT_THEN_IMPERFECT ||
           which == STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_INTERVAL_VARIETY;
}

static StandardCounterpointRulePair g_pairs[STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_COUNT];
static bool g_built[STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_COUNT];

static bool valid_rule(StandardCounterpointFirstSpeciesHarmonyRule which) {
    return which >= 0 && which < STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_COUNT;
}

static void ensure_built(StandardCounterpointFirstSpeciesHarmonyRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    char name[96];
    snprintf(name, sizeof(name), "first-species-harmony-%s", NAMES[which]);
    standard_counterpoint_build_rule_pair(name, rule_is_soft(which) ? CONSTRAINT_TYPE_SOFT : CONSTRAINT_TYPE_HARD,
                                          &g_pairs[which]);
}

const Rule *standard_counterpoint_first_species_harmony_rule(StandardCounterpointFirstSpeciesHarmonyRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *
standard_counterpoint_first_species_harmony_constraint(StandardCounterpointFirstSpeciesHarmonyRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

const char *standard_counterpoint_first_species_harmony_rule_name(StandardCounterpointFirstSpeciesHarmonyRule which) {
    return valid_rule(which) ? NAMES[which] : NULL;
}

/* The interval between the parts at position i, and its category. */
static bool interval_at(const StandardCounterpointPair *pair, size_t i, StandardCounterpointInterval *interval,
                        StandardCounterpointIntervalCategory *category) {
    return standard_counterpoint_interval_between(pair->lower->notes[i], pair->upper->notes[i], interval) &&
           standard_counterpoint_classify_interval(interval, category);
}

static bool check_all_consonant(const StandardCounterpointPair *pair) {
    for (size_t i = 0; i < pair->lower->count; i++) {
        StandardCounterpointInterval interval;
        StandardCounterpointIntervalCategory category;
        if (!interval_at(pair, i, &interval, &category) || category == STANDARD_COUNTERPOINT_DISSONANCE) {
            return false;
        }
    }
    return true;
}

static bool check_no_repeated_perfect(const StandardCounterpointPair *pair) {
    for (size_t i = 0; i + 1 < pair->lower->count; i++) {
        StandardCounterpointInterval from, to;
        StandardCounterpointIntervalCategory from_category, to_category;
        if (!interval_at(pair, i, &from, &from_category) || !interval_at(pair, i + 1, &to, &to_category)) {
            return false;
        }
        if (from_category == STANDARD_COUNTERPOINT_PERFECT_CONSONANCE &&
            to_category == STANDARD_COUNTERPOINT_PERFECT_CONSONANCE && from.simple_size == to.simple_size) {
            return false;
        }
    }
    return true;
}

static bool check_max_same_imperfect_run(const StandardCounterpointPair *pair) {
    size_t run = 0;
    int previous_size = 0;
    for (size_t i = 0; i < pair->lower->count; i++) {
        StandardCounterpointInterval interval;
        StandardCounterpointIntervalCategory category;
        if (!interval_at(pair, i, &interval, &category)) {
            return false;
        }
        if (category != STANDARD_COUNTERPOINT_IMPERFECT_CONSONANCE) {
            run = 0;
            continue;
        }
        run = (run > 0 && interval.simple_size == previous_size) ? run + 1 : 1;
        previous_size = interval.simple_size;
        if (run > STANDARD_COUNTERPOINT_FIRST_SPECIES_MAX_IMPERFECT_RUN) {
            return false;
        }
    }
    return true;
}

static bool check_imperfect_preferred(const StandardCounterpointPair *pair) {
    for (size_t i = 1; i + 1 < pair->lower->count; i++) {
        StandardCounterpointInterval interval;
        StandardCounterpointIntervalCategory category;
        if (!interval_at(pair, i, &interval, &category) || category == STANDARD_COUNTERPOINT_PERFECT_CONSONANCE) {
            return false;
        }
    }
    return true;
}

static bool check_perfect_then_imperfect(const StandardCounterpointPair *pair) {
    for (size_t i = 0; i + 1 < pair->lower->count; i++) {
        StandardCounterpointInterval interval, next;
        StandardCounterpointIntervalCategory category, next_category;
        if (!interval_at(pair, i, &interval, &category) || !interval_at(pair, i + 1, &next, &next_category)) {
            return false;
        }
        if (category == STANDARD_COUNTERPOINT_PERFECT_CONSONANCE &&
            next_category != STANDARD_COUNTERPOINT_IMPERFECT_CONSONANCE) {
            return false;
        }
    }
    return true;
}

static bool check_interval_variety(const StandardCounterpointPair *pair) {
    size_t count = pair->lower->count;
    bool seen[9] = {false};
    size_t distinct = 0;
    for (size_t i = 0; i < count; i++) {
        StandardCounterpointInterval interval;
        StandardCounterpointIntervalCategory category;
        if (!interval_at(pair, i, &interval, &category)) {
            return false;
        }
        if (!seen[interval.simple_size]) {
            seen[interval.simple_size] = true;
            distinct++;
        }
    }
    size_t required = count < STANDARD_COUNTERPOINT_FIRST_SPECIES_MIN_INTERVAL_VARIETY
                          ? count
                          : STANDARD_COUNTERPOINT_FIRST_SPECIES_MIN_INTERVAL_VARIETY;
    return distinct >= required;
}

bool standard_counterpoint_first_species_harmony_rule_holds(StandardCounterpointFirstSpeciesHarmonyRule which,
                                                            const StandardCounterpointPair *pair) {
    if (!valid_rule(which) || !standard_counterpoint_pair_valid(pair)) {
        return false;
    }
    switch (which) {
        case STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_ALL_CONSONANT:
            return check_all_consonant(pair);
        case STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_NO_REPEATED_PERFECT:
            return check_no_repeated_perfect(pair);
        case STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_NO_DIRECT_PERFECTS:
            return standard_counterpoint_harmonic_rule_holds(STANDARD_COUNTERPOINT_HARMONIC_RULE_DIRECT_PERFECTS,
                                                             pair);
        case STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_MAX_SAME_IMPERFECT_RUN:
            return check_max_same_imperfect_run(pair);
        case STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_IMPERFECT_PREFERRED:
            return check_imperfect_preferred(pair);
        case STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_PERFECT_THEN_IMPERFECT:
            return check_perfect_then_imperfect(pair);
        case STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_INTERVAL_VARIETY:
            return check_interval_variety(pair);
        default:
            return false;
    }
}

bool standard_counterpoint_first_species_harmony_check(const StandardCounterpointPair *pair,
                                                       unsigned *hard_violations, unsigned *soft_violations) {
    if (!standard_counterpoint_pair_valid(pair) || hard_violations == NULL || soft_violations == NULL) {
        return false;
    }
    unsigned hard = 0;
    unsigned soft = 0;
    for (int i = 0; i < STANDARD_COUNTERPOINT_FIRST_SPECIES_HARMONY_COUNT; i++) {
        StandardCounterpointFirstSpeciesHarmonyRule which = (StandardCounterpointFirstSpeciesHarmonyRule)i;
        if (standard_counterpoint_first_species_harmony_rule_holds(which, pair)) {
            continue;
        }
        if (rule_is_soft(which)) {
            soft |= 1u << i;
        } else {
            hard |= 1u << i;
        }
    }
    *hard_violations = hard;
    *soft_violations = soft;
    return true;
}
