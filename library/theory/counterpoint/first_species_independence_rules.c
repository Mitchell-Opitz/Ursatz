#include "theory/counterpoint/first_species_independence_rules.h"

#include <stddef.h>

#include "theory/tonality/interval.h"
#include "model/pitch/pitch_arithmetic.h"
#include "model/pitch/pitch_spelling.h"
#include "theory/counterpoint/counterpoint_subject.h"
#include "theory/counterpoint/framework.h"
#include "theory/rules/rule_table.h"

static const char *const NAMES[STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_COUNT] = {
    "single-repeat", "no-repeat", "no-overlap", "emergency-gap"};

static bool rule_is_hard(StandardCounterpointFirstSpeciesIndependenceRule which) {
    return which == STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_SINGLE_REPEAT;
}

static bool table_is_hard(int which) {
    return rule_is_hard((StandardCounterpointFirstSpeciesIndependenceRule)which);
}

static RulePair g_pairs[STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_COUNT];
static UrsatzOnce g_built[STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_COUNT];
static ConstraintEvalResult evaluate_candidate(const Constraint *constraint, const Value *candidate,
                                               const Context *context);
static PreferenceScore score_candidate(const Preference *preference, const Value *candidate, const Context *context);
static const RuleTable TABLE = {
    "standard-counterpoint", standard_counterpoint_framework, "standard-counterpoint",
    "first-species-independence", NAMES, STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_COUNT, table_is_hard,
    g_pairs, g_built, evaluate_candidate, score_candidate};

static bool valid_rule(StandardCounterpointFirstSpeciesIndependenceRule which) {
    return rule_table_valid(&TABLE, (int)which);
}

static size_t repeated_notes(const StandardCounterpointLine *line) {
    size_t repeats = 0;
    for (size_t i = 0; i + 1 < line->count; i++) {
        if (pitch_spelling_equals(line->notes[i], line->notes[i + 1])) {
            repeats++;
        }
    }
    return repeats;
}

static bool check_no_overlap(const StandardCounterpointPair *pair) {
    for (size_t i = 0; i + 1 < pair->lower->count; i++) {
        if (pitch_spelling_direction(pair->lower->notes[i], pair->upper->notes[i + 1]) < 0) {
            return false;
        }
        if (pitch_spelling_direction(pair->upper->notes[i], pair->lower->notes[i + 1]) > 0) {
            return false;
        }
    }
    return true;
}

static bool check_emergency_gap(const StandardCounterpointPair *pair) {
    size_t wide = 0;
    for (size_t i = 0; i < pair->lower->count; i++) {
        StandardTonalityInterval interval;
        if (!standard_tonality_interval_between(pair->lower->notes[i], pair->upper->notes[i], &interval)) {
            return false;
        }
        if (interval.size > STANDARD_COUNTERPOINT_FIRST_SPECIES_TENTH) {
            wide++;
        }
    }
    return wide <= STANDARD_COUNTERPOINT_FIRST_SPECIES_MAX_EMERGENCY_NOTES;
}

bool standard_counterpoint_first_species_independence_rule_holds(
    StandardCounterpointFirstSpeciesIndependenceRule which, const StandardCounterpointPair *pair,
    StandardCounterpointCantusPart cantus_part) {
    if (!valid_rule(which) || !standard_counterpoint_pair_valid(pair) ||
        (cantus_part != STANDARD_COUNTERPOINT_CANTUS_IN_LOWER &&
         cantus_part != STANDARD_COUNTERPOINT_CANTUS_IN_UPPER)) {
        return false;
    }
    const StandardCounterpointLine *counterpoint =
        cantus_part == STANDARD_COUNTERPOINT_CANTUS_IN_LOWER ? pair->upper : pair->lower;
    switch (which) {
        case STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_SINGLE_REPEAT:
            return repeated_notes(counterpoint) <= 1;
        case STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_NO_REPEAT:
            return repeated_notes(counterpoint) == 0;
        case STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_NO_OVERLAP:
            return check_no_overlap(pair);
        case STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_EMERGENCY_GAP:
            return check_emergency_gap(pair);
        default:
            return false;
    }
}

typedef StandardCounterpointPairCantus Subject;

static bool holds_adapter(int which, const void *subject) {
    const Subject *s = subject;
    return standard_counterpoint_first_species_independence_rule_holds(
        (StandardCounterpointFirstSpeciesIndependenceRule)which, s->pair, s->cantus_part);
}

RULE_TABLE_EVALUATOR(evaluate_candidate, TABLE, holds_adapter, Subject)
RULE_TABLE_SCORER(score_candidate, TABLE, holds_adapter, Subject)

bool standard_counterpoint_first_species_independence_check(const StandardCounterpointPair *pair,
                                                            StandardCounterpointCantusPart cantus_part,
                                                            unsigned *hard_violations, unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL || !standard_counterpoint_pair_valid(pair) ||
        (cantus_part != STANDARD_COUNTERPOINT_CANTUS_IN_LOWER &&
         cantus_part != STANDARD_COUNTERPOINT_CANTUS_IN_UPPER)) {
        return false;
    }
    Subject subject = {pair, cantus_part};
    rule_table_check(&TABLE, holds_adapter, &subject, hard_violations,
                                           soft_violations);
    return true;
}
