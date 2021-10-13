#include "species_break_rules.h"

#include <stddef.h>
#include <stdio.h>

#include "consonance_rules.h"
#include "counterpoint_interval.h"
#include "counterpoint_pitch.h"
#include "rule_pair.h"

static const char *const NAMES[STANDARD_COUNTERPOINT_SPECIES_BREAK_COUNT] = {"downbeat-consonant", "weak-passing",
                                                                              "limits"};

static bool rule_is_soft(StandardCounterpointSpeciesBreakRule which) {
    return which == STANDARD_COUNTERPOINT_SPECIES_BREAK_LIMITS;
}

static StandardCounterpointRulePair g_pairs[STANDARD_COUNTERPOINT_SPECIES_BREAK_COUNT];
static bool g_built[STANDARD_COUNTERPOINT_SPECIES_BREAK_COUNT];

static bool valid_rule(StandardCounterpointSpeciesBreakRule which) {
    return which >= 0 && which < STANDARD_COUNTERPOINT_SPECIES_BREAK_COUNT;
}

static void ensure_built(StandardCounterpointSpeciesBreakRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    char name[96];
    snprintf(name, sizeof(name), "species-break-%s", NAMES[which]);
    standard_counterpoint_build_rule_pair(name, rule_is_soft(which) ? CONSTRAINT_TYPE_SOFT : CONSTRAINT_TYPE_HARD,
                                          &g_pairs[which]);
}

const Rule *standard_counterpoint_species_break_rule(StandardCounterpointSpeciesBreakRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_counterpoint_species_break_constraint(StandardCounterpointSpeciesBreakRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

const char *standard_counterpoint_species_break_rule_name(StandardCounterpointSpeciesBreakRule which) {
    return valid_rule(which) ? NAMES[which] : NULL;
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

bool standard_counterpoint_species_break_check(const StandardCounterpointFourthSpeciesExercise *exercise,
                                               unsigned *hard_violations, unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL ||
        !standard_counterpoint_fourth_species_exercise_valid(exercise)) {
        return false;
    }
    unsigned hard = 0;
    unsigned soft = 0;
    for (int i = 0; i < STANDARD_COUNTERPOINT_SPECIES_BREAK_COUNT; i++) {
        StandardCounterpointSpeciesBreakRule which = (StandardCounterpointSpeciesBreakRule)i;
        if (standard_counterpoint_species_break_rule_holds(which, exercise)) {
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
