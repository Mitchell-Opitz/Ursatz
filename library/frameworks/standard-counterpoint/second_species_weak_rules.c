#include "second_species_weak_rules.h"

#include <stddef.h>

#include "consonance_rules.h"
#include "rule_table.h"
#include "second_species_pattern.h"

static const char *const NAMES[STANDARD_COUNTERPOINT_SECOND_SPECIES_WEAK_COUNT] = {
    "dissonant-passing", "pattern-fit", "mixture", "register-change-rare"};

static bool rule_is_hard(StandardCounterpointSecondSpeciesWeakRule which) {
    return which == STANDARD_COUNTERPOINT_SECOND_SPECIES_WEAK_DISSONANT_PASSING;
}

static bool table_is_hard(int which) {
    return rule_is_hard((StandardCounterpointSecondSpeciesWeakRule)which);
}

static StandardCounterpointRulePair g_pairs[STANDARD_COUNTERPOINT_SECOND_SPECIES_WEAK_COUNT];
static bool g_built[STANDARD_COUNTERPOINT_SECOND_SPECIES_WEAK_COUNT];
static ConstraintEvalResult evaluate_candidate(const Constraint *constraint, const Value *candidate,
                                               const Context *context);
static PreferenceScore score_candidate(const Preference *preference, const Value *candidate, const Context *context);
static const StandardCounterpointRuleTable TABLE = {
    "second-species-weak", NAMES, STANDARD_COUNTERPOINT_SECOND_SPECIES_WEAK_COUNT, table_is_hard, g_pairs, g_built,
    evaluate_candidate, score_candidate};

static bool valid_rule(StandardCounterpointSecondSpeciesWeakRule which) {
    return standard_counterpoint_rule_table_valid(&TABLE, (int)which);
}

const Rule *standard_counterpoint_second_species_weak_rule(StandardCounterpointSecondSpeciesWeakRule which) {
    return standard_counterpoint_rule_table_rule(&TABLE, (int)which);
}

const Constraint *standard_counterpoint_second_species_weak_constraint(StandardCounterpointSecondSpeciesWeakRule which) {
    return standard_counterpoint_rule_table_constraint(&TABLE, (int)which);
}

const Preference *standard_counterpoint_second_species_weak_preference(StandardCounterpointSecondSpeciesWeakRule which) {
    return standard_counterpoint_rule_table_preference(&TABLE, (int)which);
}

const char *standard_counterpoint_second_species_weak_rule_name(StandardCounterpointSecondSpeciesWeakRule which) {
    return standard_counterpoint_rule_table_name(&TABLE, (int)which);
}

typedef StandardCounterpointSecondSpeciesExercise Exercise;

/* True when the bar's weak beat is a new note and a dissonance against the
   cantus firmus. */
static bool weak_is_dissonant(const Exercise *exercise, size_t bar, bool *dissonant) {
    StandardCounterpointInterval interval;
    StandardCounterpointIntervalCategory category;
    if (!standard_counterpoint_second_species_weak_interval(exercise, bar, &interval) ||
        !standard_counterpoint_classify_interval(&interval, &category)) {
        return false;
    }
    *dissonant = category == STANDARD_COUNTERPOINT_DISSONANCE;
    return true;
}

static bool check_dissonant_passing(const Exercise *exercise) {
    for (size_t bar = 0; bar < exercise->cantus->count; bar++) {
        if (!standard_counterpoint_second_species_weak_is_attack(exercise, bar)) {
            continue;
        }
        bool dissonant;
        if (!weak_is_dissonant(exercise, bar, &dissonant)) {
            return false;
        }
        if (!dissonant) {
            continue;
        }
        StandardCounterpointWeakBeatPattern pattern;
        if (!standard_counterpoint_second_species_weak_beat_pattern(exercise, bar, &pattern) ||
            !standard_counterpoint_weak_beat_pattern_is_dissonant(pattern)) {
            return false;
        }
    }
    return true;
}

