#include "third_species_beat_rules.h"

#include <stddef.h>

#include "counterpoint_interval.h"
#include "counterpoint_pitch.h"
#include "rule_table.h"
#include "third_species_figure.h"

#define BUFFER_SIZE (STANDARD_COUNTERPOINT_THIRD_SPECIES_SLOTS_PER_BAR * STANDARD_COUNTERPOINT_THIRD_SPECIES_MAX_BARS)

static const char *const NAMES[STANDARD_COUNTERPOINT_THIRD_SPECIES_BEAT_COUNT] = {"dissonance-figure", "mixture",
                                                                                   "large-leap-recovery"};

static bool rule_is_hard(StandardCounterpointThirdSpeciesBeatRule which) {
    return which == STANDARD_COUNTERPOINT_THIRD_SPECIES_BEAT_DISSONANCE_FIGURE;
}

static bool table_is_hard(int which) {
    return rule_is_hard((StandardCounterpointThirdSpeciesBeatRule)which);
}

static StandardCounterpointRulePair g_pairs[STANDARD_COUNTERPOINT_THIRD_SPECIES_BEAT_COUNT];
static UrsatzOnce g_built[STANDARD_COUNTERPOINT_THIRD_SPECIES_BEAT_COUNT];
static ConstraintEvalResult evaluate_candidate(const Constraint *constraint, const Value *candidate,
                                               const Context *context);
static PreferenceScore score_candidate(const Preference *preference, const Value *candidate, const Context *context);
static const StandardCounterpointRuleTable TABLE = {
    "third-species-beat", NAMES, STANDARD_COUNTERPOINT_THIRD_SPECIES_BEAT_COUNT, table_is_hard, g_pairs, g_built,
    evaluate_candidate, score_candidate};

static bool valid_rule(StandardCounterpointThirdSpeciesBeatRule which) {
    return standard_counterpoint_rule_table_valid(&TABLE, (int)which);
}

const Rule *standard_counterpoint_third_species_beat_rule(StandardCounterpointThirdSpeciesBeatRule which) {
    return standard_counterpoint_rule_table_rule(&TABLE, (int)which);
}

const Constraint *standard_counterpoint_third_species_beat_constraint(StandardCounterpointThirdSpeciesBeatRule which) {
    return standard_counterpoint_rule_table_constraint(&TABLE, (int)which);
}

const Preference *standard_counterpoint_third_species_beat_preference(StandardCounterpointThirdSpeciesBeatRule which) {
    return standard_counterpoint_rule_table_preference(&TABLE, (int)which);
}

const char *standard_counterpoint_third_species_beat_rule_name(StandardCounterpointThirdSpeciesBeatRule which) {
    return standard_counterpoint_rule_table_name(&TABLE, (int)which);
}

typedef StandardCounterpointThirdSpeciesExercise Exercise;

static bool check_dissonance_figure(const Exercise *exercise) {
    for (size_t bar = 0; bar < exercise->cantus->count; bar++) {
        for (size_t beat = 1; beat < STANDARD_COUNTERPOINT_THIRD_SPECIES_SLOTS_PER_BAR; beat++) {
            if (!standard_counterpoint_third_species_beat_is_attack(exercise, bar, beat) ||
                !standard_counterpoint_third_species_beat_is_dissonant(exercise, bar, beat)) {
                continue;
            }
            StandardCounterpointDissonanceFigure figure;
            if (!standard_counterpoint_third_species_dissonance_figure(exercise, bar, beat, &figure)) {
                return false;
            }
        }
    }
    return true;
}

static bool check_mixture(const Exercise *exercise) {
    if (exercise->cantus->count < STANDARD_COUNTERPOINT_THIRD_SPECIES_MIN_BARS_FOR_MIXTURE) {
        return true;
    }
    size_t dissonant = 0, consonant = 0;
    for (size_t bar = 0; bar < exercise->cantus->count; bar++) {
        for (size_t beat = 1; beat < STANDARD_COUNTERPOINT_THIRD_SPECIES_SLOTS_PER_BAR; beat++) {
            if (!standard_counterpoint_third_species_beat_is_attack(exercise, bar, beat)) {
                continue;
            }
            if (standard_counterpoint_third_species_beat_is_dissonant(exercise, bar, beat)) {
                dissonant++;
            } else {
                consonant++;
            }
        }
    }
    return dissonant > 0 && consonant > 0;
}

static bool check_large_leap_recovery(const Exercise *exercise) {
    const PitchSpelling *notes[BUFFER_SIZE];
    size_t slots[BUFFER_SIZE];
    size_t count;
    if (!standard_counterpoint_third_species_attacks(exercise, notes, slots, BUFFER_SIZE, &count)) {
        return false;
    }
    for (size_t j = 1; j + 1 < count; j++) {
        StandardCounterpointInterval leap, next;
        if (!standard_counterpoint_interval_between(notes[j - 1], notes[j], &leap) ||
            !standard_counterpoint_interval_between(notes[j], notes[j + 1], &next)) {
            return false;
        }
        if (leap.size < STANDARD_COUNTERPOINT_THIRD_SPECIES_LARGE_LEAP) {
            continue;
        }
        int out = standard_counterpoint_direction(notes[j - 1], notes[j]);
        int back = standard_counterpoint_direction(notes[j], notes[j + 1]);
        if (!standard_counterpoint_interval_is_step(&next) || out == 0 || back == 0 || out == back) {
            return false;
        }
    }
    return true;
}

bool standard_counterpoint_third_species_beat_rule_holds(StandardCounterpointThirdSpeciesBeatRule which,
                                                         const StandardCounterpointThirdSpeciesExercise *exercise) {
    if (!valid_rule(which) || !standard_counterpoint_third_species_exercise_valid(exercise)) {
        return false;
    }
    switch (which) {
        case STANDARD_COUNTERPOINT_THIRD_SPECIES_BEAT_DISSONANCE_FIGURE:
            return check_dissonance_figure(exercise);
        case STANDARD_COUNTERPOINT_THIRD_SPECIES_BEAT_MIXTURE:
            return check_mixture(exercise);
        case STANDARD_COUNTERPOINT_THIRD_SPECIES_BEAT_LARGE_LEAP_RECOVERY:
            return check_large_leap_recovery(exercise);
        default:
            return false;
    }
}

static bool holds_adapter(int which, const void *subject) {
    return standard_counterpoint_third_species_beat_rule_holds((StandardCounterpointThirdSpeciesBeatRule)which,
                                                               subject);
}

STANDARD_COUNTERPOINT_RULE_TABLE_EVALUATOR(evaluate_candidate, TABLE, holds_adapter, StandardCounterpointThirdSpeciesExercise)
STANDARD_COUNTERPOINT_RULE_TABLE_SCORER(score_candidate, TABLE, holds_adapter, StandardCounterpointThirdSpeciesExercise)

bool standard_counterpoint_third_species_beat_check(const StandardCounterpointThirdSpeciesExercise *exercise,
                                                    unsigned *hard_violations, unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL ||
        !standard_counterpoint_third_species_exercise_valid(exercise)) {
        return false;
    }
    standard_counterpoint_rule_table_check(&TABLE, holds_adapter, exercise, hard_violations,
                                           soft_violations);
    return true;
}
