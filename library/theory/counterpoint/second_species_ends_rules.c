#include "theory/counterpoint/second_species_ends_rules.h"

#include <stddef.h>

#include "theory/tonality/interval.h"
#include "model/pitch/pitch_arithmetic.h"
#include "theory/counterpoint/counterpoint_subject.h"
#include "theory/counterpoint/framework.h"
#include "theory/rules/rule_table.h"
#include "theory/counterpoint/species_helpers.h"
#include "model/pitch/midi_pitch.h"

static const char *const NAMES[STANDARD_COUNTERPOINT_SECOND_SPECIES_ENDS_COUNT] = {"begin", "opening-bar", "final-bar",
                                                                                    "clausula-vera"};

static RulePair g_pairs[STANDARD_COUNTERPOINT_SECOND_SPECIES_ENDS_COUNT];
static UrsatzOnce g_built[STANDARD_COUNTERPOINT_SECOND_SPECIES_ENDS_COUNT];
static ConstraintEvalResult evaluate_candidate(const Constraint *constraint, const Value *candidate,
                                               const Context *context);
static PreferenceScore score_candidate(const Preference *preference, const Value *candidate, const Context *context);
static const RuleTable TABLE = {
    "standard-counterpoint", standard_counterpoint_framework, "standard-counterpoint",
    "second-species-ends", NAMES, STANDARD_COUNTERPOINT_SECOND_SPECIES_ENDS_COUNT, NULL, g_pairs, g_built,
    evaluate_candidate, score_candidate};

static bool valid_rule(StandardCounterpointSecondSpeciesEndsRule which) {
    return rule_table_valid(&TABLE, (int)which);
}

typedef StandardCounterpointSecondSpeciesExercise Exercise;

static const StandardCounterpointSlot *slot_at(const Exercise *exercise, size_t slot) {
    return &exercise->counterpoint->slots[slot];
}

static bool check_begin(const Exercise *exercise, const PitchSpelling *tonic) {
    const StandardCounterpointSlot *first = slot_at(exercise, 0);
    if (first->kind == STANDARD_COUNTERPOINT_SLOT_REST) {
        first = slot_at(exercise, 1);
    }
    if (exercise->cantus_part == STANDARD_COUNTERPOINT_CANTUS_IN_LOWER) {
        return standard_counterpoint_species_is_tonic(first->pitch, tonic) || standard_counterpoint_species_is_sol(first->pitch, tonic);
    }
    return standard_counterpoint_species_is_tonic(first->pitch, tonic);
}

static bool check_opening_bar(const Exercise *exercise) {
    return slot_at(exercise, 1)->kind == STANDARD_COUNTERPOINT_SLOT_ATTACK;
}

static bool check_final_bar(const Exercise *exercise, const PitchSpelling *tonic) {
    size_t last_slot = exercise->counterpoint->count - 1;
    const StandardCounterpointSlot *strong = slot_at(exercise, last_slot - 1);
    const StandardCounterpointSlot *weak = slot_at(exercise, last_slot);
    return strong->kind == STANDARD_COUNTERPOINT_SLOT_ATTACK && weak->kind == STANDARD_COUNTERPOINT_SLOT_TIE &&
           standard_counterpoint_species_is_tonic(strong->pitch, tonic);
}

static bool check_clausula_vera(const Exercise *exercise) {
    size_t bars = exercise->cantus->count;
    const PitchSpelling *cantus_before = exercise->cantus->notes[bars - 2];
    const PitchSpelling *cantus_final = exercise->cantus->notes[bars - 1];
    const PitchSpelling *before = slot_at(exercise, 2 * bars - 3)->pitch;
    const PitchSpelling *final = slot_at(exercise, 2 * bars - 2)->pitch;

    StandardTonalityInterval cantus_move, counterpoint_move, vertical;
    if (!standard_tonality_interval_between(cantus_before, cantus_final, &cantus_move) ||
        !standard_tonality_interval_between(before, final, &counterpoint_move) ||
        !standard_tonality_interval_between(cantus_before, before, &vertical)) {
        return false;
    }
    int cantus_direction = pitch_spelling_direction(cantus_before, cantus_final);
    int counterpoint_direction = pitch_spelling_direction(before, final);
    bool major_or_minor = vertical.quality == STANDARD_TONALITY_INTERVAL_QUALITY_MAJOR ||
                          vertical.quality == STANDARD_TONALITY_INTERVAL_QUALITY_MINOR;
    return standard_tonality_interval_is_step(&cantus_move) &&
           standard_tonality_interval_is_step(&counterpoint_move) && cantus_direction != 0 &&
           counterpoint_direction != 0 && cantus_direction != counterpoint_direction && major_or_minor &&
           (vertical.simple_size == 3 || vertical.simple_size == 6);
}

bool standard_counterpoint_second_species_ends_rule_holds(StandardCounterpointSecondSpeciesEndsRule which,
                                                          const StandardCounterpointSecondSpeciesExercise *exercise,
                                                          const PitchSpelling *tonic) {
    if (!valid_rule(which) || tonic == NULL || !standard_counterpoint_second_species_exercise_valid(exercise)) {
        return false;
    }
    switch (which) {
        case STANDARD_COUNTERPOINT_SECOND_SPECIES_ENDS_BEGIN:
            return check_begin(exercise, tonic);
        case STANDARD_COUNTERPOINT_SECOND_SPECIES_ENDS_OPENING_BAR:
            return check_opening_bar(exercise);
        case STANDARD_COUNTERPOINT_SECOND_SPECIES_ENDS_FINAL_BAR:
            return check_final_bar(exercise, tonic);
        case STANDARD_COUNTERPOINT_SECOND_SPECIES_ENDS_CLAUSULA_VERA:
            return check_clausula_vera(exercise);
        default:
            return false;
    }
}

typedef StandardCounterpointSecondSpeciesTonic Subject;

static bool holds_adapter(int which, const void *subject) {
    const Subject *s = subject;
    return standard_counterpoint_second_species_ends_rule_holds((StandardCounterpointSecondSpeciesEndsRule)which,
                                                                s->exercise, s->tonic);
}

RULE_TABLE_EVALUATOR(evaluate_candidate, TABLE, holds_adapter, Subject)
RULE_TABLE_SCORER(score_candidate, TABLE, holds_adapter, Subject)

bool standard_counterpoint_second_species_ends_check(const StandardCounterpointSecondSpeciesExercise *exercise,
                                                     const PitchSpelling *tonic, unsigned *violations) {
    if (violations == NULL || tonic == NULL || !standard_counterpoint_second_species_exercise_valid(exercise)) {
        return false;
    }
    Subject subject = {exercise, tonic};
    unsigned unused_soft;
    rule_table_check(&TABLE, holds_adapter, &subject, violations, &unused_soft);
    return true;
}
