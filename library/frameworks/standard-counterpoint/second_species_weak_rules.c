#include "second_species_weak_rules.h"

#include <stddef.h>
#include <stdio.h>

#include "consonance_rules.h"
#include "rule_pair.h"
#include "second_species_pattern.h"

static const char *const NAMES[STANDARD_COUNTERPOINT_SECOND_SPECIES_WEAK_COUNT] = {
    "dissonant-passing", "pattern-fit", "mixture", "register-change-rare"};

static bool rule_is_hard(StandardCounterpointSecondSpeciesWeakRule which) {
    return which == STANDARD_COUNTERPOINT_SECOND_SPECIES_WEAK_DISSONANT_PASSING;
}

static StandardCounterpointRulePair g_pairs[STANDARD_COUNTERPOINT_SECOND_SPECIES_WEAK_COUNT];
static bool g_built[STANDARD_COUNTERPOINT_SECOND_SPECIES_WEAK_COUNT];

static bool valid_rule(StandardCounterpointSecondSpeciesWeakRule which) {
    return which >= 0 && which < STANDARD_COUNTERPOINT_SECOND_SPECIES_WEAK_COUNT;
}

static void ensure_built(StandardCounterpointSecondSpeciesWeakRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    char name[96];
    snprintf(name, sizeof(name), "second-species-weak-%s", NAMES[which]);
    standard_counterpoint_build_rule_pair(name, rule_is_hard(which) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT,
                                          &g_pairs[which]);
}

const Rule *standard_counterpoint_second_species_weak_rule(StandardCounterpointSecondSpeciesWeakRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *
standard_counterpoint_second_species_weak_constraint(StandardCounterpointSecondSpeciesWeakRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

const char *standard_counterpoint_second_species_weak_rule_name(StandardCounterpointSecondSpeciesWeakRule which) {
    return valid_rule(which) ? NAMES[which] : NULL;
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

bool standard_counterpoint_second_species_weak_check(const StandardCounterpointSecondSpeciesExercise *exercise,
                                                     unsigned *hard_violations, unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL ||
        !standard_counterpoint_second_species_exercise_valid(exercise)) {
        return false;
    }
    unsigned hard = 0;
    unsigned soft = 0;
    for (int i = 0; i < STANDARD_COUNTERPOINT_SECOND_SPECIES_WEAK_COUNT; i++) {
        StandardCounterpointSecondSpeciesWeakRule which = (StandardCounterpointSecondSpeciesWeakRule)i;
        if (standard_counterpoint_second_species_weak_rule_holds(which, exercise)) {
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
