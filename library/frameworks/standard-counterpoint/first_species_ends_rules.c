#include "first_species_ends_rules.h"

#include <stddef.h>
#include <stdio.h>

#include "counterpoint_interval.h"
#include "counterpoint_pitch.h"
#include "motion_rules.h"
#include "rule_pair.h"

static const char *const NAMES[STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_COUNT] = {
    "begin", "end-on-tonic", "contrary-stepwise-approach", "penultimate-third-or-sixth"};

static StandardCounterpointRulePair g_pairs[STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_COUNT];
static bool g_built[STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_COUNT];

static bool valid_rule(StandardCounterpointFirstSpeciesEndsRule which) {
    return which >= 0 && which < STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_COUNT;
}

static void ensure_built(StandardCounterpointFirstSpeciesEndsRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    char name[96];
    snprintf(name, sizeof(name), "first-species-ends-%s", NAMES[which]);
    standard_counterpoint_build_rule_pair(name, CONSTRAINT_TYPE_HARD, &g_pairs[which]);
}

const Rule *standard_counterpoint_first_species_ends_rule(StandardCounterpointFirstSpeciesEndsRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_counterpoint_first_species_ends_constraint(StandardCounterpointFirstSpeciesEndsRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

const char *standard_counterpoint_first_species_ends_rule_name(StandardCounterpointFirstSpeciesEndsRule which) {
    return valid_rule(which) ? NAMES[which] : NULL;
}

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

static const StandardCounterpointLine *counterpoint_line(const StandardCounterpointPair *pair,
                                                         StandardCounterpointCantusPart cantus_part) {
    return cantus_part == STANDARD_COUNTERPOINT_CANTUS_IN_LOWER ? pair->upper : pair->lower;
}

static bool check_begin(const StandardCounterpointPair *pair, StandardCounterpointCantusPart cantus_part,
                        const PitchSpelling *tonic) {
    const PitchSpelling *first = counterpoint_line(pair, cantus_part)->notes[0];
    if (cantus_part == STANDARD_COUNTERPOINT_CANTUS_IN_LOWER) {
        return is_tonic(first, tonic) || is_sol(first, tonic);
    }
    return is_tonic(first, tonic);
}

static bool check_end_on_tonic(const StandardCounterpointPair *pair, StandardCounterpointCantusPart cantus_part,
                               const PitchSpelling *tonic) {
    const StandardCounterpointLine *line = counterpoint_line(pair, cantus_part);
    return is_tonic(line->notes[line->count - 1], tonic);
}

static bool check_contrary_stepwise_approach(const StandardCounterpointPair *pair) {
    size_t last = pair->lower->count - 1;
    if (last == 0) {
        return false;
    }
    MotionType motion;
    StandardCounterpointInterval lower_step, upper_step;
    return standard_counterpoint_pair_motion(pair, last - 1, &motion) && motion == MOTION_TYPE_CONTRARY &&
           standard_counterpoint_line_interval_at(pair->lower, last - 1, &lower_step) &&
           standard_counterpoint_line_interval_at(pair->upper, last - 1, &upper_step) &&
           standard_counterpoint_interval_is_step(&lower_step) && standard_counterpoint_interval_is_step(&upper_step);
}

static bool check_penultimate_third_or_sixth(const StandardCounterpointPair *pair) {
    size_t last = pair->lower->count - 1;
    if (last == 0) {
        return false;
    }
    StandardCounterpointInterval interval;
    if (!standard_counterpoint_interval_between(pair->lower->notes[last - 1], pair->upper->notes[last - 1],
                                                &interval)) {
        return false;
    }
    bool major_or_minor = interval.quality == STANDARD_COUNTERPOINT_QUALITY_MAJOR ||
                          interval.quality == STANDARD_COUNTERPOINT_QUALITY_MINOR;
    return major_or_minor && (interval.simple_size == 3 || interval.simple_size == 6);
}

bool standard_counterpoint_first_species_ends_rule_holds(StandardCounterpointFirstSpeciesEndsRule which,
                                                         const StandardCounterpointPair *pair,
                                                         StandardCounterpointCantusPart cantus_part,
                                                         const PitchSpelling *tonic) {
    if (!valid_rule(which) || !standard_counterpoint_pair_valid(pair) || tonic == NULL ||
        (cantus_part != STANDARD_COUNTERPOINT_CANTUS_IN_LOWER &&
         cantus_part != STANDARD_COUNTERPOINT_CANTUS_IN_UPPER)) {
        return false;
    }
    switch (which) {
        case STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_BEGIN:
            return check_begin(pair, cantus_part, tonic);
        case STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_END_ON_TONIC:
            return check_end_on_tonic(pair, cantus_part, tonic);
        case STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_CONTRARY_STEPWISE_APPROACH:
            return check_contrary_stepwise_approach(pair);
        case STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_PENULTIMATE_THIRD_OR_SIXTH:
            return check_penultimate_third_or_sixth(pair);
        default:
            return false;
    }
}

bool standard_counterpoint_first_species_ends_check(const StandardCounterpointPair *pair,
                                                    StandardCounterpointCantusPart cantus_part,
                                                    const PitchSpelling *tonic, unsigned *violations) {
    if (violations == NULL || !standard_counterpoint_pair_valid(pair) || tonic == NULL ||
        (cantus_part != STANDARD_COUNTERPOINT_CANTUS_IN_LOWER &&
         cantus_part != STANDARD_COUNTERPOINT_CANTUS_IN_UPPER)) {
        return false;
    }
    unsigned mask = 0;
    for (int i = 0; i < STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_COUNT; i++) {
        if (!standard_counterpoint_first_species_ends_rule_holds((StandardCounterpointFirstSpeciesEndsRule)i, pair,
                                                                 cantus_part, tonic)) {
            mask |= 1u << i;
        }
    }
    *violations = mask;
    return true;
}
