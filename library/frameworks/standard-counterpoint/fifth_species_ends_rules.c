#include "fifth_species_ends_rules.h"

#include <stddef.h>
#include <stdio.h>

#include "consonance_rules.h"
#include "counterpoint_interval.h"
#include "counterpoint_pitch.h"
#include "rule_pair.h"

#define SLOTS STANDARD_COUNTERPOINT_FIFTH_SPECIES_SLOTS_PER_BAR

static const char *const NAMES[STANDARD_COUNTERPOINT_FIFTH_SPECIES_ENDS_COUNT] = {"begin", "clausula-vera",
                                                                                    "final-bar"};

static StandardCounterpointRulePair g_pairs[STANDARD_COUNTERPOINT_FIFTH_SPECIES_ENDS_COUNT];
static bool g_built[STANDARD_COUNTERPOINT_FIFTH_SPECIES_ENDS_COUNT];

static bool valid_rule(StandardCounterpointFifthSpeciesEndsRule which) {
    return which >= 0 && which < STANDARD_COUNTERPOINT_FIFTH_SPECIES_ENDS_COUNT;
}

static void ensure_built(StandardCounterpointFifthSpeciesEndsRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    char name[96];
    snprintf(name, sizeof(name), "fifth-species-ends-%s", NAMES[which]);
    standard_counterpoint_build_rule_pair(name, CONSTRAINT_TYPE_HARD, &g_pairs[which]);
}

const Rule *standard_counterpoint_fifth_species_ends_rule(StandardCounterpointFifthSpeciesEndsRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_counterpoint_fifth_species_ends_constraint(StandardCounterpointFifthSpeciesEndsRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

const char *standard_counterpoint_fifth_species_ends_rule_name(StandardCounterpointFifthSpeciesEndsRule which) {
    return valid_rule(which) ? NAMES[which] : NULL;
}

typedef StandardCounterpointFifthSpeciesExercise Exercise;

static bool is_tonic(const PitchSpelling *note, const PitchSpelling *tonic) {
    return pitch_spelling_letter(note) == pitch_spelling_letter(tonic) &&
           pitch_spelling_accidental(note) == pitch_spelling_accidental(tonic);
}

static bool check_begin(const Exercise *exercise) {
    size_t slot = 0;
    while (standard_counterpoint_fifth_species_pitch(exercise, slot) == NULL) {
        slot++;
    }
    StandardCounterpointInterval interval;
    StandardCounterpointIntervalCategory category;
    return standard_counterpoint_fifth_species_interval(exercise, slot, &interval) &&
           standard_counterpoint_classify_interval(&interval, &category) &&
           category == STANDARD_COUNTERPOINT_PERFECT_CONSONANCE;
}

static bool check_final_bar(const Exercise *exercise, const PitchSpelling *tonic) {
    size_t final_slot = exercise->counterpoint->count - SLOTS;
    return is_tonic(standard_counterpoint_fifth_species_pitch(exercise, final_slot), tonic);
}

static bool check_clausula_vera(const Exercise *exercise) {
    size_t bars = exercise->cantus->count;
    size_t final_slot = exercise->counterpoint->count - SLOTS;
    const PitchSpelling *cantus_before = exercise->cantus->notes[bars - 2];
    const PitchSpelling *cantus_final = exercise->cantus->notes[bars - 1];
    const PitchSpelling *before = standard_counterpoint_fifth_species_pitch(exercise, final_slot - 1);
    const PitchSpelling *final = standard_counterpoint_fifth_species_pitch(exercise, final_slot);

    StandardCounterpointInterval cantus_move, counterpoint_move, vertical;
    if (!standard_counterpoint_interval_between(cantus_before, cantus_final, &cantus_move) ||
        !standard_counterpoint_interval_between(before, final, &counterpoint_move) ||
        !standard_counterpoint_interval_between(cantus_before, before, &vertical)) {
        return false;
    }
    int cantus_direction = standard_counterpoint_direction(cantus_before, cantus_final);
    int counterpoint_direction = standard_counterpoint_direction(before, final);
    bool major_or_minor = vertical.quality == STANDARD_COUNTERPOINT_QUALITY_MAJOR ||
                          vertical.quality == STANDARD_COUNTERPOINT_QUALITY_MINOR;
    return standard_counterpoint_interval_is_step(&cantus_move) &&
           standard_counterpoint_interval_is_step(&counterpoint_move) && cantus_direction != 0 &&
           counterpoint_direction != 0 && cantus_direction != counterpoint_direction && major_or_minor &&
           (vertical.simple_size == 3 || vertical.simple_size == 6);
}

bool standard_counterpoint_fifth_species_ends_rule_holds(StandardCounterpointFifthSpeciesEndsRule which,
                                                         const StandardCounterpointFifthSpeciesExercise *exercise,
                                                         const PitchSpelling *tonic) {
    if (!valid_rule(which) || tonic == NULL || !standard_counterpoint_fifth_species_exercise_valid(exercise)) {
        return false;
    }
    switch (which) {
        case STANDARD_COUNTERPOINT_FIFTH_SPECIES_ENDS_BEGIN:
            return check_begin(exercise);
        case STANDARD_COUNTERPOINT_FIFTH_SPECIES_ENDS_CLAUSULA_VERA:
            return check_clausula_vera(exercise);
        case STANDARD_COUNTERPOINT_FIFTH_SPECIES_ENDS_FINAL_BAR:
            return check_final_bar(exercise, tonic);
        default:
            return false;
    }
}

bool standard_counterpoint_fifth_species_ends_check(const StandardCounterpointFifthSpeciesExercise *exercise,
                                                    const PitchSpelling *tonic, unsigned *violations) {
    if (violations == NULL || tonic == NULL || !standard_counterpoint_fifth_species_exercise_valid(exercise)) {
        return false;
    }
    unsigned mask = 0;
    for (int i = 0; i < STANDARD_COUNTERPOINT_FIFTH_SPECIES_ENDS_COUNT; i++) {
        if (!standard_counterpoint_fifth_species_ends_rule_holds((StandardCounterpointFifthSpeciesEndsRule)i, exercise,
                                                                 tonic)) {
            mask |= 1u << i;
        }
    }
    *violations = mask;
    return true;
}
