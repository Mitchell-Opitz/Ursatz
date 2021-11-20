#include "fifth_species_suspension_rules.h"

#include <stddef.h>
#include <stdio.h>

#include "fifth_species_figure.h"
#include "rule_pair.h"

static const char *const NAMES[STANDARD_COUNTERPOINT_FIFTH_SPECIES_SUSPENSION_RULE_COUNT] = {"proper", "used",
                                                                                               "few-nine-eight"};

static bool rule_is_hard(StandardCounterpointFifthSpeciesSuspensionRule which) {
    return which == STANDARD_COUNTERPOINT_FIFTH_SPECIES_SUSPENSION_PROPER;
}

static StandardCounterpointRulePair g_pairs[STANDARD_COUNTERPOINT_FIFTH_SPECIES_SUSPENSION_RULE_COUNT];
static bool g_built[STANDARD_COUNTERPOINT_FIFTH_SPECIES_SUSPENSION_RULE_COUNT];

static bool valid_rule(StandardCounterpointFifthSpeciesSuspensionRule which) {
    return which >= 0 && which < STANDARD_COUNTERPOINT_FIFTH_SPECIES_SUSPENSION_RULE_COUNT;
}

static void ensure_built(StandardCounterpointFifthSpeciesSuspensionRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    char name[96];
    snprintf(name, sizeof(name), "fifth-species-suspension-%s", NAMES[which]);
    standard_counterpoint_build_rule_pair(name, rule_is_hard(which) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT,
                                          &g_pairs[which]);
}

const Rule *standard_counterpoint_fifth_species_suspension_rule(StandardCounterpointFifthSpeciesSuspensionRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *
standard_counterpoint_fifth_species_suspension_constraint(StandardCounterpointFifthSpeciesSuspensionRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

const char *
standard_counterpoint_fifth_species_suspension_rule_name(StandardCounterpointFifthSpeciesSuspensionRule which) {
    return valid_rule(which) ? NAMES[which] : NULL;
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

bool standard_counterpoint_fifth_species_suspension_check(const StandardCounterpointFifthSpeciesExercise *exercise,
                                                           unsigned *hard_violations, unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL ||
        !standard_counterpoint_fifth_species_exercise_valid(exercise)) {
        return false;
    }
    unsigned hard = 0;
    unsigned soft = 0;
    for (int i = 0; i < STANDARD_COUNTERPOINT_FIFTH_SPECIES_SUSPENSION_RULE_COUNT; i++) {
        StandardCounterpointFifthSpeciesSuspensionRule which = (StandardCounterpointFifthSpeciesSuspensionRule)i;
        if (standard_counterpoint_fifth_species_suspension_rule_holds(which, exercise)) {
            continue;
        }
        if (rule_is_hard(which)) {
            hard |= 1u << i;
        } else {
            soft |= 1u << i;
        }
    }
    *hard_violations = hard;
    *soft_violations = soft;
    return true;
}
