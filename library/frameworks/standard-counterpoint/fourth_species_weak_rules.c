#include "fourth_species_weak_rules.h"

#include <stddef.h>

#include "consonance_rules.h"
#include "counterpoint_interval.h"
#include "motion_rules.h"
#include "rule_table.h"

static const char *const NAMES[STANDARD_COUNTERPOINT_FOURTH_SPECIES_WEAK_COUNT] = {
    "consonant", "no-repeated-perfect", "max-same-imperfect-run", "no-direct-perfect", "no-interior-unison"};

static bool rule_is_soft(StandardCounterpointFourthSpeciesWeakRule which) {
    return which == STANDARD_COUNTERPOINT_FOURTH_SPECIES_WEAK_NO_INTERIOR_UNISON;
}

static bool table_is_hard(int which) {
    return !rule_is_soft((StandardCounterpointFourthSpeciesWeakRule)which);
}

static StandardCounterpointRulePair g_pairs[STANDARD_COUNTERPOINT_FOURTH_SPECIES_WEAK_COUNT];
static UrsatzOnce g_built[STANDARD_COUNTERPOINT_FOURTH_SPECIES_WEAK_COUNT];
static ConstraintEvalResult evaluate_candidate(const Constraint *constraint, const Value *candidate,
                                               const Context *context);
static PreferenceScore score_candidate(const Preference *preference, const Value *candidate, const Context *context);
static const StandardCounterpointRuleTable TABLE = {
    "fourth-species-weak", NAMES, STANDARD_COUNTERPOINT_FOURTH_SPECIES_WEAK_COUNT, table_is_hard, g_pairs, g_built,
    evaluate_candidate, score_candidate};

static bool valid_rule(StandardCounterpointFourthSpeciesWeakRule which) {
    return standard_counterpoint_rule_table_valid(&TABLE, (int)which);
}

const Rule *standard_counterpoint_fourth_species_weak_rule(StandardCounterpointFourthSpeciesWeakRule which) {
    return standard_counterpoint_rule_table_rule(&TABLE, (int)which);
}

const Constraint *standard_counterpoint_fourth_species_weak_constraint(StandardCounterpointFourthSpeciesWeakRule which) {
    return standard_counterpoint_rule_table_constraint(&TABLE, (int)which);
}

const Preference *standard_counterpoint_fourth_species_weak_preference(StandardCounterpointFourthSpeciesWeakRule which) {
    return standard_counterpoint_rule_table_preference(&TABLE, (int)which);
}

const char *standard_counterpoint_fourth_species_weak_rule_name(StandardCounterpointFourthSpeciesWeakRule which) {
    return standard_counterpoint_rule_table_name(&TABLE, (int)which);
}

typedef StandardCounterpointFourthSpeciesExercise Exercise;

/* The number of weak-beat bars: the first to the penultimate. */
static size_t weak_bars(const Exercise *exercise) {
    return exercise->cantus->count - 1;
}

static bool weak_category(const Exercise *exercise, size_t bar, StandardCounterpointInterval *interval,
                          StandardCounterpointIntervalCategory *category) {
    return standard_counterpoint_fourth_species_weak_interval(exercise, bar, interval) &&
           standard_counterpoint_classify_interval(interval, category);
}

/* The perfect interval a weak beat makes, with a unison read as an octave:
   5 for a fifth, 8 for an octave or unison, 0 for anything else. */
static int perfect_class(const StandardCounterpointInterval *interval) {
    StandardCounterpointIntervalCategory category;
    if (!standard_counterpoint_classify_interval(interval, &category) ||
        category != STANDARD_COUNTERPOINT_PERFECT_CONSONANCE) {
        return 0;
    }
    return interval->simple_size == 5 ? 5 : 8;
}

static bool check_consonant(const Exercise *exercise) {
    for (size_t bar = 0; bar < weak_bars(exercise); bar++) {
        if (standard_counterpoint_fourth_species_bar_is_broken(exercise, bar)) {
            continue;
        }
        StandardCounterpointInterval interval;
        StandardCounterpointIntervalCategory category;
        if (!weak_category(exercise, bar, &interval, &category) || category == STANDARD_COUNTERPOINT_DISSONANCE) {
            return false;
        }
    }
    return true;
}

static bool check_no_repeated_perfect(const Exercise *exercise) {
    for (size_t bar = 0; bar + 1 < weak_bars(exercise); bar++) {
        StandardCounterpointInterval first, second;
        if (!standard_counterpoint_fourth_species_weak_interval(exercise, bar, &first) ||
            !standard_counterpoint_fourth_species_weak_interval(exercise, bar + 1, &second)) {
            return false;
        }
        int a = perfect_class(&first);
        if (a != 0 && a == perfect_class(&second)) {
            return false;
        }
    }
    return true;
}

