#include "fifth_species_suspension_rules.h"

#include <stddef.h>

#include "fifth_species_figure.h"
#include "rule_table.h"

static const char *const NAMES[STANDARD_COUNTERPOINT_FIFTH_SPECIES_SUSPENSION_RULE_COUNT] = {"proper", "used",
                                                                                               "few-nine-eight"};

static bool rule_is_hard(StandardCounterpointFifthSpeciesSuspensionRule which) {
    return which == STANDARD_COUNTERPOINT_FIFTH_SPECIES_SUSPENSION_PROPER;
}

static bool table_is_hard(int which) {
    return rule_is_hard((StandardCounterpointFifthSpeciesSuspensionRule)which);
}

static StandardCounterpointRulePair g_pairs[STANDARD_COUNTERPOINT_FIFTH_SPECIES_SUSPENSION_RULE_COUNT];
static UrsatzOnce g_built[STANDARD_COUNTERPOINT_FIFTH_SPECIES_SUSPENSION_RULE_COUNT];
static ConstraintEvalResult evaluate_candidate(const Constraint *constraint, const Value *candidate,
                                               const Context *context);
static PreferenceScore score_candidate(const Preference *preference, const Value *candidate, const Context *context);
static const StandardCounterpointRuleTable TABLE = {
    "fifth-species-suspension", NAMES, STANDARD_COUNTERPOINT_FIFTH_SPECIES_SUSPENSION_RULE_COUNT, table_is_hard,
    g_pairs, g_built, evaluate_candidate, score_candidate};

static bool valid_rule(StandardCounterpointFifthSpeciesSuspensionRule which) {
    return standard_counterpoint_rule_table_valid(&TABLE, (int)which);
}

const Rule *standard_counterpoint_fifth_species_suspension_rule(StandardCounterpointFifthSpeciesSuspensionRule which) {
    return standard_counterpoint_rule_table_rule(&TABLE, (int)which);
}

const Constraint *standard_counterpoint_fifth_species_suspension_constraint(StandardCounterpointFifthSpeciesSuspensionRule which) {
    return standard_counterpoint_rule_table_constraint(&TABLE, (int)which);
}

const Preference *standard_counterpoint_fifth_species_suspension_preference(StandardCounterpointFifthSpeciesSuspensionRule which) {
    return standard_counterpoint_rule_table_preference(&TABLE, (int)which);
}

const char *standard_counterpoint_fifth_species_suspension_rule_name(StandardCounterpointFifthSpeciesSuspensionRule which) {
    return standard_counterpoint_rule_table_name(&TABLE, (int)which);
}

typedef StandardCounterpointFifthSpeciesExercise Exercise;

static bool check_proper(const Exercise *exercise) {
    for (size_t bar = 1; bar < exercise->cantus->count; bar++) {
        size_t slot = STANDARD_COUNTERPOINT_FIFTH_SPECIES_SLOTS_PER_BAR * bar;
        if (!standard_counterpoint_fifth_species_slot_is_tie(exercise, slot) ||
            !standard_counterpoint_fifth_species_slot_is_dissonant(exercise, slot)) {
            continue;
        }
        StandardCounterpointSuspensionType type;
        if (!standard_counterpoint_fifth_species_suspension_type(exercise, slot, &type)) {
            return false;
        }
    }
    return true;
}

/* Counts the dissonant suspensions, and the 9-8 suspensions among them. */
static void count_suspensions(const Exercise *exercise, size_t *dissonant, size_t *nine_eight) {
    *dissonant = *nine_eight = 0;
    for (size_t bar = 1; bar < exercise->cantus->count; bar++) {
        size_t slot = STANDARD_COUNTERPOINT_FIFTH_SPECIES_SLOTS_PER_BAR * bar;
        StandardCounterpointSuspensionType type;
        if (standard_counterpoint_fifth_species_suspension_type(exercise, slot, &type)) {
            (*dissonant)++;
            if (type == STANDARD_COUNTERPOINT_SUSPENSION_NINE_EIGHT) {
                (*nine_eight)++;
            }
        }
    }
}

static bool check_used(const Exercise *exercise) {
    size_t dissonant, nine_eight;
    count_suspensions(exercise, &dissonant, &nine_eight);
    return exercise->cantus->count < STANDARD_COUNTERPOINT_FIFTH_SPECIES_SUSPENSION_MIN_BARS || dissonant > 0;
}

static bool check_few_nine_eight(const Exercise *exercise) {
    size_t dissonant, nine_eight;
    count_suspensions(exercise, &dissonant, &nine_eight);
    return 2 * nine_eight <= dissonant;
}

bool standard_counterpoint_fifth_species_suspension_rule_holds(
    StandardCounterpointFifthSpeciesSuspensionRule which, const StandardCounterpointFifthSpeciesExercise *exercise) {
    if (!valid_rule(which) || !standard_counterpoint_fifth_species_exercise_valid(exercise)) {
        return false;
    }
    switch (which) {
        case STANDARD_COUNTERPOINT_FIFTH_SPECIES_SUSPENSION_PROPER:
            return check_proper(exercise);
        case STANDARD_COUNTERPOINT_FIFTH_SPECIES_SUSPENSION_USED:
            return check_used(exercise);
        case STANDARD_COUNTERPOINT_FIFTH_SPECIES_SUSPENSION_FEW_NINE_EIGHT:
            return check_few_nine_eight(exercise);
        default:
            return false;
    }
}

static bool holds_adapter(int which, const void *subject) {
    return standard_counterpoint_fifth_species_suspension_rule_holds(
        (StandardCounterpointFifthSpeciesSuspensionRule)which, subject);
}

STANDARD_COUNTERPOINT_RULE_TABLE_EVALUATOR(evaluate_candidate, TABLE, holds_adapter, StandardCounterpointFifthSpeciesExercise)
STANDARD_COUNTERPOINT_RULE_TABLE_SCORER(score_candidate, TABLE, holds_adapter, StandardCounterpointFifthSpeciesExercise)

bool standard_counterpoint_fifth_species_suspension_check(const StandardCounterpointFifthSpeciesExercise *exercise,
                                                           unsigned *hard_violations, unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL ||
        !standard_counterpoint_fifth_species_exercise_valid(exercise)) {
        return false;
    }
    standard_counterpoint_rule_table_check(&TABLE, holds_adapter, exercise, hard_violations,
                                           soft_violations);
    return true;
}
