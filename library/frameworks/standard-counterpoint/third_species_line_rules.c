#include "third_species_line_rules.h"

#include <stddef.h>
#include <stdio.h>

#include "cantus_firmus_rules.h"
#include "counterpoint_interval.h"
#include "counterpoint_line.h"
#include "counterpoint_pitch.h"
#include "rule_pair.h"

#define BUFFER_SIZE (STANDARD_COUNTERPOINT_THIRD_SPECIES_SLOTS_PER_BAR * STANDARD_COUNTERPOINT_THIRD_SPECIES_MAX_BARS)

static const char *const NAMES[STANDARD_COUNTERPOINT_THIRD_SPECIES_LINE_COUNT] = {
    "leaps",         "final-step", "range",           "leading-tone",     "single-climax",
    "no-repeated-motive", "stepwise", "leap-placement", "secondary-climaxes"};

static bool rule_is_hard(StandardCounterpointThirdSpeciesLineRule which) {
    return which == STANDARD_COUNTERPOINT_THIRD_SPECIES_LINE_LEAPS ||
           which == STANDARD_COUNTERPOINT_THIRD_SPECIES_LINE_FINAL_STEP ||
           which == STANDARD_COUNTERPOINT_THIRD_SPECIES_LINE_RANGE ||
           which == STANDARD_COUNTERPOINT_THIRD_SPECIES_LINE_LEADING_TONE;
}

static StandardCounterpointRulePair g_pairs[STANDARD_COUNTERPOINT_THIRD_SPECIES_LINE_COUNT];
static bool g_built[STANDARD_COUNTERPOINT_THIRD_SPECIES_LINE_COUNT];

static bool valid_rule(StandardCounterpointThirdSpeciesLineRule which) {
    return which >= 0 && which < STANDARD_COUNTERPOINT_THIRD_SPECIES_LINE_COUNT;
}

static void ensure_built(StandardCounterpointThirdSpeciesLineRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    char name[96];
    snprintf(name, sizeof(name), "third-species-line-%s", NAMES[which]);
    standard_counterpoint_build_rule_pair(name, rule_is_hard(which) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT,
                                          &g_pairs[which]);
}

