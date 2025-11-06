#include "second_species_line_rules.h"

#include <stddef.h>

#include "cantus_firmus_rules.h"
#include "counterpoint_interval.h"
#include "counterpoint_line.h"
#include "counterpoint_pitch.h"
#include "counterpoint_subject.h"
#include "rule_table.h"
#include "second_species_pattern.h"

#define BUFFER_SIZE (STANDARD_COUNTERPOINT_SECOND_SPECIES_SLOTS_PER_BAR * STANDARD_COUNTERPOINT_SECOND_SPECIES_MAX_BARS)

static const char *const NAMES[STANDARD_COUNTERPOINT_SECOND_SPECIES_LINE_COUNT] = {
    "leaps",         "final-step", "range",           "leading-tone",     "single-climax",
    "no-repeated-motive", "stepwise", "leap-direction", "secondary-climaxes"};

static bool rule_is_hard(StandardCounterpointSecondSpeciesLineRule which) {
    return which == STANDARD_COUNTERPOINT_SECOND_SPECIES_LINE_LEAPS ||
           which == STANDARD_COUNTERPOINT_SECOND_SPECIES_LINE_FINAL_STEP ||
           which == STANDARD_COUNTERPOINT_SECOND_SPECIES_LINE_RANGE ||
           which == STANDARD_COUNTERPOINT_SECOND_SPECIES_LINE_LEADING_TONE;
}

static bool table_is_hard(int which) {
    return rule_is_hard((StandardCounterpointSecondSpeciesLineRule)which);
}

static StandardCounterpointRulePair g_pairs[STANDARD_COUNTERPOINT_SECOND_SPECIES_LINE_COUNT];
static bool g_built[STANDARD_COUNTERPOINT_SECOND_SPECIES_LINE_COUNT];
static ConstraintEvalResult evaluate_candidate(const Constraint *constraint, const Value *candidate,
                                               const Context *context);
static PreferenceScore score_candidate(const Preference *preference, const Value *candidate, const Context *context);
static const StandardCounterpointRuleTable TABLE = {
    "second-species-line", NAMES, STANDARD_COUNTERPOINT_SECOND_SPECIES_LINE_COUNT, table_is_hard, g_pairs, g_built,
    evaluate_candidate, score_candidate};

static bool valid_rule(StandardCounterpointSecondSpeciesLineRule which) {
    return standard_counterpoint_rule_table_valid(&TABLE, (int)which);
}

const Rule *standard_counterpoint_second_species_line_rule(StandardCounterpointSecondSpeciesLineRule which) {
    return standard_counterpoint_rule_table_rule(&TABLE, (int)which);
}

const Constraint *standard_counterpoint_second_species_line_constraint(StandardCounterpointSecondSpeciesLineRule which) {
    return standard_counterpoint_rule_table_constraint(&TABLE, (int)which);
}

const Preference *standard_counterpoint_second_species_line_preference(StandardCounterpointSecondSpeciesLineRule which) {
    return standard_counterpoint_rule_table_preference(&TABLE, (int)which);
}

const char *standard_counterpoint_second_species_line_rule_name(StandardCounterpointSecondSpeciesLineRule which) {
    return standard_counterpoint_rule_table_name(&TABLE, (int)which);
}

typedef StandardCounterpointSecondSpeciesExercise Exercise;

/* The attacks of the counterpoint, as a line and with each one's slot. */
typedef struct {
    const PitchSpelling *notes[BUFFER_SIZE];
    size_t slots[BUFFER_SIZE];
    StandardCounterpointLine line;
} Attacks;

static bool read_attacks(const Exercise *exercise, Attacks *attacks) {
    size_t count;
    if (!standard_counterpoint_second_species_attacks(exercise, attacks->notes, attacks->slots, BUFFER_SIZE, &count) ||
        count == 0) {
        return false;
    }
    attacks->line.notes = attacks->notes;
    attacks->line.count = count;
    return true;
}

/* True when the attacks j - 1 and j are the strong and weak beat of one
   bar. */
static bool within_bar(const Attacks *attacks, size_t j) {
    return attacks->slots[j - 1] % STANDARD_COUNTERPOINT_SECOND_SPECIES_SLOTS_PER_BAR == 0 &&
           attacks->slots[j] == attacks->slots[j - 1] + 1;
}

static bool is_change_of_register_leap(const Exercise *exercise, const Attacks *attacks, size_t j) {
    if (!within_bar(attacks, j)) {
        return false;
    }
    StandardCounterpointWeakBeatPattern pattern;
    size_t bar = attacks->slots[j - 1] / STANDARD_COUNTERPOINT_SECOND_SPECIES_SLOTS_PER_BAR;
    return standard_counterpoint_second_species_weak_beat_pattern(exercise, bar, &pattern) &&
           pattern == STANDARD_COUNTERPOINT_WEAK_BEAT_CHANGE_OF_REGISTER;
}

static bool check_leaps(const Exercise *exercise, const Attacks *attacks) {
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
            if (!ascending_minor && !is_change_of_register_leap(exercise, attacks, j)) {
                return false;
            }
        }
    }
    return true;
}

