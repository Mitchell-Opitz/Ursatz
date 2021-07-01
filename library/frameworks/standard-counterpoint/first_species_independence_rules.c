#include "first_species_independence_rules.h"

#include <stddef.h>
#include <stdio.h>

#include "counterpoint_interval.h"
#include "counterpoint_pitch.h"
#include "rule_pair.h"

static const char *const NAMES[STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_COUNT] = {
    "single-repeat", "no-repeat", "no-overlap", "emergency-gap"};

static bool rule_is_hard(StandardCounterpointFirstSpeciesIndependenceRule which) {
    return which == STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_SINGLE_REPEAT;
}

static StandardCounterpointRulePair g_pairs[STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_COUNT];
static bool g_built[STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_COUNT];

static bool valid_rule(StandardCounterpointFirstSpeciesIndependenceRule which) {
    return which >= 0 && which < STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_COUNT;
}

static void ensure_built(StandardCounterpointFirstSpeciesIndependenceRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    char name[96];
    snprintf(name, sizeof(name), "first-species-independence-%s", NAMES[which]);
    standard_counterpoint_build_rule_pair(name, rule_is_hard(which) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT,
                                          &g_pairs[which]);
}

const Rule *
standard_counterpoint_first_species_independence_rule(StandardCounterpointFirstSpeciesIndependenceRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *
standard_counterpoint_first_species_independence_constraint(StandardCounterpointFirstSpeciesIndependenceRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

const char *
standard_counterpoint_first_species_independence_rule_name(StandardCounterpointFirstSpeciesIndependenceRule which) {
    return valid_rule(which) ? NAMES[which] : NULL;
}

static size_t repeated_notes(const StandardCounterpointLine *line) {
    size_t repeats = 0;
    for (size_t i = 0; i + 1 < line->count; i++) {
        if (standard_counterpoint_same_pitch(line->notes[i], line->notes[i + 1])) {
            repeats++;
        }
    }
    return repeats;
}

static bool check_no_overlap(const StandardCounterpointPair *pair) {
    for (size_t i = 0; i + 1 < pair->lower->count; i++) {
        if (standard_counterpoint_direction(pair->lower->notes[i], pair->upper->notes[i + 1]) < 0) {
            return false;
        }
        if (standard_counterpoint_direction(pair->upper->notes[i], pair->lower->notes[i + 1]) > 0) {
            return false;
        }
    }
    return true;
}

static bool check_emergency_gap(const StandardCounterpointPair *pair) {
    size_t wide = 0;
    for (size_t i = 0; i < pair->lower->count; i++) {
        StandardCounterpointInterval interval;
        if (!standard_counterpoint_interval_between(pair->lower->notes[i], pair->upper->notes[i], &interval)) {
            return false;
        }
        if (interval.size > STANDARD_COUNTERPOINT_FIRST_SPECIES_TENTH) {
            wide++;
        }
    }
    return wide <= STANDARD_COUNTERPOINT_FIRST_SPECIES_MAX_EMERGENCY_NOTES;
}

bool standard_counterpoint_first_species_independence_rule_holds(
    StandardCounterpointFirstSpeciesIndependenceRule which, const StandardCounterpointPair *pair,
    StandardCounterpointCantusPart cantus_part) {
    if (!valid_rule(which) || !standard_counterpoint_pair_valid(pair) ||
        (cantus_part != STANDARD_COUNTERPOINT_CANTUS_IN_LOWER &&
         cantus_part != STANDARD_COUNTERPOINT_CANTUS_IN_UPPER)) {
        return false;
    }
    const StandardCounterpointLine *counterpoint =
        cantus_part == STANDARD_COUNTERPOINT_CANTUS_IN_LOWER ? pair->upper : pair->lower;
    switch (which) {
        case STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_SINGLE_REPEAT:
            return repeated_notes(counterpoint) <= 1;
        case STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_NO_REPEAT:
            return repeated_notes(counterpoint) == 0;
        case STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_NO_OVERLAP:
            return check_no_overlap(pair);
        case STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_EMERGENCY_GAP:
            return check_emergency_gap(pair);
        default:
            return false;
    }
}

bool standard_counterpoint_first_species_independence_check(const StandardCounterpointPair *pair,
                                                            StandardCounterpointCantusPart cantus_part,
                                                            unsigned *hard_violations, unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL || !standard_counterpoint_pair_valid(pair) ||
        (cantus_part != STANDARD_COUNTERPOINT_CANTUS_IN_LOWER &&
         cantus_part != STANDARD_COUNTERPOINT_CANTUS_IN_UPPER)) {
        return false;
    }
    unsigned hard = 0;
    unsigned soft = 0;
    for (int i = 0; i < STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_COUNT; i++) {
        StandardCounterpointFirstSpeciesIndependenceRule which = (StandardCounterpointFirstSpeciesIndependenceRule)i;
        if (standard_counterpoint_first_species_independence_rule_holds(which, pair, cantus_part)) {
            continue;
        }
        if (rule_is_hard(which)) {
            hard |= 1u << i;
        } else {
            soft |= 1u << i;
        }
    }
    *hard_violations = hard;
    *soft_violations = soft;
    return true;
}
