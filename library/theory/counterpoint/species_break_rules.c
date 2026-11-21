#include "theory/counterpoint/species_break_rules.h"

#include <stddef.h>

#include "theory/counterpoint/consonance_rules.h"
#include "theory/tonality/interval.h"
#include "model/pitch/pitch_arithmetic.h"
#include "theory/counterpoint/framework.h"
#include "theory/rules/rule_table.h"

static const char *const NAMES[STANDARD_COUNTERPOINT_SPECIES_BREAK_COUNT] = {"downbeat-consonant", "weak-passing",
                                                                              "limits"};

static bool rule_is_soft(StandardCounterpointSpeciesBreakRule which) {
    return which == STANDARD_COUNTERPOINT_SPECIES_BREAK_LIMITS;
}

static bool table_is_hard(int which) {
    return !rule_is_soft((StandardCounterpointSpeciesBreakRule)which);
}

static RulePair g_pairs[STANDARD_COUNTERPOINT_SPECIES_BREAK_COUNT];
static UrsatzOnce g_built[STANDARD_COUNTERPOINT_SPECIES_BREAK_COUNT];
static const RuleTable TABLE = {
    "standard-counterpoint", standard_counterpoint_framework, "standard-counterpoint",
    "species-break", NAMES, STANDARD_COUNTERPOINT_SPECIES_BREAK_COUNT, table_is_hard, g_pairs, g_built};

static bool valid_rule(StandardCounterpointSpeciesBreakRule which) {
    return rule_table_valid(&TABLE, (int)which);
}

typedef StandardCounterpointFourthSpeciesExercise Exercise;

static bool consonant_at(const StandardTonalityInterval *interval) {
    StandardTonalityIntervalCategory category;
    return standard_counterpoint_classify_interval(interval, &category) &&
           category != STANDARD_COUNTERPOINT_DISSONANCE;
}

static bool check_downbeat_consonant(const Exercise *exercise) {
    for (size_t bar = 1; bar + 1 < exercise->cantus->count; bar++) {
        StandardTonalityInterval interval;
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
    StandardTonalityInterval interval;
    return standard_tonality_interval_between(a, b, &interval) && standard_tonality_interval_is_step(&interval) &&
           standard_counterpoint_interval_melodic_consonant(&interval);
}

static bool check_weak_passing(const Exercise *exercise) {
    for (size_t bar = 1; bar + 1 < exercise->cantus->count; bar++) {
        StandardTonalityInterval weak;
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
        StandardTonalityInterval next;
        if (!standard_counterpoint_fourth_species_downbeat_interval(exercise, bar + 1, &next) || !consonant_at(&next) ||
            !step_between(from, through) || !step_between(through, to)) {
            return false;
        }
        int out = pitch_spelling_direction(from, through);
        int back = pitch_spelling_direction(through, to);
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


bool standard_counterpoint_species_break_check(const StandardCounterpointFourthSpeciesExercise *exercise,
                                               unsigned *hard_violations, unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL ||
        !standard_counterpoint_fourth_species_exercise_valid(exercise)) {
        return false;
    }
    rule_table_check(&TABLE, holds_adapter, exercise, hard_violations,
                                           soft_violations);
    return true;
}
