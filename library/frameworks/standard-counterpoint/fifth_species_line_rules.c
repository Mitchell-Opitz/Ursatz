#include "fifth_species_line_rules.h"

#include <stddef.h>

#include "cantus_firmus_rules.h"
#include "counterpoint_interval.h"
#include "counterpoint_line.h"
#include "counterpoint_pitch.h"
#include "counterpoint_subject.h"
#include "rule_table.h"
#include "species_helpers.h"

#define BUFFER_SIZE STANDARD_COUNTERPOINT_FIFTH_SPECIES_MAX_NOTES

static const char *const NAMES[STANDARD_COUNTERPOINT_FIFTH_SPECIES_LINE_COUNT] = {
    "leaps",         "final-step",    "range",           "leading-tone",
    "single-climax", "leap-recovery", "mostly-stepwise", "no-repeated-motive"};

static bool rule_is_hard(StandardCounterpointFifthSpeciesLineRule which) {
    return which <= STANDARD_COUNTERPOINT_FIFTH_SPECIES_LINE_LEADING_TONE;
}

static bool table_is_hard(int which) {
    return rule_is_hard((StandardCounterpointFifthSpeciesLineRule)which);
}

static StandardCounterpointRulePair g_pairs[STANDARD_COUNTERPOINT_FIFTH_SPECIES_LINE_COUNT];
static UrsatzOnce g_built[STANDARD_COUNTERPOINT_FIFTH_SPECIES_LINE_COUNT];
static ConstraintEvalResult evaluate_candidate(const Constraint *constraint, const Value *candidate,
                                               const Context *context);
static PreferenceScore score_candidate(const Preference *preference, const Value *candidate, const Context *context);
static const StandardCounterpointRuleTable TABLE = {
    "fifth-species-line", NAMES, STANDARD_COUNTERPOINT_FIFTH_SPECIES_LINE_COUNT, table_is_hard, g_pairs, g_built,
    evaluate_candidate, score_candidate};

static bool valid_rule(StandardCounterpointFifthSpeciesLineRule which) {
    return standard_counterpoint_rule_table_valid(&TABLE, (int)which);
}

const Rule *standard_counterpoint_fifth_species_line_rule(StandardCounterpointFifthSpeciesLineRule which) {
    return standard_counterpoint_rule_table_rule(&TABLE, (int)which);
}

const Constraint *standard_counterpoint_fifth_species_line_constraint(StandardCounterpointFifthSpeciesLineRule which) {
    return standard_counterpoint_rule_table_constraint(&TABLE, (int)which);
}

const Preference *standard_counterpoint_fifth_species_line_preference(StandardCounterpointFifthSpeciesLineRule which) {
    return standard_counterpoint_rule_table_preference(&TABLE, (int)which);
}

const char *standard_counterpoint_fifth_species_line_rule_name(StandardCounterpointFifthSpeciesLineRule which) {
    return standard_counterpoint_rule_table_name(&TABLE, (int)which);
}

typedef StandardCounterpointFifthSpeciesExercise Exercise;

typedef struct {
    const PitchSpelling *notes[BUFFER_SIZE];
    size_t slots[BUFFER_SIZE];
    StandardCounterpointLine line;
} Attacks;

static bool read_attacks(const Exercise *exercise, Attacks *attacks) {
    size_t count;
    if (!standard_counterpoint_fifth_species_attacks(exercise, attacks->notes, attacks->slots, BUFFER_SIZE, &count) ||
        count == 0) {
        return false;
    }
    attacks->line.notes = attacks->notes;
    attacks->line.count = count;
    return true;
}

static bool cantus_rule_for(StandardCounterpointFifthSpeciesLineRule which, StandardCounterpointCantusRule *out) {
    switch (which) {
        case STANDARD_COUNTERPOINT_FIFTH_SPECIES_LINE_FINAL_STEP:
            *out = STANDARD_COUNTERPOINT_CANTUS_RULE_FINAL_STEP;
            return true;
        case STANDARD_COUNTERPOINT_FIFTH_SPECIES_LINE_RANGE:
            *out = STANDARD_COUNTERPOINT_CANTUS_RULE_RANGE;
            return true;
        case STANDARD_COUNTERPOINT_FIFTH_SPECIES_LINE_LEADING_TONE:
            *out = STANDARD_COUNTERPOINT_CANTUS_RULE_LEADING_TONE;
            return true;
        case STANDARD_COUNTERPOINT_FIFTH_SPECIES_LINE_SINGLE_CLIMAX:
            *out = STANDARD_COUNTERPOINT_CANTUS_RULE_SINGLE_CLIMAX;
            return true;
        case STANDARD_COUNTERPOINT_FIFTH_SPECIES_LINE_LEAP_RECOVERY:
            *out = STANDARD_COUNTERPOINT_CANTUS_RULE_LEAP_RECOVERY;
            return true;
        case STANDARD_COUNTERPOINT_FIFTH_SPECIES_LINE_MOSTLY_STEPWISE:
            *out = STANDARD_COUNTERPOINT_CANTUS_RULE_MOSTLY_STEPWISE;
            return true;
        case STANDARD_COUNTERPOINT_FIFTH_SPECIES_LINE_NO_REPEATED_MOTIVE:
            *out = STANDARD_COUNTERPOINT_CANTUS_RULE_NO_REPEATED_MOTIVE;
            return true;
        default:
            return false;
    }
}

bool standard_counterpoint_fifth_species_line_rule_holds(StandardCounterpointFifthSpeciesLineRule which,
                                                          const StandardCounterpointFifthSpeciesExercise *exercise,
                                                          const PitchSpelling *tonic) {
    Attacks attacks;
    if (!valid_rule(which) || tonic == NULL || !read_attacks(exercise, &attacks)) {
        return false;
    }
    StandardCounterpointCantusRule cantus_rule;
    if (cantus_rule_for(which, &cantus_rule)) {
        return standard_counterpoint_cantus_rule_holds(cantus_rule, &attacks.line, tonic);
    }
    switch (which) {
        case STANDARD_COUNTERPOINT_FIFTH_SPECIES_LINE_LEAPS:
            return standard_counterpoint_species_check_leaps(attacks.notes, attacks.line.count, NULL, NULL);
        default:
            return false;
    }
}

typedef StandardCounterpointFifthSpeciesTonic Subject;

static bool holds_adapter(int which, const void *subject) {
    const Subject *s = subject;
    return standard_counterpoint_fifth_species_line_rule_holds((StandardCounterpointFifthSpeciesLineRule)which,
                                                               s->exercise, s->tonic);
}

STANDARD_COUNTERPOINT_RULE_TABLE_EVALUATOR(evaluate_candidate, TABLE, holds_adapter, Subject)
STANDARD_COUNTERPOINT_RULE_TABLE_SCORER(score_candidate, TABLE, holds_adapter, Subject)

bool standard_counterpoint_fifth_species_line_check(const StandardCounterpointFifthSpeciesExercise *exercise,
                                                     const PitchSpelling *tonic, unsigned *hard_violations,
                                                     unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL || tonic == NULL ||
        !standard_counterpoint_fifth_species_exercise_valid(exercise)) {
        return false;
    }
    Subject subject = {exercise, tonic};
    standard_counterpoint_rule_table_check(&TABLE, holds_adapter, &subject, hard_violations,
                                           soft_violations);
    return true;
}
