#include "fifth_species_harmony_rules.h"

#include <stddef.h>

#include "consonance_rules.h"
#include "counterpoint_interval.h"
#include "fifth_species_figure.h"
#include "motion_rules.h"
#include "motion_type.h"
#include "rule_table.h"

static const char *const NAMES[STANDARD_COUNTERPOINT_FIFTH_SPECIES_HARMONY_RULE_COUNT] = {
    "downbeat-consonant", "dissonance-figure", "no-repeated-perfect", "no-direct-perfect", "max-same-imperfect-run",
    "no-interior-unison"};

static bool rule_is_hard(StandardCounterpointFifthSpeciesHarmonyRule which) {
    return which != STANDARD_COUNTERPOINT_FIFTH_SPECIES_HARMONY_NO_INTERIOR_UNISON;
}

static bool table_is_hard(int which) {
    return rule_is_hard((StandardCounterpointFifthSpeciesHarmonyRule)which);
}

static StandardCounterpointRulePair g_pairs[STANDARD_COUNTERPOINT_FIFTH_SPECIES_HARMONY_RULE_COUNT];
static bool g_built[STANDARD_COUNTERPOINT_FIFTH_SPECIES_HARMONY_RULE_COUNT];
static const StandardCounterpointRuleTable TABLE = {
    "fifth-species-harmony", NAMES, STANDARD_COUNTERPOINT_FIFTH_SPECIES_HARMONY_RULE_COUNT, table_is_hard,
    g_pairs, g_built};

static bool valid_rule(StandardCounterpointFifthSpeciesHarmonyRule which) {
    return standard_counterpoint_rule_table_valid(&TABLE, (int)which);
}

const Rule *standard_counterpoint_fifth_species_harmony_rule(StandardCounterpointFifthSpeciesHarmonyRule which) {
    return standard_counterpoint_rule_table_rule(&TABLE, (int)which);
}

const Constraint *standard_counterpoint_fifth_species_harmony_constraint(StandardCounterpointFifthSpeciesHarmonyRule which) {
    return standard_counterpoint_rule_table_constraint(&TABLE, (int)which);
}

const char *standard_counterpoint_fifth_species_harmony_rule_name(StandardCounterpointFifthSpeciesHarmonyRule which) {
    return standard_counterpoint_rule_table_name(&TABLE, (int)which);
}

typedef StandardCounterpointFifthSpeciesExercise Exercise;

#define SLOTS STANDARD_COUNTERPOINT_FIFTH_SPECIES_SLOTS_PER_BAR

/* The interval of the downbeat of a bar; false when it is a rest. */
static bool downbeat_interval(const Exercise *exercise, size_t bar, StandardCounterpointInterval *out) {
    return standard_counterpoint_fifth_species_interval(exercise, SLOTS * bar, out);
}

/* 5 for a fifth, 8 for an octave or unison, 0 for anything else. */
static int perfect_class(const StandardCounterpointInterval *interval) {
    StandardCounterpointIntervalCategory category;
    if (!standard_counterpoint_classify_interval(interval, &category) ||
        category != STANDARD_COUNTERPOINT_PERFECT_CONSONANCE) {
        return 0;
    }
    return interval->simple_size == 5 ? 5 : 8;
}

static bool check_downbeat_consonant(const Exercise *exercise) {
    for (size_t bar = 0; bar < exercise->cantus->count; bar++) {
        StandardCounterpointInterval interval;
        StandardCounterpointIntervalCategory category;
        if (!standard_counterpoint_fifth_species_slot_is_attack(exercise, SLOTS * bar)) {
            continue;
        }
        if (!downbeat_interval(exercise, bar, &interval) ||
            !standard_counterpoint_classify_interval(&interval, &category) ||
            category == STANDARD_COUNTERPOINT_DISSONANCE) {
            return false;
        }
    }
    return true;
}

static bool check_dissonance_figure(const Exercise *exercise) {
    for (size_t slot = 0; slot < exercise->counterpoint->count; slot++) {
        bool entry = standard_counterpoint_fifth_species_slot_is_attack(exercise, slot) ||
                     (standard_counterpoint_fifth_species_slot_is_tie(exercise, slot) && slot % SLOTS == 0);
        if (!entry || !standard_counterpoint_fifth_species_slot_is_dissonant(exercise, slot)) {
            continue;
        }
        StandardCounterpointFifthSpeciesFigure figure;
        if (!standard_counterpoint_fifth_species_dissonance_figure(exercise, slot, &figure)) {
            return false;
        }
    }
    return true;
}

