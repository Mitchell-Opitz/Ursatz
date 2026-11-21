#include "theory/counterpoint/third_species_beat_rules.h"

#include <stddef.h>

#include "theory/tonality/interval.h"
#include "model/pitch/pitch_arithmetic.h"
#include "theory/rules/rule_table.h"
#include "theory/counterpoint/third_species_figure.h"

#define BUFFER_SIZE (STANDARD_COUNTERPOINT_THIRD_SPECIES_SLOTS_PER_BAR * STANDARD_COUNTERPOINT_THIRD_SPECIES_MAX_BARS)

static const char *const NAMES[STANDARD_COUNTERPOINT_THIRD_SPECIES_BEAT_COUNT] = {"dissonance-figure", "mixture",
                                                                                   "large-leap-recovery"};

static const bool SOFT[STANDARD_COUNTERPOINT_THIRD_SPECIES_BEAT_COUNT] = {
    [STANDARD_COUNTERPOINT_THIRD_SPECIES_BEAT_MIXTURE] = true,
    [STANDARD_COUNTERPOINT_THIRD_SPECIES_BEAT_LARGE_LEAP_RECOVERY] = true};

static const RuleTable TABLE = {
    .name = "third-species-beat", .names = NAMES, .soft = SOFT,
    .count = STANDARD_COUNTERPOINT_THIRD_SPECIES_BEAT_COUNT};

static bool valid_rule(StandardCounterpointThirdSpeciesBeatRule which) {
    return rule_table_valid(&TABLE, (int)which);
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
        StandardTonalityInterval leap, next;
        if (!standard_tonality_interval_between(notes[j - 1], notes[j], &leap) ||
            !standard_tonality_interval_between(notes[j], notes[j + 1], &next)) {
            return false;
        }
        if (leap.size < STANDARD_COUNTERPOINT_THIRD_SPECIES_LARGE_LEAP) {
            continue;
        }
        int out = pitch_spelling_direction(notes[j - 1], notes[j]);
        int back = pitch_spelling_direction(notes[j], notes[j + 1]);
        if (!standard_tonality_interval_is_step(&next) || out == 0 || back == 0 || out == back) {
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


bool standard_counterpoint_third_species_beat_check(const StandardCounterpointThirdSpeciesExercise *exercise,
                                                    unsigned *hard_violations, unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL ||
        !standard_counterpoint_third_species_exercise_valid(exercise)) {
        return false;
    }
    rule_table_check(&TABLE, holds_adapter, exercise, hard_violations,
                                           soft_violations);
    return true;
}
