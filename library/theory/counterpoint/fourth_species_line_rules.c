#include "theory/counterpoint/fourth_species_line_rules.h"

#include <stddef.h>

#include "theory/counterpoint/cantus_firmus_rules.h"
#include "theory/tonality/interval.h"
#include "theory/counterpoint/counterpoint_line.h"
#include "model/pitch/pitch_arithmetic.h"
#include "theory/counterpoint/counterpoint_subject.h"
#include "theory/rules/rule_table.h"
#include "theory/counterpoint/species_helpers.h"

#define BUFFER_SIZE (STANDARD_COUNTERPOINT_FOURTH_SPECIES_SLOTS_PER_BAR * STANDARD_COUNTERPOINT_FOURTH_SPECIES_MAX_BARS)

static const char *const NAMES[STANDARD_COUNTERPOINT_FOURTH_SPECIES_LINE_COUNT] = {
    "leaps", "final-step", "range", "leading-tone", "downward-steps", "upward-leap"};

static const bool SOFT[STANDARD_COUNTERPOINT_FOURTH_SPECIES_LINE_COUNT] = {
    [STANDARD_COUNTERPOINT_FOURTH_SPECIES_LINE_DOWNWARD_STEPS] = true,
    [STANDARD_COUNTERPOINT_FOURTH_SPECIES_LINE_UPWARD_LEAP] = true};

static const RuleTable TABLE = {
    .name = "fourth-species-line", .names = NAMES, .soft = SOFT,
    .count = STANDARD_COUNTERPOINT_FOURTH_SPECIES_LINE_COUNT};

static bool valid_rule(StandardCounterpointFourthSpeciesLineRule which) {
    return rule_table_valid(&TABLE, (int)which);
}

typedef StandardCounterpointFourthSpeciesExercise Exercise;

typedef struct {
    const PitchSpelling *notes[BUFFER_SIZE];
    size_t slots[BUFFER_SIZE];
    StandardCounterpointLine line;
} Attacks;

static bool read_attacks(const Exercise *exercise, Attacks *attacks) {
    size_t count;
    if (!standard_counterpoint_fourth_species_attacks(exercise, attacks->notes, attacks->slots, BUFFER_SIZE, &count) ||
        count == 0) {
        return false;
    }
    attacks->line.notes = attacks->notes;
    attacks->line.count = count;
    return true;
}

static bool check_downward_steps(const Attacks *attacks) {
    size_t total = attacks->line.count - 1;
    size_t down = 0;
    for (size_t j = 1; j < attacks->line.count; j++) {
        StandardTonalityInterval interval;
        if (!standard_tonality_interval_between(attacks->notes[j - 1], attacks->notes[j], &interval)) {
            return false;
        }
        if (standard_tonality_interval_is_step(&interval) &&
            pitch_spelling_direction(attacks->notes[j - 1], attacks->notes[j]) < 0) {
            down++;
        }
    }
    return total == 0 || 2 * down >= total;
}

static bool check_upward_leap(const Attacks *attacks) {
    for (size_t j = 1; j < attacks->line.count; j++) {
        StandardTonalityInterval interval;
        if (!standard_tonality_interval_between(attacks->notes[j - 1], attacks->notes[j], &interval)) {
            return false;
        }
        if (standard_tonality_interval_is_leap(&interval) &&
            pitch_spelling_direction(attacks->notes[j - 1], attacks->notes[j]) > 0) {
            return true;
        }
    }
    return false;
}

static bool cantus_rule_for(StandardCounterpointFourthSpeciesLineRule which, StandardCounterpointCantusRule *out) {
    switch (which) {
        case STANDARD_COUNTERPOINT_FOURTH_SPECIES_LINE_FINAL_STEP:
            *out = STANDARD_COUNTERPOINT_CANTUS_RULE_FINAL_STEP;
            return true;
        case STANDARD_COUNTERPOINT_FOURTH_SPECIES_LINE_RANGE:
            *out = STANDARD_COUNTERPOINT_CANTUS_RULE_RANGE;
            return true;
        case STANDARD_COUNTERPOINT_FOURTH_SPECIES_LINE_LEADING_TONE:
            *out = STANDARD_COUNTERPOINT_CANTUS_RULE_LEADING_TONE;
            return true;
        default:
            return false;
    }
}

bool standard_counterpoint_fourth_species_line_rule_holds(StandardCounterpointFourthSpeciesLineRule which,
                                                          const StandardCounterpointFourthSpeciesExercise *exercise,
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
        case STANDARD_COUNTERPOINT_FOURTH_SPECIES_LINE_LEAPS:
            return standard_counterpoint_species_check_leaps(attacks.notes, attacks.line.count, NULL, NULL);
        case STANDARD_COUNTERPOINT_FOURTH_SPECIES_LINE_DOWNWARD_STEPS:
            return check_downward_steps(&attacks);
        case STANDARD_COUNTERPOINT_FOURTH_SPECIES_LINE_UPWARD_LEAP:
            return check_upward_leap(&attacks);
        default:
            return false;
    }
}

typedef StandardCounterpointFourthSpeciesTonic Subject;

static bool holds_adapter(int which, const void *subject) {
    const Subject *s = subject;
    return standard_counterpoint_fourth_species_line_rule_holds((StandardCounterpointFourthSpeciesLineRule)which,
                                                                s->exercise, s->tonic);
}


bool standard_counterpoint_fourth_species_line_check(const StandardCounterpointFourthSpeciesExercise *exercise,
                                                     const PitchSpelling *tonic, unsigned *hard_violations,
                                                     unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL || tonic == NULL ||
        !standard_counterpoint_fourth_species_exercise_valid(exercise)) {
        return false;
    }
    Subject subject = {exercise, tonic};
    rule_table_check(&TABLE, holds_adapter, &subject, hard_violations,
                                           soft_violations);
    return true;
}
