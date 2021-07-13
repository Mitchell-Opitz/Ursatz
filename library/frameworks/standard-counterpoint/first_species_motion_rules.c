#include "first_species_motion_rules.h"

#include <stddef.h>
#include <stdio.h>

#include "counterpoint_interval.h"
#include "motion_rules.h"
#include "rule_pair.h"

static const char *const NAMES[STANDARD_COUNTERPOINT_FIRST_SPECIES_MOTION_COUNT] = {
    "motion-variety", "contrary-preferred", "similar-with-leap", "similar-with-large-leap"};

static StandardCounterpointRulePair g_pairs[STANDARD_COUNTERPOINT_FIRST_SPECIES_MOTION_COUNT];
static bool g_built[STANDARD_COUNTERPOINT_FIRST_SPECIES_MOTION_COUNT];

static bool valid_rule(StandardCounterpointFirstSpeciesMotionRule which) {
    return which >= 0 && which < STANDARD_COUNTERPOINT_FIRST_SPECIES_MOTION_COUNT;
}

static void ensure_built(StandardCounterpointFirstSpeciesMotionRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    char name[96];
    snprintf(name, sizeof(name), "first-species-motion-%s", NAMES[which]);
    standard_counterpoint_build_rule_pair(name, CONSTRAINT_TYPE_SOFT, &g_pairs[which]);
}

const Rule *standard_counterpoint_first_species_motion_rule(StandardCounterpointFirstSpeciesMotionRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *
standard_counterpoint_first_species_motion_constraint(StandardCounterpointFirstSpeciesMotionRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

const char *standard_counterpoint_first_species_motion_rule_name(StandardCounterpointFirstSpeciesMotionRule which) {
    return valid_rule(which) ? NAMES[which] : NULL;
}

/* How many motions of each MotionType the texture has. */
static bool count_motions(const StandardCounterpointPair *pair, size_t counts[MOTION_TYPE_COUNT]) {
    for (int i = 0; i < MOTION_TYPE_COUNT; i++) {
        counts[i] = 0;
    }
    for (size_t i = 0; i + 1 < pair->lower->count; i++) {
        MotionType motion;
        if (!standard_counterpoint_pair_motion(pair, i, &motion)) {
            return false;
        }
        counts[motion]++;
    }
    return true;
}

static bool check_motion_variety(const StandardCounterpointPair *pair) {
    size_t counts[MOTION_TYPE_COUNT];
    if (!count_motions(pair, counts)) {
        return false;
    }
    if (pair->lower->count - 1 < STANDARD_COUNTERPOINT_FIRST_SPECIES_MIN_MOTIONS_FOR_VARIETY) {
        return true;
    }
    return counts[MOTION_TYPE_CONTRARY] > 0 && counts[MOTION_TYPE_SIMILAR] > 0 && counts[MOTION_TYPE_PARALLEL] > 0;
}

static bool check_contrary_preferred(const StandardCounterpointPair *pair) {
    size_t counts[MOTION_TYPE_COUNT];
    if (!count_motions(pair, counts)) {
        return false;
    }
    return counts[MOTION_TYPE_CONTRARY] >= counts[MOTION_TYPE_SIMILAR] &&
           counts[MOTION_TYPE_CONTRARY] >= counts[MOTION_TYPE_PARALLEL] &&
           counts[MOTION_TYPE_CONTRARY] >= counts[MOTION_TYPE_OBLIQUE];
}

/* True when no same-direction motion has a part leaping at least
   `min_size` (generic size). */
static bool no_similar_leap(const StandardCounterpointPair *pair, int min_size) {
    for (size_t i = 0; i + 1 < pair->lower->count; i++) {
        MotionType motion;
        StandardCounterpointInterval lower_move, upper_move;
        if (!standard_counterpoint_pair_motion(pair, i, &motion) ||
            !standard_counterpoint_line_interval_at(pair->lower, i, &lower_move) ||
            !standard_counterpoint_line_interval_at(pair->upper, i, &upper_move)) {
            return false;
        }
        if (motion_type_is_same_direction(motion) && (lower_move.size >= min_size || upper_move.size >= min_size)) {
            return false;
        }
    }
    return true;
}

bool standard_counterpoint_first_species_motion_rule_holds(StandardCounterpointFirstSpeciesMotionRule which,
                                                           const StandardCounterpointPair *pair) {
    if (!valid_rule(which) || !standard_counterpoint_pair_valid(pair)) {
        return false;
    }
    switch (which) {
        case STANDARD_COUNTERPOINT_FIRST_SPECIES_MOTION_VARIETY:
            return check_motion_variety(pair);
        case STANDARD_COUNTERPOINT_FIRST_SPECIES_MOTION_CONTRARY_PREFERRED:
            return check_contrary_preferred(pair);
        case STANDARD_COUNTERPOINT_FIRST_SPECIES_MOTION_SIMILAR_WITH_LEAP:
            return no_similar_leap(pair, 3);
        case STANDARD_COUNTERPOINT_FIRST_SPECIES_MOTION_SIMILAR_WITH_LARGE_LEAP:
            return no_similar_leap(pair, STANDARD_COUNTERPOINT_FIRST_SPECIES_LARGE_LEAP);
        default:
            return false;
    }
}

bool standard_counterpoint_first_species_motion_check(const StandardCounterpointPair *pair, unsigned *violations) {
    if (!standard_counterpoint_pair_valid(pair) || violations == NULL) {
        return false;
    }
    unsigned mask = 0;
    for (int i = 0; i < STANDARD_COUNTERPOINT_FIRST_SPECIES_MOTION_COUNT; i++) {
        if (!standard_counterpoint_first_species_motion_rule_holds((StandardCounterpointFirstSpeciesMotionRule)i,
                                                                   pair)) {
            mask |= 1u << i;
        }
    }
    *violations = mask;
    return true;
}
