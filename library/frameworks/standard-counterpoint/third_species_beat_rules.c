#include "third_species_beat_rules.h"

#include <stddef.h>
#include <stdio.h>

#include "counterpoint_interval.h"
#include "counterpoint_pitch.h"
#include "rule_pair.h"
#include "third_species_figure.h"

#define BUFFER_SIZE (STANDARD_COUNTERPOINT_THIRD_SPECIES_SLOTS_PER_BAR * STANDARD_COUNTERPOINT_THIRD_SPECIES_MAX_BARS)

static const char *const NAMES[STANDARD_COUNTERPOINT_THIRD_SPECIES_BEAT_COUNT] = {"dissonance-figure", "mixture",
                                                                                   "large-leap-recovery"};

static bool rule_is_hard(StandardCounterpointThirdSpeciesBeatRule which) {
    return which == STANDARD_COUNTERPOINT_THIRD_SPECIES_BEAT_DISSONANCE_FIGURE;
}

static StandardCounterpointRulePair g_pairs[STANDARD_COUNTERPOINT_THIRD_SPECIES_BEAT_COUNT];
static bool g_built[STANDARD_COUNTERPOINT_THIRD_SPECIES_BEAT_COUNT];

static bool valid_rule(StandardCounterpointThirdSpeciesBeatRule which) {
    return which >= 0 && which < STANDARD_COUNTERPOINT_THIRD_SPECIES_BEAT_COUNT;
}

static void ensure_built(StandardCounterpointThirdSpeciesBeatRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    char name[96];
    snprintf(name, sizeof(name), "third-species-beat-%s", NAMES[which]);
    standard_counterpoint_build_rule_pair(name, rule_is_hard(which) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT,
                                          &g_pairs[which]);
}

const Rule *standard_counterpoint_third_species_beat_rule(StandardCounterpointThirdSpeciesBeatRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_counterpoint_third_species_beat_constraint(StandardCounterpointThirdSpeciesBeatRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

const char *standard_counterpoint_third_species_beat_rule_name(StandardCounterpointThirdSpeciesBeatRule which) {
    return valid_rule(which) ? NAMES[which] : NULL;
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

bool standard_counterpoint_third_species_beat_check(const StandardCounterpointThirdSpeciesExercise *exercise,
                                                    unsigned *hard_violations, unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL ||
        !standard_counterpoint_third_species_exercise_valid(exercise)) {
        return false;
    }
    unsigned hard = 0;
    unsigned soft = 0;
    for (int i = 0; i < STANDARD_COUNTERPOINT_THIRD_SPECIES_BEAT_COUNT; i++) {
        StandardCounterpointThirdSpeciesBeatRule which = (StandardCounterpointThirdSpeciesBeatRule)i;
        if (standard_counterpoint_third_species_beat_rule_holds(which, exercise)) {
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
