#include "melodic_rules.h"

#include <stddef.h>

#include "counterpoint_pitch.h"
#include "counterpoint_subject.h"
#include "rule_table.h"

static const char *const NAMES[STANDARD_COUNTERPOINT_MELODIC_RULE_COUNT] = {
    "clausula-vera",    "repeated-notes", "pitch-occurrences", "consecutive-leaps",
    "leap-density",     "repeated-generic", "unique-climax",    "climax-not-final",
    "climax-by-leap",   "forbidden-leap",  "forbidden-outline"};

static bool rule_is_hard(StandardCounterpointMelodicRule which) {
    return which == STANDARD_COUNTERPOINT_MELODIC_RULE_CLAUSULA_VERA ||
           which == STANDARD_COUNTERPOINT_MELODIC_RULE_FORBIDDEN_LEAP ||
           which == STANDARD_COUNTERPOINT_MELODIC_RULE_FORBIDDEN_OUTLINE;
}

static bool table_is_hard(int which) {
    return rule_is_hard((StandardCounterpointMelodicRule)which);
}

static StandardCounterpointRulePair g_pairs[STANDARD_COUNTERPOINT_MELODIC_RULE_COUNT];
static bool g_built[STANDARD_COUNTERPOINT_MELODIC_RULE_COUNT];
static ConstraintEvalResult evaluate_candidate(const Constraint *constraint, const Value *candidate,
                                               const Context *context);
static PreferenceScore score_candidate(const Preference *preference, const Value *candidate, const Context *context);
static const StandardCounterpointRuleTable TABLE = {
    "melodic", NAMES, STANDARD_COUNTERPOINT_MELODIC_RULE_COUNT, table_is_hard, g_pairs, g_built, evaluate_candidate, score_candidate};

static bool valid_rule(StandardCounterpointMelodicRule which) {
    return standard_counterpoint_rule_table_valid(&TABLE, (int)which);
}

const Rule *standard_counterpoint_melodic_rule(StandardCounterpointMelodicRule which) {
    return standard_counterpoint_rule_table_rule(&TABLE, (int)which);
}

const Constraint *standard_counterpoint_melodic_constraint(StandardCounterpointMelodicRule which) {
    return standard_counterpoint_rule_table_constraint(&TABLE, (int)which);
}

const Preference *standard_counterpoint_melodic_preference(StandardCounterpointMelodicRule which) {
    return standard_counterpoint_rule_table_preference(&TABLE, (int)which);
}

const char *standard_counterpoint_melodic_rule_name(StandardCounterpointMelodicRule which) {
    return standard_counterpoint_rule_table_name(&TABLE, (int)which);
}

static bool leap_at(const StandardCounterpointLine *line, size_t i) {
    StandardCounterpointInterval interval;
    return standard_counterpoint_line_interval_at(line, i, &interval) &&
           standard_counterpoint_interval_is_leap(&interval);
}

static bool check_clausula_vera(const StandardCounterpointLine *line) {
    StandardCounterpointInterval interval;
    return line->count >= 2 && standard_counterpoint_line_interval_at(line, line->count - 2, &interval) &&
           standard_counterpoint_interval_is_step(&interval);
}

static bool check_repeated_notes(const StandardCounterpointLine *line, const StandardCounterpointLimits *limits) {
    size_t run = 1;
    for (size_t i = 1; i < line->count; i++) {
        run = standard_counterpoint_same_pitch(line->notes[i - 1], line->notes[i]) ? run + 1 : 1;
        if (run > limits->max_consecutive_repeats) {
            return false;
        }
    }
    return true;
}

static bool check_pitch_occurrences(const StandardCounterpointLine *line, const StandardCounterpointLimits *limits) {
    for (size_t i = 0; i < line->count; i++) {
        size_t occurrences = 0;
        for (size_t j = 0; j < line->count; j++) {
            if (standard_counterpoint_same_pitch(line->notes[i], line->notes[j])) {
                occurrences++;
            }
        }
        if (occurrences > limits->max_pitch_occurrences) {
            return false;
        }
    }
    return true;
}

static bool check_consecutive_leaps(const StandardCounterpointLine *line, const StandardCounterpointLimits *limits) {
    size_t run = 0;
    for (size_t i = 0; i + 1 < line->count; i++) {
        run = leap_at(line, i) ? run + 1 : 0;
        if (run > limits->max_consecutive_leaps) {
            return false;
        }
    }
    return true;
}

static bool check_leap_density(const StandardCounterpointLine *line, const StandardCounterpointLimits *limits) {
    size_t intervals = line->count - 1;
    size_t window = limits->leap_window;
    if (window == 0) {
        return true;
    }
    if (window > intervals) {
        window = intervals;
    }
    for (size_t start = 0; start + window <= intervals; start++) {
        size_t leaps = 0;
        for (size_t i = start; i < start + window; i++) {
            if (leap_at(line, i)) {
                leaps++;
            }
        }
        if (leaps > limits->max_leaps_in_window) {
            return false;
        }
    }
    return true;
}

static bool check_repeated_generic(const StandardCounterpointLine *line, const StandardCounterpointLimits *limits) {
    size_t run = 0;
    int previous_size = 0;
    for (size_t i = 0; i + 1 < line->count; i++) {
        StandardCounterpointInterval interval;
        if (!standard_counterpoint_line_interval_at(line, i, &interval)) {
            return false;
        }
        run = (run > 0 && interval.size == previous_size) ? run + 1 : 1;
        previous_size = interval.size;
        if (run > limits->max_same_generic_run) {
            return false;
        }
    }
    return true;
}

