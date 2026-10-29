#include "first_species_ends_rules.h"

#include <stddef.h>

#include "counterpoint_interval.h"
#include "counterpoint_pitch.h"
#include "motion_rules.h"
#include "counterpoint_subject.h"
#include "rule_table.h"

static const char *const NAMES[STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_COUNT] = {
    "begin", "end-on-tonic", "contrary-stepwise-approach", "penultimate-third-or-sixth"};

static StandardCounterpointRulePair g_pairs[STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_COUNT];
static UrsatzOnce g_built[STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_COUNT];
static ConstraintEvalResult evaluate_candidate(const Constraint *constraint, const Value *candidate,
                                               const Context *context);
static PreferenceScore score_candidate(const Preference *preference, const Value *candidate, const Context *context);
static const StandardCounterpointRuleTable TABLE = {
    "first-species-ends", NAMES, STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_COUNT, NULL, g_pairs, g_built,
    evaluate_candidate, score_candidate};

static bool valid_rule(StandardCounterpointFirstSpeciesEndsRule which) {
    return standard_counterpoint_rule_table_valid(&TABLE, (int)which);
}

const Rule *standard_counterpoint_first_species_ends_rule(StandardCounterpointFirstSpeciesEndsRule which) {
    return standard_counterpoint_rule_table_rule(&TABLE, (int)which);
}

const Constraint *standard_counterpoint_first_species_ends_constraint(StandardCounterpointFirstSpeciesEndsRule which) {
    return standard_counterpoint_rule_table_constraint(&TABLE, (int)which);
}

const Preference *standard_counterpoint_first_species_ends_preference(StandardCounterpointFirstSpeciesEndsRule which) {
    return standard_counterpoint_rule_table_preference(&TABLE, (int)which);
}

const char *standard_counterpoint_first_species_ends_rule_name(StandardCounterpointFirstSpeciesEndsRule which) {
    return standard_counterpoint_rule_table_name(&TABLE, (int)which);
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

typedef StandardCounterpointPairCantusTonic Subject;

static bool holds_adapter(int which, const void *subject) {
    const Subject *s = subject;
    return standard_counterpoint_first_species_ends_rule_holds((StandardCounterpointFirstSpeciesEndsRule)which,
                                                               s->pair, s->cantus_part, s->tonic);
}

STANDARD_COUNTERPOINT_RULE_TABLE_EVALUATOR(evaluate_candidate, TABLE, holds_adapter, Subject)
STANDARD_COUNTERPOINT_RULE_TABLE_SCORER(score_candidate, TABLE, holds_adapter, Subject)

bool standard_counterpoint_first_species_ends_check(const StandardCounterpointPair *pair,
                                                    StandardCounterpointCantusPart cantus_part,
                                                    const PitchSpelling *tonic, unsigned *violations) {
    if (violations == NULL || !standard_counterpoint_pair_valid(pair) || tonic == NULL ||
        (cantus_part != STANDARD_COUNTERPOINT_CANTUS_IN_LOWER &&
         cantus_part != STANDARD_COUNTERPOINT_CANTUS_IN_UPPER)) {
        return false;
    }
    Subject subject = {pair, cantus_part, tonic};
    unsigned unused_soft;
    standard_counterpoint_rule_table_check(&TABLE, holds_adapter, &subject, violations, &unused_soft);
    return true;
}