static bool check_max_same_imperfect_run(const Exercise *exercise) {
    size_t run = 0;
    int previous_size = 0;
    for (size_t bar = 0; bar < weak_bars(exercise); bar++) {
        StandardCounterpointInterval interval;
        StandardCounterpointIntervalCategory category;
        if (!weak_category(exercise, bar, &interval, &category)) {
            return false;
        }
        if (category != STANDARD_COUNTERPOINT_IMPERFECT_CONSONANCE) {
            run = 0;
            continue;
        }
        run = (run > 0 && interval.simple_size == previous_size) ? run + 1 : 1;
        previous_size = interval.simple_size;
        if (run > STANDARD_COUNTERPOINT_FOURTH_SPECIES_MAX_IMPERFECT_RUN) {
            return false;
        }
    }
    return true;
}

static bool check_no_direct_perfect(const Exercise *exercise) {
    bool cantus_lower = exercise->cantus_part == STANDARD_COUNTERPOINT_CANTUS_IN_LOWER;
    for (size_t bar = 0; bar + 1 < weak_bars(exercise); bar++) {
        StandardCounterpointInterval next;
        if (!standard_counterpoint_fourth_species_weak_interval(exercise, bar + 1, &next)) {
            return false;
        }
        if (perfect_class(&next) == 0) {
            continue;
        }
        const PitchSpelling *cantus_from = exercise->cantus->notes[bar];
        const PitchSpelling *cantus_to = exercise->cantus->notes[bar + 1];
        const PitchSpelling *weak_from = standard_counterpoint_fourth_species_weak(exercise, bar);
        const PitchSpelling *weak_to = standard_counterpoint_fourth_species_weak(exercise, bar + 1);
        MotionType motion;
        bool classified = cantus_lower ? standard_counterpoint_classify_motion(cantus_from, cantus_to, weak_from, weak_to, &motion)
                                       : standard_counterpoint_classify_motion(weak_from, weak_to, cantus_from, cantus_to, &motion);
        if (!classified) {
            return false;
        }
        if (motion_type_is_same_direction(motion)) {
            return false;
        }
    }
    return true;
}

static bool check_no_interior_unison(const Exercise *exercise) {
    for (size_t bar = 1; bar + 1 < weak_bars(exercise); bar++) {
        StandardCounterpointInterval interval;
        if (!standard_counterpoint_fourth_species_weak_interval(exercise, bar, &interval) || interval.size == 1) {
            return false;
        }
    }
    return true;
}

bool standard_counterpoint_fourth_species_weak_rule_holds(StandardCounterpointFourthSpeciesWeakRule which,
                                                          const StandardCounterpointFourthSpeciesExercise *exercise) {
    if (!valid_rule(which) || !standard_counterpoint_fourth_species_exercise_valid(exercise)) {
        return false;
    }
    switch (which) {
        case STANDARD_COUNTERPOINT_FOURTH_SPECIES_WEAK_CONSONANT:
            return check_consonant(exercise);
        case STANDARD_COUNTERPOINT_FOURTH_SPECIES_WEAK_NO_REPEATED_PERFECT:
            return check_no_repeated_perfect(exercise);
        case STANDARD_COUNTERPOINT_FOURTH_SPECIES_WEAK_MAX_SAME_IMPERFECT_RUN:
            return check_max_same_imperfect_run(exercise);
        case STANDARD_COUNTERPOINT_FOURTH_SPECIES_WEAK_NO_DIRECT_PERFECT:
            return check_no_direct_perfect(exercise);
        case STANDARD_COUNTERPOINT_FOURTH_SPECIES_WEAK_NO_INTERIOR_UNISON:
            return check_no_interior_unison(exercise);
        default:
            return false;
    }
}

static bool holds_adapter(int which, const void *subject) {
    return standard_counterpoint_fourth_species_weak_rule_holds((StandardCounterpointFourthSpeciesWeakRule)which,
                                                                subject);
}

STANDARD_COUNTERPOINT_RULE_TABLE_EVALUATOR(evaluate_candidate, TABLE, holds_adapter, StandardCounterpointFourthSpeciesExercise)
STANDARD_COUNTERPOINT_RULE_TABLE_SCORER(score_candidate, TABLE, holds_adapter, StandardCounterpointFourthSpeciesExercise)

bool standard_counterpoint_fourth_species_weak_check(const StandardCounterpointFourthSpeciesExercise *exercise,
                                                     unsigned *hard_violations, unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL ||
        !standard_counterpoint_fourth_species_exercise_valid(exercise)) {
        return false;
    }
    standard_counterpoint_rule_table_check(&TABLE, holds_adapter, exercise, hard_violations,
                                           soft_violations);
    return true;
}
