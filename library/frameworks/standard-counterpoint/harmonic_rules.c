#include "harmonic_rules.h"

#include <stddef.h>

#include "consonance_rules.h"
#include "counterpoint_interval.h"
#include "counterpoint_pitch.h"
#include "motion_rules.h"
#include "rule_table.h"

static const char *const NAMES[STANDARD_COUNTERPOINT_HARMONIC_RULE_COUNT] = {
    "first-interval",   "last-interval",    "max-gap",          "mostly-within-octave", "no-crossing",
    "parallel-octaves", "parallel-fifths",  "direct-perfects",  "interior-unisons",     "climax-coordination"};

static bool rule_is_soft(StandardCounterpointHarmonicRule which) {
    return which == STANDARD_COUNTERPOINT_HARMONIC_RULE_MOSTLY_WITHIN_OCTAVE ||
           which == STANDARD_COUNTERPOINT_HARMONIC_RULE_DIRECT_PERFECTS ||
           which == STANDARD_COUNTERPOINT_HARMONIC_RULE_INTERIOR_UNISONS ||
           which == STANDARD_COUNTERPOINT_HARMONIC_RULE_CLIMAX_COORDINATION;
}

static bool table_is_hard(int which) {
    return !rule_is_soft((StandardCounterpointHarmonicRule)which);
}

static StandardCounterpointRulePair g_pairs[STANDARD_COUNTERPOINT_HARMONIC_RULE_COUNT];
static bool g_built[STANDARD_COUNTERPOINT_HARMONIC_RULE_COUNT];
static const StandardCounterpointRuleTable TABLE = {
    "harmonic", NAMES, STANDARD_COUNTERPOINT_HARMONIC_RULE_COUNT, table_is_hard, g_pairs, g_built};

static bool valid_rule(StandardCounterpointHarmonicRule which) {
    return standard_counterpoint_rule_table_valid(&TABLE, (int)which);
}

const Rule *standard_counterpoint_harmonic_rule(StandardCounterpointHarmonicRule which) {
    return standard_counterpoint_rule_table_rule(&TABLE, (int)which);
}

const Constraint *standard_counterpoint_harmonic_constraint(StandardCounterpointHarmonicRule which) {
    return standard_counterpoint_rule_table_constraint(&TABLE, (int)which);
}

const char *standard_counterpoint_harmonic_rule_name(StandardCounterpointHarmonicRule which) {
    return standard_counterpoint_rule_table_name(&TABLE, (int)which);
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

static bool check_no_crossing(const StandardCounterpointPair *pair) {
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
        case STANDARD_COUNTERPOINT_HARMONIC_RULE_NO_CROSSING:
            return check_no_crossing(pair);
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

static bool holds_adapter(int which, const void *subject) {
    return standard_counterpoint_harmonic_rule_holds((StandardCounterpointHarmonicRule)which, subject);
}

bool standard_counterpoint_harmonic_check(const StandardCounterpointPair *pair, unsigned *hard_violations,
                                          unsigned *soft_violations) {
    if (!standard_counterpoint_pair_valid(pair) || hard_violations == NULL || soft_violations == NULL) {
        return false;
    }
    standard_counterpoint_rule_table_check(&TABLE, holds_adapter, pair, hard_violations,
                                           soft_violations);
    return true;
}