static bool check_unique_climax(const StandardCounterpointLine *line) {
    const PitchSpelling *top = line->notes[standard_counterpoint_line_highest_index(line)];
    size_t occurrences = 0;
    for (size_t i = 0; i < line->count; i++) {
        if (standard_counterpoint_direction(line->notes[i], top) == 0) {
            occurrences++;
        }
    }
    return occurrences == 1;
}

static bool check_climax_not_final(const StandardCounterpointLine *line) {
    const PitchSpelling *top = line->notes[standard_counterpoint_line_highest_index(line)];
    return standard_counterpoint_direction(line->notes[line->count - 1], top) != 0;
}

static bool check_climax_by_leap(const StandardCounterpointLine *line) {
    size_t climax = standard_counterpoint_line_highest_index(line);
    return climax > 0 && leap_at(line, climax - 1);
}

static bool leap_allowed(const StandardCounterpointLine *line, size_t i) {
    StandardCounterpointInterval interval;
    if (!standard_counterpoint_line_interval_at(line, i, &interval)) {
        return false;
    }
    if (interval.size > 8) {
        return false;
    }
    if (interval.quality == STANDARD_COUNTERPOINT_QUALITY_AUGMENTED ||
        interval.quality == STANDARD_COUNTERPOINT_QUALITY_DIMINISHED) {
        return false;
    }
    if (interval.simple_size == 7) {
        return false;
    }
    if (interval.simple_size == 6) {
        return interval.quality == STANDARD_COUNTERPOINT_QUALITY_MINOR &&
               standard_counterpoint_line_direction_at(line, i) > 0;
    }
    return true;
}

static bool check_forbidden_leap(const StandardCounterpointLine *line) {
    for (size_t i = 0; i + 1 < line->count; i++) {
        if (!leap_allowed(line, i)) {
            return false;
        }
    }
    return true;
}

static bool check_forbidden_outline(const StandardCounterpointLine *line) {
    size_t i = 0;
    while (i + 1 < line->count) {
        int direction = standard_counterpoint_line_direction_at(line, i);
        if (direction == 0) {
            i++;
            continue;
        }
        size_t end = i + 1;
        while (end + 1 < line->count && standard_counterpoint_line_direction_at(line, end) == direction) {
            end++;
        }
        StandardCounterpointInterval outline;
        if (!standard_counterpoint_interval_between(line->notes[i], line->notes[end], &outline)) {
            return false;
        }
        if (outline.quality == STANDARD_COUNTERPOINT_QUALITY_AUGMENTED ||
            outline.quality == STANDARD_COUNTERPOINT_QUALITY_DIMINISHED || outline.simple_size == 7) {
            return false;
        }
        i = end;
    }
    return true;
}

bool standard_counterpoint_melodic_rule_holds(StandardCounterpointMelodicRule which,
                                              const StandardCounterpointLine *line,
                                              const StandardCounterpointLimits *limits) {
    if (!valid_rule(which) || !standard_counterpoint_line_valid(line) || limits == NULL) {
        return false;
    }
    switch (which) {
        case STANDARD_COUNTERPOINT_MELODIC_RULE_CLAUSULA_VERA:
            return check_clausula_vera(line);
        case STANDARD_COUNTERPOINT_MELODIC_RULE_REPEATED_NOTES:
            return check_repeated_notes(line, limits);
        case STANDARD_COUNTERPOINT_MELODIC_RULE_PITCH_OCCURRENCES:
            return check_pitch_occurrences(line, limits);
        case STANDARD_COUNTERPOINT_MELODIC_RULE_CONSECUTIVE_LEAPS:
            return check_consecutive_leaps(line, limits);
        case STANDARD_COUNTERPOINT_MELODIC_RULE_LEAP_DENSITY:
            return line->count < 2 || check_leap_density(line, limits);
        case STANDARD_COUNTERPOINT_MELODIC_RULE_REPEATED_GENERIC:
            return check_repeated_generic(line, limits);
        case STANDARD_COUNTERPOINT_MELODIC_RULE_UNIQUE_CLIMAX:
            return check_unique_climax(line);
        case STANDARD_COUNTERPOINT_MELODIC_RULE_CLIMAX_NOT_FINAL:
            return check_climax_not_final(line);
        case STANDARD_COUNTERPOINT_MELODIC_RULE_CLIMAX_BY_LEAP:
            return check_climax_by_leap(line);
        case STANDARD_COUNTERPOINT_MELODIC_RULE_FORBIDDEN_LEAP:
            return check_forbidden_leap(line);
        case STANDARD_COUNTERPOINT_MELODIC_RULE_FORBIDDEN_OUTLINE:
            return check_forbidden_outline(line);
        default:
            return false;
    }
}

typedef StandardCounterpointLineLimits Subject;

static bool holds_adapter(int which, const void *subject) {
    const Subject *s = subject;
    return standard_counterpoint_melodic_rule_holds((StandardCounterpointMelodicRule)which, s->line, s->limits);
}

STANDARD_COUNTERPOINT_RULE_TABLE_EVALUATOR(evaluate_candidate, TABLE, holds_adapter, Subject)
STANDARD_COUNTERPOINT_RULE_TABLE_SCORER(score_candidate, TABLE, holds_adapter, Subject)

bool standard_counterpoint_melodic_check(const StandardCounterpointLine *line,
                                         const StandardCounterpointLimits *limits, unsigned *hard_violations,
                                         unsigned *soft_violations) {
    if (!standard_counterpoint_line_valid(line) || limits == NULL || hard_violations == NULL ||
        soft_violations == NULL) {
        return false;
    }
    Subject subject = {line, limits};
    standard_counterpoint_rule_table_check(&TABLE, holds_adapter, &subject, hard_violations,
                                           soft_violations);
    return true;
}