static bool check_no_repeated_perfect(const Exercise *exercise) {
    for (size_t bar = 0; bar + 1 < exercise->cantus->count; bar++) {
        StandardCounterpointInterval first, second;
        if (!downbeat_interval(exercise, bar, &first) || !downbeat_interval(exercise, bar + 1, &second)) {
            continue;
        }
        int a = perfect_class(&first);
        if (a != 0 && a == perfect_class(&second)) {
            return false;
        }
    }
    return true;
}

static bool check_no_direct_perfect(const Exercise *exercise) {
    bool cantus_lower = exercise->cantus_part == STANDARD_COUNTERPOINT_CANTUS_IN_LOWER;
    for (size_t bar = 1; bar < exercise->cantus->count; bar++) {
        StandardCounterpointInterval interval;
        if (!downbeat_interval(exercise, bar, &interval) || perfect_class(&interval) == 0) {
            continue;
        }
        const PitchSpelling *before = standard_counterpoint_fifth_species_pitch(exercise, SLOTS * bar - 1);
        const PitchSpelling *after = standard_counterpoint_fifth_species_pitch(exercise, SLOTS * bar);
        if (before == NULL) {
            continue;
        }
        const PitchSpelling *cantus_from = exercise->cantus->notes[bar - 1];
        const PitchSpelling *cantus_to = exercise->cantus->notes[bar];
        MotionType motion;
        bool classified = cantus_lower
                              ? standard_counterpoint_classify_motion(cantus_from, cantus_to, before, after, &motion)
                              : standard_counterpoint_classify_motion(before, after, cantus_from, cantus_to, &motion);
        if (!classified || motion_type_is_same_direction(motion)) {
            return false;
        }
    }
    return true;
}

static bool check_max_same_imperfect_run(const Exercise *exercise) {
    size_t run = 0;
    int previous_size = 0;
    for (size_t bar = 0; bar < exercise->cantus->count; bar++) {
        StandardCounterpointInterval interval;
        StandardCounterpointIntervalCategory category;
        if (!downbeat_interval(exercise, bar, &interval) ||
            !standard_counterpoint_classify_interval(&interval, &category) ||
            category != STANDARD_COUNTERPOINT_IMPERFECT_CONSONANCE) {
            run = 0;
            continue;
        }
        run = (run > 0 && interval.simple_size == previous_size) ? run + 1 : 1;
        previous_size = interval.simple_size;
        if (run > STANDARD_COUNTERPOINT_FIFTH_SPECIES_HARMONY_MAX_IMPERFECT_RUN) {
            return false;
        }
    }
    return true;
}

static bool check_no_interior_unison(const Exercise *exercise) {
    for (size_t bar = 1; bar + 1 < exercise->cantus->count; bar++) {
        StandardCounterpointInterval interval;
        if (downbeat_interval(exercise, bar, &interval) && interval.size == 1) {
            return false;
        }
    }
    return true;
}

bool standard_counterpoint_fifth_species_harmony_rule_holds(
    StandardCounterpointFifthSpeciesHarmonyRule which, const StandardCounterpointFifthSpeciesExercise *exercise) {
    if (!valid_rule(which) || !standard_counterpoint_fifth_species_exercise_valid(exercise)) {
        return false;
    }
    switch (which) {
        case STANDARD_COUNTERPOINT_FIFTH_SPECIES_HARMONY_DOWNBEAT_CONSONANT:
            return check_downbeat_consonant(exercise);
        case STANDARD_COUNTERPOINT_FIFTH_SPECIES_HARMONY_DISSONANCE_FIGURE:
            return check_dissonance_figure(exercise);
        case STANDARD_COUNTERPOINT_FIFTH_SPECIES_HARMONY_NO_REPEATED_PERFECT:
            return check_no_repeated_perfect(exercise);
        case STANDARD_COUNTERPOINT_FIFTH_SPECIES_HARMONY_NO_DIRECT_PERFECT:
            return check_no_direct_perfect(exercise);
        case STANDARD_COUNTERPOINT_FIFTH_SPECIES_HARMONY_MAX_SAME_IMPERFECT_RUN:
            return check_max_same_imperfect_run(exercise);
        case STANDARD_COUNTERPOINT_FIFTH_SPECIES_HARMONY_NO_INTERIOR_UNISON:
            return check_no_interior_unison(exercise);
        default:
            return false;
    }
}

static bool holds_adapter(int which, const void *subject) {
    return standard_counterpoint_fifth_species_harmony_rule_holds((StandardCounterpointFifthSpeciesHarmonyRule)which,
                                                                  subject);
}

bool standard_counterpoint_fifth_species_harmony_check(const StandardCounterpointFifthSpeciesExercise *exercise,
                                                           unsigned *hard_violations, unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL ||
        !standard_counterpoint_fifth_species_exercise_valid(exercise)) {
        return false;
    }
    standard_counterpoint_rule_table_check(&TABLE, holds_adapter, exercise, hard_violations,
                                           soft_violations);
    return true;
}