const Rule *standard_counterpoint_third_species_line_rule(StandardCounterpointThirdSpeciesLineRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_counterpoint_third_species_line_constraint(StandardCounterpointThirdSpeciesLineRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

const char *standard_counterpoint_third_species_line_rule_name(StandardCounterpointThirdSpeciesLineRule which) {
    return valid_rule(which) ? NAMES[which] : NULL;
}

typedef StandardCounterpointThirdSpeciesExercise Exercise;

/* The attacks of the counterpoint, as a line and with each one's slot. */
typedef struct {
    const PitchSpelling *notes[BUFFER_SIZE];
    size_t slots[BUFFER_SIZE];
    StandardCounterpointLine line;
} Attacks;

static bool read_attacks(const Exercise *exercise, Attacks *attacks) {
    size_t count;
    if (!standard_counterpoint_third_species_attacks(exercise, attacks->notes, attacks->slots, BUFFER_SIZE, &count) ||
        count == 0) {
        return false;
    }
    attacks->line.notes = attacks->notes;
    attacks->line.count = count;
    return true;
}

/* True when the attacks j - 1 and j fall in the same bar. */
static bool within_bar(const Attacks *attacks, size_t j) {
    return attacks->slots[j - 1] / STANDARD_COUNTERPOINT_THIRD_SPECIES_SLOTS_PER_BAR ==
           attacks->slots[j] / STANDARD_COUNTERPOINT_THIRD_SPECIES_SLOTS_PER_BAR;
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
    return total == 0 || 4 * steps >= 3 * total;
}

static bool check_leap_placement(const Attacks *attacks) {
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

/* The climaxes are judged on the downbeats, the skeleton of the line: with
   four notes to a bar, nearly every bar has a local high note of its own. */
static bool check_secondary_climaxes(const Attacks *attacks) {
    const PitchSpelling *downbeats[BUFFER_SIZE];
    size_t count = 0;
    for (size_t i = 0; i < attacks->line.count; i++) {
        if (attacks->slots[i] % STANDARD_COUNTERPOINT_THIRD_SPECIES_SLOTS_PER_BAR == 0) {
            downbeats[count++] = attacks->notes[i];
        }
    }
    if (count < STANDARD_COUNTERPOINT_THIRD_SPECIES_MIN_ATTACKS_FOR_SECONDARY_CLIMAXES) {
        return true;
    }
    const PitchSpelling *top = attacks->notes[standard_counterpoint_line_highest_index(&attacks->line)];
    size_t peaks = 0;
    for (size_t i = 1; i + 1 < count; i++) {
        if (standard_counterpoint_direction(downbeats[i - 1], downbeats[i]) > 0 &&
            standard_counterpoint_direction(downbeats[i], downbeats[i + 1]) < 0 &&
            standard_counterpoint_direction(downbeats[i], top) != 0) {
            peaks++;
        }
    }
    return peaks >= 1 && peaks <= STANDARD_COUNTERPOINT_THIRD_SPECIES_MAX_SECONDARY_CLIMAXES;
}

static bool cantus_rule_for(StandardCounterpointThirdSpeciesLineRule which, StandardCounterpointCantusRule *out) {
    switch (which) {
        case STANDARD_COUNTERPOINT_THIRD_SPECIES_LINE_FINAL_STEP:
            *out = STANDARD_COUNTERPOINT_CANTUS_RULE_FINAL_STEP;
            return true;
        case STANDARD_COUNTERPOINT_THIRD_SPECIES_LINE_RANGE:
            *out = STANDARD_COUNTERPOINT_CANTUS_RULE_RANGE;
            return true;
        case STANDARD_COUNTERPOINT_THIRD_SPECIES_LINE_LEADING_TONE:
            *out = STANDARD_COUNTERPOINT_CANTUS_RULE_LEADING_TONE;
            return true;
        case STANDARD_COUNTERPOINT_THIRD_SPECIES_LINE_SINGLE_CLIMAX:
            *out = STANDARD_COUNTERPOINT_CANTUS_RULE_SINGLE_CLIMAX;
            return true;
        case STANDARD_COUNTERPOINT_THIRD_SPECIES_LINE_NO_REPEATED_MOTIVE:
            *out = STANDARD_COUNTERPOINT_CANTUS_RULE_NO_REPEATED_MOTIVE;
            return true;
        default:
            return false;
    }
}

bool standard_counterpoint_third_species_line_rule_holds(StandardCounterpointThirdSpeciesLineRule which,
                                                          const StandardCounterpointThirdSpeciesExercise *exercise,
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
        case STANDARD_COUNTERPOINT_THIRD_SPECIES_LINE_LEAPS:
            return check_leaps(&attacks);
        case STANDARD_COUNTERPOINT_THIRD_SPECIES_LINE_STEPWISE:
            return check_stepwise(&attacks);
        case STANDARD_COUNTERPOINT_THIRD_SPECIES_LINE_LEAP_PLACEMENT:
            return check_leap_placement(&attacks);
        case STANDARD_COUNTERPOINT_THIRD_SPECIES_LINE_SECONDARY_CLIMAXES:
            return check_secondary_climaxes(&attacks);
        default:
            return false;
    }
}

bool standard_counterpoint_third_species_line_check(const StandardCounterpointThirdSpeciesExercise *exercise,
                                                     const PitchSpelling *tonic, unsigned *hard_violations,
                                                     unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL || tonic == NULL ||
        !standard_counterpoint_third_species_exercise_valid(exercise)) {
        return false;
    }
    unsigned hard = 0;
    unsigned soft = 0;
    for (int i = 0; i < STANDARD_COUNTERPOINT_THIRD_SPECIES_LINE_COUNT; i++) {
        StandardCounterpointThirdSpeciesLineRule which = (StandardCounterpointThirdSpeciesLineRule)i;
        if (standard_counterpoint_third_species_line_rule_holds(which, exercise, tonic)) {
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
