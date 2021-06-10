#include "tendency_rules.h"

#include <stddef.h>
#include <stdio.h>

#include "counterpoint_interval.h"
#include "counterpoint_pitch.h"
#include "rule_pair.h"

static const char *const NAMES[STANDARD_COUNTERPOINT_TENDENCY_COUNT] = {
    "pitch-proximity", "step-declination", "step-inertia", "melodic-regression", "melodic-arch"};

static StandardCounterpointRulePair g_pairs[STANDARD_COUNTERPOINT_TENDENCY_COUNT];
static bool g_built[STANDARD_COUNTERPOINT_TENDENCY_COUNT];

static bool valid_rule(StandardCounterpointTendency which) {
    return which >= 0 && which < STANDARD_COUNTERPOINT_TENDENCY_COUNT;
}

static void ensure_built(StandardCounterpointTendency which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    char name[96];
    snprintf(name, sizeof(name), "tendency-%s", NAMES[which]);
    standard_counterpoint_build_rule_pair(name, CONSTRAINT_TYPE_SOFT, &g_pairs[which]);
}

const Rule *standard_counterpoint_tendency_rule(StandardCounterpointTendency which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_counterpoint_tendency_constraint(StandardCounterpointTendency which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

const char *standard_counterpoint_tendency_name(StandardCounterpointTendency which) {
    return valid_rule(which) ? NAMES[which] : NULL;
}

static bool check_pitch_proximity(const StandardCounterpointLine *line) {
    size_t steps = 0, leaps = 0, small_leaps = 0, large_leaps = 0;
    for (size_t i = 0; i + 1 < line->count; i++) {
        StandardCounterpointInterval interval;
        if (!standard_counterpoint_line_interval_at(line, i, &interval)) {
            return false;
        }
        if (standard_counterpoint_interval_is_step(&interval)) {
            steps++;
        } else if (standard_counterpoint_interval_is_leap(&interval)) {
            leaps++;
            if (interval.size <= 4) {
                small_leaps++;
            } else {
                large_leaps++;
            }
        }
    }
    return steps >= leaps && small_leaps >= large_leaps;
}

static bool check_step_declination(const StandardCounterpointLine *line) {
    size_t descending = 0, ascending = 0;
    for (size_t i = 0; i + 1 < line->count; i++) {
        StandardCounterpointInterval interval;
        if (!standard_counterpoint_line_interval_at(line, i, &interval)) {
            return false;
        }
        if (standard_counterpoint_interval_is_step(&interval)) {
            if (standard_counterpoint_line_direction_at(line, i) > 0) {
                ascending++;
            } else {
                descending++;
            }
        }
    }
    return descending >= ascending;
}

static bool check_step_inertia(const StandardCounterpointLine *line) {
    size_t continuations = 0, reversals = 0;
    for (size_t i = 0; i + 2 < line->count; i++) {
        int first = standard_counterpoint_line_direction_at(line, i);
        int second = standard_counterpoint_line_direction_at(line, i + 1);
        if (first == 0 || second == 0) {
            continue;
        }
        if (first == second) {
            continuations++;
        } else {
            reversals++;
        }
    }
    return continuations >= reversals;
}

static bool check_melodic_regression(const StandardCounterpointLine *line) {
    int low = standard_counterpoint_semitone_number(line->notes[standard_counterpoint_line_lowest_index(line)]);
    int high = standard_counterpoint_semitone_number(line->notes[standard_counterpoint_line_highest_index(line)]);
    int range = high - low;
    if (range == 0) {
        return true;
    }
    for (size_t i = 0; i + 1 < line->count; i++) {
        int above_low = standard_counterpoint_semitone_number(line->notes[i]) - low;
        int direction = standard_counterpoint_line_direction_at(line, i);
        if (4 * above_low >= 3 * range && direction > 0) {
            return false;
        }
        if (4 * above_low <= range && direction < 0) {
            return false;
        }
    }
    return true;
}

static bool check_melodic_arch(const StandardCounterpointLine *line) {
    size_t count = line->count;
    size_t climax = standard_counterpoint_line_highest_index(line);
    if (climax == 0 || climax + 1 >= count) {
        return false;
    }
    if (4 * climax < count || 4 * climax > 3 * count) {
        return false;
    }
    const PitchSpelling *top = line->notes[climax];
    return standard_counterpoint_direction(line->notes[count - 1], top) != 0;
}

bool standard_counterpoint_tendency_holds(StandardCounterpointTendency which,
                                          const StandardCounterpointLine *line) {
    if (!valid_rule(which) || !standard_counterpoint_line_valid(line)) {
        return false;
    }
    switch (which) {
        case STANDARD_COUNTERPOINT_TENDENCY_PITCH_PROXIMITY:
            return check_pitch_proximity(line);
        case STANDARD_COUNTERPOINT_TENDENCY_STEP_DECLINATION:
            return check_step_declination(line);
        case STANDARD_COUNTERPOINT_TENDENCY_STEP_INERTIA:
            return check_step_inertia(line);
        case STANDARD_COUNTERPOINT_TENDENCY_MELODIC_REGRESSION:
            return check_melodic_regression(line);
        case STANDARD_COUNTERPOINT_TENDENCY_MELODIC_ARCH:
            return check_melodic_arch(line);
        default:
            return false;
    }
}

bool standard_counterpoint_tendency_check(const StandardCounterpointLine *line, unsigned *violations) {
    if (!standard_counterpoint_line_valid(line) || violations == NULL) {
        return false;
    }
    unsigned mask = 0;
    for (int i = 0; i < STANDARD_COUNTERPOINT_TENDENCY_COUNT; i++) {
        if (!standard_counterpoint_tendency_holds((StandardCounterpointTendency)i, line)) {
            mask |= 1u << i;
        }
    }
    *violations = mask;
    return true;
}
