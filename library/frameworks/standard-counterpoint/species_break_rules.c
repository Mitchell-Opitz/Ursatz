#include "species_break_rules.h"

#include <stddef.h>

#include "consonance_rules.h"
#include "counterpoint_interval.h"
#include "counterpoint_pitch.h"
#include "rule_table.h"

static const char *const NAMES[STANDARD_COUNTERPOINT_SPECIES_BREAK_COUNT] = {"downbeat-consonant", "weak-passing",
                                                                              "limits"};

static bool rule_is_soft(StandardCounterpointSpeciesBreakRule which) {
    return which == STANDARD_COUNTERPOINT_SPECIES_BREAK_LIMITS;
}

static bool table_is_hard(int which) {
    return !rule_is_soft((StandardCounterpointSpeciesBreakRule)which);
}

static StandardCounterpointRulePair g_pairs[STANDARD_COUNTERPOINT_SPECIES_BREAK_COUNT];
static bool g_built[STANDARD_COUNTERPOINT_SPECIES_BREAK_COUNT];
static ConstraintEvalResult evaluate_candidate(const Constraint *constraint, const Value *candidate,
                                               const Context *context);
static PreferenceScore score_candidate(const Preference *preference, const Value *candidate, const Context *context);
static const StandardCounterpointRuleTable TABLE = {
    "species-break", NAMES, STANDARD_COUNTERPOINT_SPECIES_BREAK_COUNT, table_is_hard, g_pairs, g_built,
    evaluate_candidate, score_candidate};

static bool valid_rule(StandardCounterpointSpeciesBreakRule which) {
    return standard_counterpoint_rule_table_valid(&TABLE, (int)which);
}

const Rule *standard_counterpoint_species_break_rule(StandardCounterpointSpeciesBreakRule which) {
    return standard_counterpoint_rule_table_rule(&TABLE, (int)which);
}

const Constraint *standard_counterpoint_species_break_constraint(StandardCounterpointSpeciesBreakRule which) {
    return standard_counterpoint_rule_table_constraint(&TABLE, (int)which);
}

const Preference *standard_counterpoint_species_break_preference(StandardCounterpointSpeciesBreakRule which) {
    return standard_counterpoint_rule_table_preference(&TABLE, (int)which);
}

const char *standard_counterpoint_species_break_rule_name(StandardCounterpointSpeciesBreakRule which) {
    return standard_counterpoint_rule_table_name(&TABLE, (int)which);
}

typedef StandardCounterpointFourthSpeciesExercise Exercise;

static bool consonant_at(const StandardCounterpointInterval *interval) {
    StandardCounterpointIntervalCategory category;
    return standard_counterpoint_classify_interval(interval, &category) &&
           category != STANDARD_COUNTERPOINT_DISSONANCE;
}

static bool check_downbeat_consonant(const Exercise *exercise) {
    for (size_t bar = 1; bar + 1 < exercise->cantus->count; bar++) {
        StandardCounterpointInterval interval;
        if (!standard_counterpoint_fourth_species_bar_is_broken(exercise, bar)) {
            continue;
        }
        if (!standard_counterpoint_fourth_species_downbeat_interval(exercise, bar, &interval) ||
            !consonant_at(&interval)) {
            return false;
        }
    }
    return true;
}

static bool step_between(const PitchSpelling *a, const PitchSpelling *b) {
    StandardCounterpointInterval interval;
    return standard_counterpoint_interval_between(a, b, &interval) && standard_counterpoint_interval_is_step(&interval) &&
           standard_counterpoint_interval_melodic_consonant(&interval);
}

static bool check_weak_passing(const Exercise *exercise) {
    for (size_t bar = 1; bar + 1 < exercise->cantus->count; bar++) {
        StandardCounterpointInterval weak;
        if (!standard_counterpoint_fourth_species_bar_is_broken(exercise, bar)) {
            continue;
        }
        if (!standard_counterpoint_fourth_species_weak_interval(exercise, bar, &weak)) {
            return false;
        }
        if (consonant_at(&weak)) {
            continue;
        }
        const PitchSpelling *from = standard_counterpoint_fourth_species_downbeat(exercise, bar);
        const PitchSpelling *through = standard_counterpoint_fourth_species_weak(exercise, bar);
        const PitchSpelling *to = standard_counterpoint_fourth_species_downbeat(exercise, bar + 1);
        StandardCounterpointInterval next;
        if (!standard_counterpoint_fourth_species_downbeat_interval(exercise, bar + 1, &next) || !consonant_at(&next) ||
            !step_between(from, through) || !step_between(through, to)) {
            return false;
        }
        int out = standard_counterpoint_direction(from, through);
        int back = standard_counterpoint_direction(through, to);
        if (out == 0 || out != back) {
            return false;
        }
    }
    return true;
}

static bool check_limits(const Exercise *exercise) {
    size_t stretches = 0;
    size_t length = 0;
    for (size_t bar = 1; bar + 1 < exercise->cantus->count; bar++) {
        if (standard_counterpoint_fourth_species_bar_is_broken(exercise, bar)) {
            if (length == 0) {
                stretches++;
            }
            length++;
            if (length > STANDARD_COUNTERPOINT_SPECIES_BREAK_MAX_LENGTH) {
                return false;
            }
        } else {
            length = 0;
        }
    }
    return stretches <= STANDARD_COUNTERPOINT_SPECIES_BREAK_MAX_STRETCHES;
}

bool standard_counterpoint_species_break_rule_holds(StandardCounterpointSpeciesBreakRule which,
                                                    const StandardCounterpointFourthSpeciesExercise *exercise) {
    if (!valid_rule(which) || !standard_counterpoint_fourth_species_exercise_valid(exercise)) {
        return false;
    }
    switch (which) {
        case STANDARD_COUNTERPOINT_SPECIES_BREAK_DOWNBEAT_CONSONANT:
            return check_downbeat_consonant(exercise);
        case STANDARD_COUNTERPOINT_SPECIES_BREAK_WEAK_PASSING:
            return check_weak_passing(exercise);
        case STANDARD_COUNTERPOINT_SPECIES_BREAK_LIMITS:
            return check_limits(exercise);
        default:
            return false;
    }
}

static bool holds_adapter(int which, const void *subject) {
    return standard_counterpoint_species_break_rule_holds((StandardCounterpointSpeciesBreakRule)which, subject);
}

STANDARD_COUNTERPOINT_RULE_TABLE_EVALUATOR(evaluate_candidate, TABLE, holds_adapter, StandardCounterpointFourthSpeciesExercise)
STANDARD_COUNTERPOINT_RULE_TABLE_SCORER(score_candidate, TABLE, holds_adapter, StandardCounterpointFourthSpeciesExercise)

bool standard_counterpoint_species_break_check(const StandardCounterpointFourthSpeciesExercise *exercise,
                                               unsigned *hard_violations, unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL ||
        !standard_counterpoint_fourth_species_exercise_valid(exercise)) {
        return false;
    }
    standard_counterpoint_rule_table_check(&TABLE, holds_adapter, exercise, hard_violations,
                                           soft_violations);
    return true;
}