static bool check_stepwise(const Attacks *attacks) {
    size_t total = attacks->line.count - 1;
    size_t steps = 0;
    for (size_t j = 1; j < attacks->line.count; j++) {
        StandardCounterpointInterval interval;
        if (!standard_counterpoint_interval_between(attacks->notes[j - 1], attacks->notes[j], &interval)) {
            return false;
        }
        if (standard_counterpoint_interval_is_step(&interval)) {
            steps++;
        }
    }
    return total == 0 || 3 * steps >= 2 * total;
}

static bool check_leap_direction(const Attacks *attacks) {
    size_t inside = 0, across = 0;
    for (size_t j = 1; j < attacks->line.count; j++) {
        StandardCounterpointInterval interval;
        if (!standard_counterpoint_interval_between(attacks->notes[j - 1], attacks->notes[j], &interval)) {
            return false;
        }
        if (!standard_counterpoint_interval_is_leap(&interval)) {
            continue;
        }
        if (within_bar(attacks, j)) {
            inside++;
        } else {
            across++;
        }
    }
    return across <= inside;
}

static bool check_secondary_climaxes(const Attacks *attacks) {
    size_t count = attacks->line.count;
    if (count < STANDARD_COUNTERPOINT_SECOND_SPECIES_MIN_ATTACKS_FOR_SECONDARY_CLIMAXES) {
        return true;
    }
    const PitchSpelling *top = attacks->notes[standard_counterpoint_line_highest_index(&attacks->line)];
    size_t peaks = 0;
    for (size_t i = 1; i + 1 < count; i++) {
        if (standard_counterpoint_direction(attacks->notes[i - 1], attacks->notes[i]) > 0 &&
            standard_counterpoint_direction(attacks->notes[i], attacks->notes[i + 1]) < 0 &&
            standard_counterpoint_direction(attacks->notes[i], top) != 0) {
            peaks++;
        }
    }
    return peaks >= 1 && peaks <= STANDARD_COUNTERPOINT_SECOND_SPECIES_MAX_SECONDARY_CLIMAXES;
}

static bool cantus_rule_for(StandardCounterpointSecondSpeciesLineRule which, StandardCounterpointCantusRule *out) {
    switch (which) {
        case STANDARD_COUNTERPOINT_SECOND_SPECIES_LINE_FINAL_STEP:
            *out = STANDARD_COUNTERPOINT_CANTUS_RULE_FINAL_STEP;
            return true;
        case STANDARD_COUNTERPOINT_SECOND_SPECIES_LINE_RANGE:
            *out = STANDARD_COUNTERPOINT_CANTUS_RULE_RANGE;
            return true;
        case STANDARD_COUNTERPOINT_SECOND_SPECIES_LINE_LEADING_TONE:
            *out = STANDARD_COUNTERPOINT_CANTUS_RULE_LEADING_TONE;
            return true;
        case STANDARD_COUNTERPOINT_SECOND_SPECIES_LINE_SINGLE_CLIMAX:
            *out = STANDARD_COUNTERPOINT_CANTUS_RULE_SINGLE_CLIMAX;
            return true;
        case STANDARD_COUNTERPOINT_SECOND_SPECIES_LINE_NO_REPEATED_MOTIVE:
            *out = STANDARD_COUNTERPOINT_CANTUS_RULE_NO_REPEATED_MOTIVE;
            return true;
        default:
            return false;
    }
}

bool standard_counterpoint_second_species_line_rule_holds(StandardCounterpointSecondSpeciesLineRule which,
                                                          const StandardCounterpointSecondSpeciesExercise *exercise,
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
        case STANDARD_COUNTERPOINT_SECOND_SPECIES_LINE_LEAPS:
            return check_leaps(exercise, &attacks);
        case STANDARD_COUNTERPOINT_SECOND_SPECIES_LINE_STEPWISE:
            return check_stepwise(&attacks);
        case STANDARD_COUNTERPOINT_SECOND_SPECIES_LINE_LEAP_DIRECTION:
            return check_leap_direction(&attacks);
        case STANDARD_COUNTERPOINT_SECOND_SPECIES_LINE_SECONDARY_CLIMAXES:
            return check_secondary_climaxes(&attacks);
        default:
            return false;
    }
}

typedef StandardCounterpointSecondSpeciesTonic Subject;

static bool holds_adapter(int which, const void *subject) {
    const Subject *s = subject;
    return standard_counterpoint_second_species_line_rule_holds((StandardCounterpointSecondSpeciesLineRule)which,
                                                                s->exercise, s->tonic);
}

STANDARD_COUNTERPOINT_RULE_TABLE_EVALUATOR(evaluate_candidate, TABLE, holds_adapter, Subject)
STANDARD_COUNTERPOINT_RULE_TABLE_SCORER(score_candidate, TABLE, holds_adapter, Subject)

bool standard_counterpoint_second_species_line_check(const StandardCounterpointSecondSpeciesExercise *exercise,
                                                     const PitchSpelling *tonic, unsigned *hard_violations,
                                                     unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL || tonic == NULL ||
        !standard_counterpoint_second_species_exercise_valid(exercise)) {
        return false;
    }
    Subject subject = {exercise, tonic};
    standard_counterpoint_rule_table_check(&TABLE, holds_adapter, &subject, hard_violations,
                                           soft_violations);
    return true;
}
