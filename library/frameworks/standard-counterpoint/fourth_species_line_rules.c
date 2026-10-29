#include "fourth_species_line_rules.h"

#include <stddef.h>

#include "cantus_firmus_rules.h"
#include "counterpoint_interval.h"
#include "counterpoint_line.h"
#include "counterpoint_pitch.h"
#include "counterpoint_subject.h"
#include "rule_table.h"

#define BUFFER_SIZE (STANDARD_COUNTERPOINT_FOURTH_SPECIES_SLOTS_PER_BAR * STANDARD_COUNTERPOINT_FOURTH_SPECIES_MAX_BARS)

static const char *const NAMES[STANDARD_COUNTERPOINT_FOURTH_SPECIES_LINE_COUNT] = {
    "leaps", "final-step", "range", "leading-tone", "downward-steps", "upward-leap"};

static bool rule_is_soft(StandardCounterpointFourthSpeciesLineRule which) {
    return which == STANDARD_COUNTERPOINT_FOURTH_SPECIES_LINE_DOWNWARD_STEPS ||
           which == STANDARD_COUNTERPOINT_FOURTH_SPECIES_LINE_UPWARD_LEAP;
}

static bool table_is_hard(int which) {
    return !rule_is_soft((StandardCounterpointFourthSpeciesLineRule)which);
}

static StandardCounterpointRulePair g_pairs[STANDARD_COUNTERPOINT_FOURTH_SPECIES_LINE_COUNT];
static UrsatzOnce g_built[STANDARD_COUNTERPOINT_FOURTH_SPECIES_LINE_COUNT];
static ConstraintEvalResult evaluate_candidate(const Constraint *constraint, const Value *candidate,
                                               const Context *context);
static PreferenceScore score_candidate(const Preference *preference, const Value *candidate, const Context *context);
static const StandardCounterpointRuleTable TABLE = {
    "fourth-species-line", NAMES, STANDARD_COUNTERPOINT_FOURTH_SPECIES_LINE_COUNT, table_is_hard, g_pairs, g_built,
    evaluate_candidate, score_candidate};

static bool valid_rule(StandardCounterpointFourthSpeciesLineRule which) {
    return standard_counterpoint_rule_table_valid(&TABLE, (int)which);
}

const Rule *standard_counterpoint_fourth_species_line_rule(StandardCounterpointFourthSpeciesLineRule which) {
    return standard_counterpoint_rule_table_rule(&TABLE, (int)which);
}

const Constraint *standard_counterpoint_fourth_species_line_constraint(StandardCounterpointFourthSpeciesLineRule which) {
    return standard_counterpoint_rule_table_constraint(&TABLE, (int)which);
}

const Preference *standard_counterpoint_fourth_species_line_preference(StandardCounterpointFourthSpeciesLineRule which) {
    return standard_counterpoint_rule_table_preference(&TABLE, (int)which);
}

const char *standard_counterpoint_fourth_species_line_rule_name(StandardCounterpointFourthSpeciesLineRule which) {
    return standard_counterpoint_rule_table_name(&TABLE, (int)which);
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

static bool check_leaps(const Attacks *attacks) {
    for (size_t j = 1; j < attacks->line.count; j++) {
        StandardCounterpointInterval interval;
        if (!standard_counterpoint_interval_between(attacks->notes[j - 1], attacks->notes[j], &interval)) {
            return false;
        }
        if (interval.size > 8 || interval.quality == STANDARD_COUNTERPOINT_QUALITY_AUGMENTED ||
            interval.quality == STANDARD_COUNTERPOINT_QUALITY_DIMINISHED || interval.simple_size == 7) {
            return false;
        }
        if (interval.simple_size == 6) {
            bool ascending_minor = interval.quality == STANDARD_COUNTERPOINT_QUALITY_MINOR &&
                                   standard_counterpoint_direction(attacks->notes[j - 1], attacks->notes[j]) > 0;
            if (!ascending_minor) {
                return false;
            }
        }
    }
    return true;
}

static bool check_downward_steps(const Attacks *attacks) {
    size_t total = attacks->line.count - 1;
    size_t down = 0;
    for (size_t j = 1; j < attacks->line.count; j++) {
        StandardCounterpointInterval interval;
        if (!standard_counterpoint_interval_between(attacks->notes[j - 1], attacks->notes[j], &interval)) {
            return false;
        }
        if (standard_counterpoint_interval_is_step(&interval) &&
            standard_counterpoint_direction(attacks->notes[j - 1], attacks->notes[j]) < 0) {
            down++;
        }
    }
    return total == 0 || 2 * down >= total;
}

static bool check_upward_leap(const Attacks *attacks) {
    for (size_t j = 1; j < attacks->line.count; j++) {
        StandardCounterpointInterval interval;
        if (!standard_counterpoint_interval_between(attacks->notes[j - 1], attacks->notes[j], &interval)) {
            return false;
        }
        if (standard_counterpoint_interval_is_leap(&interval) &&
            standard_counterpoint_direction(attacks->notes[j - 1], attacks->notes[j]) > 0) {
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
            return check_leaps(&attacks);
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

STANDARD_COUNTERPOINT_RULE_TABLE_EVALUATOR(evaluate_candidate, TABLE, holds_adapter, Subject)
STANDARD_COUNTERPOINT_RULE_TABLE_SCORER(score_candidate, TABLE, holds_adapter, Subject)

bool standard_counterpoint_fourth_species_line_check(const StandardCounterpointFourthSpeciesExercise *exercise,
                                                     const PitchSpelling *tonic, unsigned *hard_violations,
                                                     unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL || tonic == NULL ||
        !standard_counterpoint_fourth_species_exercise_valid(exercise)) {
        return false;
    }
    Subject subject = {exercise, tonic};
    standard_counterpoint_rule_table_check(&TABLE, holds_adapter, &subject, hard_violations,
                                           soft_violations);
    return true;
}
