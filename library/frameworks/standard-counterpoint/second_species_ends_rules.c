#include "second_species_ends_rules.h"

#include <stddef.h>

#include "counterpoint_interval.h"
#include "counterpoint_pitch.h"
#include "counterpoint_subject.h"
#include "rule_table.h"

static const char *const NAMES[STANDARD_COUNTERPOINT_SECOND_SPECIES_ENDS_COUNT] = {"begin", "opening-bar", "final-bar",
                                                                                    "clausula-vera"};

static StandardCounterpointRulePair g_pairs[STANDARD_COUNTERPOINT_SECOND_SPECIES_ENDS_COUNT];
static UrsatzOnce g_built[STANDARD_COUNTERPOINT_SECOND_SPECIES_ENDS_COUNT];
static ConstraintEvalResult evaluate_candidate(const Constraint *constraint, const Value *candidate,
                                               const Context *context);
static PreferenceScore score_candidate(const Preference *preference, const Value *candidate, const Context *context);
static const StandardCounterpointRuleTable TABLE = {
    "second-species-ends", NAMES, STANDARD_COUNTERPOINT_SECOND_SPECIES_ENDS_COUNT, NULL, g_pairs, g_built,
    evaluate_candidate, score_candidate};

static bool valid_rule(StandardCounterpointSecondSpeciesEndsRule which) {
    return standard_counterpoint_rule_table_valid(&TABLE, (int)which);
}

const Rule *standard_counterpoint_second_species_ends_rule(StandardCounterpointSecondSpeciesEndsRule which) {
    return standard_counterpoint_rule_table_rule(&TABLE, (int)which);
}

const Constraint *standard_counterpoint_second_species_ends_constraint(StandardCounterpointSecondSpeciesEndsRule which) {
    return standard_counterpoint_rule_table_constraint(&TABLE, (int)which);
}

const Preference *standard_counterpoint_second_species_ends_preference(StandardCounterpointSecondSpeciesEndsRule which) {
    return standard_counterpoint_rule_table_preference(&TABLE, (int)which);
}

const char *standard_counterpoint_second_species_ends_rule_name(StandardCounterpointSecondSpeciesEndsRule which) {
    return standard_counterpoint_rule_table_name(&TABLE, (int)which);
}

typedef StandardCounterpointSecondSpeciesExercise Exercise;

static bool is_tonic(const PitchSpelling *note, const PitchSpelling *tonic) {
    return pitch_spelling_letter(note) == pitch_spelling_letter(tonic) &&
           pitch_spelling_accidental(note) == pitch_spelling_accidental(tonic);
}

/* A perfect fifth above the tonic, at any octave. */
static bool is_sol(const PitchSpelling *note, const PitchSpelling *tonic) {
    int letters = (((standard_counterpoint_step_number(note) - standard_counterpoint_step_number(tonic)) % 7) + 7) % 7;
    int semitones =
        (((standard_counterpoint_semitone_number(note) - standard_counterpoint_semitone_number(tonic)) % 12) + 12) % 12;
    return letters == 4 && semitones == 7;
}

static const StandardCounterpointSlot *slot_at(const Exercise *exercise, size_t slot) {
    return &exercise->counterpoint->slots[slot];
}

static bool check_begin(const Exercise *exercise, const PitchSpelling *tonic) {
    const StandardCounterpointSlot *first = slot_at(exercise, 0);
    if (first->kind == STANDARD_COUNTERPOINT_SLOT_REST) {
        first = slot_at(exercise, 1);
    }
    if (exercise->cantus_part == STANDARD_COUNTERPOINT_CANTUS_IN_LOWER) {
        return is_tonic(first->pitch, tonic) || is_sol(first->pitch, tonic);
    }
    return is_tonic(first->pitch, tonic);
}

static bool check_opening_bar(const Exercise *exercise) {
    return slot_at(exercise, 1)->kind == STANDARD_COUNTERPOINT_SLOT_ATTACK;
}

static bool check_final_bar(const Exercise *exercise, const PitchSpelling *tonic) {
    size_t last_slot = exercise->counterpoint->count - 1;
    const StandardCounterpointSlot *strong = slot_at(exercise, last_slot - 1);
    const StandardCounterpointSlot *weak = slot_at(exercise, last_slot);
    return strong->kind == STANDARD_COUNTERPOINT_SLOT_ATTACK && weak->kind == STANDARD_COUNTERPOINT_SLOT_TIE &&
           is_tonic(strong->pitch, tonic);
}

static bool check_clausula_vera(const Exercise *exercise) {
    size_t bars = exercise->cantus->count;
    const PitchSpelling *cantus_before = exercise->cantus->notes[bars - 2];
    const PitchSpelling *cantus_final = exercise->cantus->notes[bars - 1];
    const PitchSpelling *before = slot_at(exercise, 2 * bars - 3)->pitch;
    const PitchSpelling *final = slot_at(exercise, 2 * bars - 2)->pitch;

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

STANDARD_COUNTERPOINT_RULE_TABLE_EVALUATOR(evaluate_candidate, TABLE, holds_adapter, Subject)
STANDARD_COUNTERPOINT_RULE_TABLE_SCORER(score_candidate, TABLE, holds_adapter, Subject)

bool standard_counterpoint_second_species_ends_check(const StandardCounterpointSecondSpeciesExercise *exercise,
                                                     const PitchSpelling *tonic, unsigned *violations) {
    if (violations == NULL || tonic == NULL || !standard_counterpoint_second_species_exercise_valid(exercise)) {
        return false;
    }
    Subject subject = {exercise, tonic};
    unsigned unused_soft;
    standard_counterpoint_rule_table_check(&TABLE, holds_adapter, &subject, violations, &unused_soft);
    return true;
}
