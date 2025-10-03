#include "fourth_species_suspension_rules.h"

#include <stddef.h>

#include "consonance_rules.h"
#include "fourth_species_suspension.h"
#include "rule_table.h"

static const char *const NAMES[STANDARD_COUNTERPOINT_FOURTH_SPECIES_SUSPENSION_RULE_COUNT] = {"proper", "many",
                                                                                               "few-nine-eight"};

static bool rule_is_hard(StandardCounterpointFourthSpeciesSuspensionRule which) {
    return which == STANDARD_COUNTERPOINT_FOURTH_SPECIES_SUSPENSION_PROPER;
}

static bool table_is_hard(int which) {
    return rule_is_hard((StandardCounterpointFourthSpeciesSuspensionRule)which);
}

static StandardCounterpointRulePair g_pairs[STANDARD_COUNTERPOINT_FOURTH_SPECIES_SUSPENSION_RULE_COUNT];
static bool g_built[STANDARD_COUNTERPOINT_FOURTH_SPECIES_SUSPENSION_RULE_COUNT];
static ConstraintEvalResult evaluate_candidate(const Constraint *constraint, const Value *candidate,
                                               const Context *context);
static const StandardCounterpointRuleTable TABLE = {
    "fourth-species-suspension", NAMES, STANDARD_COUNTERPOINT_FOURTH_SPECIES_SUSPENSION_RULE_COUNT, table_is_hard,
    g_pairs, g_built, evaluate_candidate};

static bool valid_rule(StandardCounterpointFourthSpeciesSuspensionRule which) {
    return standard_counterpoint_rule_table_valid(&TABLE, (int)which);
}

const Rule *standard_counterpoint_fourth_species_suspension_rule(StandardCounterpointFourthSpeciesSuspensionRule which) {
    return standard_counterpoint_rule_table_rule(&TABLE, (int)which);
}

const Constraint *standard_counterpoint_fourth_species_suspension_constraint(StandardCounterpointFourthSpeciesSuspensionRule which) {
    return standard_counterpoint_rule_table_constraint(&TABLE, (int)which);
}

const char *standard_counterpoint_fourth_species_suspension_rule_name(StandardCounterpointFourthSpeciesSuspensionRule which) {
    return standard_counterpoint_rule_table_name(&TABLE, (int)which);
}

typedef StandardCounterpointFourthSpeciesExercise Exercise;

static bool check_proper(const Exercise *exercise) {
    for (size_t bar = 1; bar + 1 < exercise->cantus->count; bar++) {
        StandardCounterpointInterval interval;
        StandardCounterpointIntervalCategory category;
        if (!standard_counterpoint_fourth_species_downbeat_is_tie(exercise, bar)) {
            continue;
        }
        if (!standard_counterpoint_fourth_species_downbeat_interval(exercise, bar, &interval) ||
            !standard_counterpoint_classify_interval(&interval, &category)) {
            return false;
        }
        if (category != STANDARD_COUNTERPOINT_DISSONANCE) {
            continue;
        }
        StandardCounterpointSuspensionType type;
        if (!standard_counterpoint_fourth_species_suspension(exercise, bar, &type) ||
            !standard_counterpoint_suspension_is_dissonant(type)) {
            return false;
        }
    }
    return true;
}

/* Counts the tied middle bars, the dissonant suspensions among them, and the
   9-8 suspensions among those. */
static void count_suspensions(const Exercise *exercise, size_t *tied, size_t *dissonant, size_t *nine_eight) {
    *tied = *dissonant = *nine_eight = 0;
    for (size_t bar = 1; bar + 1 < exercise->cantus->count; bar++) {
        if (!standard_counterpoint_fourth_species_downbeat_is_tie(exercise, bar)) {
            continue;
        }
        (*tied)++;
        StandardCounterpointSuspensionType type;
        if (standard_counterpoint_fourth_species_suspension(exercise, bar, &type) &&
            standard_counterpoint_suspension_is_dissonant(type)) {
            (*dissonant)++;
            if (type == STANDARD_COUNTERPOINT_SUSPENSION_NINE_EIGHT) {
                (*nine_eight)++;
            }
        }
    }
}

static bool check_many(const Exercise *exercise) {
    size_t tied, dissonant, nine_eight;
    count_suspensions(exercise, &tied, &dissonant, &nine_eight);
    return 2 * dissonant >= tied;
}

static bool check_few_nine_eight(const Exercise *exercise) {
    size_t tied, dissonant, nine_eight;
    count_suspensions(exercise, &tied, &dissonant, &nine_eight);
    return 2 * nine_eight <= dissonant;
}

bool standard_counterpoint_fourth_species_suspension_rule_holds(
    StandardCounterpointFourthSpeciesSuspensionRule which, const StandardCounterpointFourthSpeciesExercise *exercise) {
    if (!valid_rule(which) || !standard_counterpoint_fourth_species_exercise_valid(exercise)) {
        return false;
    }
    switch (which) {
        case STANDARD_COUNTERPOINT_FOURTH_SPECIES_SUSPENSION_PROPER:
            return check_proper(exercise);
        case STANDARD_COUNTERPOINT_FOURTH_SPECIES_SUSPENSION_MANY:
            return check_many(exercise);
        case STANDARD_COUNTERPOINT_FOURTH_SPECIES_SUSPENSION_FEW_NINE_EIGHT:
            return check_few_nine_eight(exercise);
        default:
            return false;
    }
}

static bool holds_adapter(int which, const void *subject) {
    return standard_counterpoint_fourth_species_suspension_rule_holds(
        (StandardCounterpointFourthSpeciesSuspensionRule)which, subject);
}

STANDARD_COUNTERPOINT_RULE_TABLE_EVALUATOR(evaluate_candidate, TABLE, holds_adapter, StandardCounterpointFourthSpeciesExercise)

bool standard_counterpoint_fourth_species_suspension_check(const StandardCounterpointFourthSpeciesExercise *exercise,
                                                           unsigned *hard_violations, unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL ||
        !standard_counterpoint_fourth_species_exercise_valid(exercise)) {
        return false;
    }
    standard_counterpoint_rule_table_check(&TABLE, holds_adapter, exercise, hard_violations,
                                           soft_violations);
    return true;
}
