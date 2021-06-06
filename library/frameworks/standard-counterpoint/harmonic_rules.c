#include "harmonic_rules.h"

#include <stddef.h>
#include <stdio.h>

#include "consonance_rules.h"
#include "counterpoint_interval.h"
#include "counterpoint_pitch.h"
#include "motion_rules.h"
#include "rule_pair.h"

static const char *const NAMES[STANDARD_COUNTERPOINT_HARMONIC_RULE_COUNT] = {
    "first-interval",   "last-interval",    "max-gap",          "mostly-within-octave", "no-overlap",
    "parallel-octaves", "parallel-fifths",  "direct-perfects",  "interior-unisons",     "climax-coordination"};

static bool rule_is_soft(StandardCounterpointHarmonicRule which) {
    return which == STANDARD_COUNTERPOINT_HARMONIC_RULE_MOSTLY_WITHIN_OCTAVE ||
           which == STANDARD_COUNTERPOINT_HARMONIC_RULE_DIRECT_PERFECTS ||
           which == STANDARD_COUNTERPOINT_HARMONIC_RULE_INTERIOR_UNISONS ||
           which == STANDARD_COUNTERPOINT_HARMONIC_RULE_CLIMAX_COORDINATION;
}

static StandardCounterpointRulePair g_pairs[STANDARD_COUNTERPOINT_HARMONIC_RULE_COUNT];
static bool g_built[STANDARD_COUNTERPOINT_HARMONIC_RULE_COUNT];

static bool valid_rule(StandardCounterpointHarmonicRule which) {
    return which >= 0 && which < STANDARD_COUNTERPOINT_HARMONIC_RULE_COUNT;
}

static void ensure_built(StandardCounterpointHarmonicRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    char name[96];
    snprintf(name, sizeof(name), "harmonic-%s", NAMES[which]);
    standard_counterpoint_build_rule_pair(name, rule_is_soft(which) ? CONSTRAINT_TYPE_SOFT : CONSTRAINT_TYPE_HARD,
                                          &g_pairs[which]);
}