static bool check_pattern_fit(const Exercise *exercise) {
    for (size_t bar = 0; bar + 1 < exercise->cantus->count; bar++) {
        if (!standard_counterpoint_second_species_weak_is_attack(exercise, bar) ||
            standard_counterpoint_second_species_downbeat(exercise, bar) == NULL) {
            continue;
        }
        bool dissonant;
        if (!weak_is_dissonant(exercise, bar, &dissonant)) {
            return false;
        }
        StandardCounterpointWeakBeatPattern pattern;
        if (!dissonant && !standard_counterpoint_second_species_weak_beat_pattern(exercise, bar, &pattern)) {
            return false;
        }
    }
    return true;
}

static bool check_mixture(const Exercise *exercise) {
    size_t attacks = 0, consonant = 0, dissonant_count = 0;
    for (size_t bar = 0; bar < exercise->cantus->count; bar++) {
        if (!standard_counterpoint_second_species_weak_is_attack(exercise, bar)) {
            continue;
        }
        bool dissonant;
        if (!weak_is_dissonant(exercise, bar, &dissonant)) {
            return false;
        }
        attacks++;
        if (dissonant) {
            dissonant_count++;
        } else {
            consonant++;
        }
    }
    if (attacks < STANDARD_COUNTERPOINT_SECOND_SPECIES_MIN_WEAK_FOR_MIXTURE) {
        return true;
    }
    return consonant > 0 && dissonant_count > 0;
}

static bool check_register_change_rare(const Exercise *exercise) {
    size_t changes = 0;
    for (size_t bar = 0; bar + 1 < exercise->cantus->count; bar++) {
        StandardCounterpointWeakBeatPattern pattern;
        if (standard_counterpoint_second_species_weak_beat_pattern(exercise, bar, &pattern) &&
            pattern == STANDARD_COUNTERPOINT_WEAK_BEAT_CHANGE_OF_REGISTER) {
            changes++;
        }
    }
    return changes <= STANDARD_COUNTERPOINT_SECOND_SPECIES_MAX_REGISTER_CHANGES;
}

bool standard_counterpoint_second_species_weak_rule_holds(StandardCounterpointSecondSpeciesWeakRule which,
                                                          const StandardCounterpointSecondSpeciesExercise *exercise) {
    if (!valid_rule(which) || !standard_counterpoint_second_species_exercise_valid(exercise)) {
        return false;
    }
    switch (which) {
        case STANDARD_COUNTERPOINT_SECOND_SPECIES_WEAK_DISSONANT_PASSING:
            return check_dissonant_passing(exercise);
        case STANDARD_COUNTERPOINT_SECOND_SPECIES_WEAK_PATTERN_FIT:
            return check_pattern_fit(exercise);
        case STANDARD_COUNTERPOINT_SECOND_SPECIES_WEAK_MIXTURE:
            return check_mixture(exercise);
        case STANDARD_COUNTERPOINT_SECOND_SPECIES_WEAK_REGISTER_CHANGE_RARE:
            return check_register_change_rare(exercise);
        default:
            return false;
    }
}

static bool holds_adapter(int which, const void *subject) {
    return standard_counterpoint_second_species_weak_rule_holds((StandardCounterpointSecondSpeciesWeakRule)which,
                                                                subject);
}

STANDARD_COUNTERPOINT_RULE_TABLE_EVALUATOR(evaluate_candidate, TABLE, holds_adapter, StandardCounterpointSecondSpeciesExercise)
STANDARD_COUNTERPOINT_RULE_TABLE_SCORER(score_candidate, TABLE, holds_adapter, StandardCounterpointSecondSpeciesExercise)

bool standard_counterpoint_second_species_weak_check(const StandardCounterpointSecondSpeciesExercise *exercise,
                                                     unsigned *hard_violations, unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL ||
        !standard_counterpoint_second_species_exercise_valid(exercise)) {
        return false;
    }
    standard_counterpoint_rule_table_check(&TABLE, holds_adapter, exercise, hard_violations,
                                           soft_violations);
    return true;
}