const Rule *standard_counterpoint_harmonic_rule(StandardCounterpointHarmonicRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_counterpoint_harmonic_constraint(StandardCounterpointHarmonicRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

const char *standard_counterpoint_harmonic_rule_name(StandardCounterpointHarmonicRule which) {
    return valid_rule(which) ? NAMES[which] : NULL;
}

/* The interval between the two parts at position i. */
static bool interval_at(const StandardCounterpointPair *pair, size_t i, StandardCounterpointInterval *out) {
    return standard_counterpoint_interval_between(pair->lower->notes[i], pair->upper->notes[i], out);
}

static bool is_perfect(const StandardCounterpointInterval *interval, int simple_size) {
    return interval->quality == STANDARD_COUNTERPOINT_QUALITY_PERFECT && interval->simple_size == simple_size;
}

static bool is_octave_class(const StandardCounterpointInterval *interval) {
    return is_perfect(interval, 1) || is_perfect(interval, 8);
}

static bool check_first_interval(const StandardCounterpointPair *pair) {
    StandardCounterpointIntervalCategory category;
    return standard_counterpoint_classify_pitches(pair->lower->notes[0], pair->upper->notes[0], &category) &&
           category == STANDARD_COUNTERPOINT_PERFECT_CONSONANCE;
}

static bool check_last_interval(const StandardCounterpointPair *pair) {
    StandardCounterpointInterval interval;
    return interval_at(pair, pair->lower->count - 1, &interval) && is_octave_class(&interval);
}

static bool check_max_gap(const StandardCounterpointPair *pair) {
    for (size_t i = 0; i < pair->lower->count; i++) {
        StandardCounterpointInterval interval;
        if (!interval_at(pair, i, &interval) || interval.size > STANDARD_COUNTERPOINT_MAX_GAP) {
            return false;
        }
    }
    return true;
}

static bool check_mostly_within_octave(const StandardCounterpointPair *pair) {
    size_t wide = 0;
    for (size_t i = 0; i < pair->lower->count; i++) {
        StandardCounterpointInterval interval;
        if (!interval_at(pair, i, &interval)) {
            return false;
        }
        if (interval.size > STANDARD_COUNTERPOINT_USUAL_GAP) {
            wide++;
        }
    }
    return wide * 4 <= pair->lower->count;
}

static bool check_no_overlap(const StandardCounterpointPair *pair) {
    for (size_t i = 0; i < pair->lower->count; i++) {
        if (standard_counterpoint_direction(pair->lower->notes[i], pair->upper->notes[i]) < 0) {
            return false;
        }
    }
    return true;
}

/* True when positions i and i + 1 are both a perfect interval satisfying
   `same` and are joined by parallel motion. */
static bool parallel_perfect_at(const StandardCounterpointPair *pair, size_t i,
                                bool (*same)(const StandardCounterpointInterval *)) {
    StandardCounterpointInterval from, to;
    MotionType motion;
    if (!interval_at(pair, i, &from) || !interval_at(pair, i + 1, &to) ||
        !standard_counterpoint_pair_motion(pair, i, &motion)) {
        return false;
    }
    return same(&from) && same(&to) && motion == MOTION_TYPE_PARALLEL;
}

static bool is_fifth(const StandardCounterpointInterval *interval) {
    return is_perfect(interval, 5);
}

static bool check_parallel_octaves(const StandardCounterpointPair *pair) {
    for (size_t i = 0; i + 1 < pair->lower->count; i++) {
        if (parallel_perfect_at(pair, i, is_octave_class)) {
            return false;
        }
    }
    return true;
}

static bool check_parallel_fifths(const StandardCounterpointPair *pair) {
    for (size_t i = 0; i + 1 < pair->lower->count; i++) {
        if (parallel_perfect_at(pair, i, is_fifth)) {
            return false;
        }
    }
    return true;
}

static bool check_direct_perfects(const StandardCounterpointPair *pair) {
    for (size_t i = 0; i + 1 < pair->lower->count; i++) {
        StandardCounterpointInterval to;
        MotionType motion;
        if (!interval_at(pair, i + 1, &to) || !standard_counterpoint_pair_motion(pair, i, &motion)) {
            return false;
        }
        if (motion_type_is_same_direction(motion) && (is_octave_class(&to) || is_fifth(&to))) {
            return false;
        }
    }
    return true;
}

static bool check_interior_unisons(const StandardCounterpointPair *pair) {
    for (size_t i = 1; i + 1 < pair->lower->count; i++) {
        StandardCounterpointInterval interval;
        if (!interval_at(pair, i, &interval) || is_perfect(&interval, 1)) {
            return false;
        }
    }
    return true;
}

static bool check_climax_coordination(const StandardCounterpointPair *pair) {
    const PitchSpelling *upper_top = pair->upper->notes[standard_counterpoint_line_highest_index(pair->upper)];
    const PitchSpelling *lower_top = pair->lower->notes[standard_counterpoint_line_highest_index(pair->lower)];
    for (size_t i = 0; i < pair->lower->count; i++) {
        if (standard_counterpoint_direction(pair->upper->notes[i], upper_top) == 0 &&
            standard_counterpoint_direction(pair->lower->notes[i], lower_top) == 0) {
            return false;
        }
    }
    return true;
}

bool standard_counterpoint_harmonic_rule_holds(StandardCounterpointHarmonicRule which,
                                               const StandardCounterpointPair *pair) {
    if (!valid_rule(which) || !standard_counterpoint_pair_valid(pair)) {
        return false;
    }
    switch (which) {
        case STANDARD_COUNTERPOINT_HARMONIC_RULE_FIRST_INTERVAL:
            return check_first_interval(pair);
        case STANDARD_COUNTERPOINT_HARMONIC_RULE_LAST_INTERVAL:
            return check_last_interval(pair);
        case STANDARD_COUNTERPOINT_HARMONIC_RULE_MAX_GAP:
            return check_max_gap(pair);
        case STANDARD_COUNTERPOINT_HARMONIC_RULE_MOSTLY_WITHIN_OCTAVE:
            return check_mostly_within_octave(pair);
        case STANDARD_COUNTERPOINT_HARMONIC_RULE_NO_OVERLAP:
            return check_no_overlap(pair);
        case STANDARD_COUNTERPOINT_HARMONIC_RULE_PARALLEL_OCTAVES:
            return check_parallel_octaves(pair);
        case STANDARD_COUNTERPOINT_HARMONIC_RULE_PARALLEL_FIFTHS:
            return check_parallel_fifths(pair);
        case STANDARD_COUNTERPOINT_HARMONIC_RULE_DIRECT_PERFECTS:
            return check_direct_perfects(pair);
        case STANDARD_COUNTERPOINT_HARMONIC_RULE_INTERIOR_UNISONS:
            return check_interior_unisons(pair);
        case STANDARD_COUNTERPOINT_HARMONIC_RULE_CLIMAX_COORDINATION:
            return check_climax_coordination(pair);
        default:
            return false;
    }
}

bool standard_counterpoint_harmonic_check(const StandardCounterpointPair *pair, unsigned *hard_violations,
                                          unsigned *soft_violations) {
    if (!standard_counterpoint_pair_valid(pair) || hard_violations == NULL || soft_violations == NULL) {
        return false;
    }
    unsigned hard = 0;
    unsigned soft = 0;
    for (int i = 0; i < STANDARD_COUNTERPOINT_HARMONIC_RULE_COUNT; i++) {
        StandardCounterpointHarmonicRule which = (StandardCounterpointHarmonicRule)i;
        if (standard_counterpoint_harmonic_rule_holds(which, pair)) {
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
