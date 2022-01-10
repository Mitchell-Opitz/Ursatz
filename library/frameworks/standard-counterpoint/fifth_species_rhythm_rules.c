#include "fifth_species_rhythm_rules.h"

#include <stddef.h>

#include "counterpoint_interval.h"
#include "fifth_species_pattern.h"
#include "rule_table.h"

static const char *const NAMES[STANDARD_COUNTERPOINT_FIFTH_SPECIES_RHYTHM_RULE_COUNT] = {
    "eighths-stepwise", "mixture", "no-long-run", "eighths-sparing"};

static bool rule_is_hard(StandardCounterpointFifthSpeciesRhythmRule which) {
    return which == STANDARD_COUNTERPOINT_FIFTH_SPECIES_RHYTHM_EIGHTHS_STEPWISE;
}

static bool table_is_hard(int which) {
    return rule_is_hard((StandardCounterpointFifthSpeciesRhythmRule)which);
}

static StandardCounterpointRulePair g_pairs[STANDARD_COUNTERPOINT_FIFTH_SPECIES_RHYTHM_RULE_COUNT];
static bool g_built[STANDARD_COUNTERPOINT_FIFTH_SPECIES_RHYTHM_RULE_COUNT];
static const StandardCounterpointRuleTable TABLE = {
    "fifth-species-rhythm", NAMES, STANDARD_COUNTERPOINT_FIFTH_SPECIES_RHYTHM_RULE_COUNT, table_is_hard,
    g_pairs, g_built};

static bool valid_rule(StandardCounterpointFifthSpeciesRhythmRule which) {
    return standard_counterpoint_rule_table_valid(&TABLE, (int)which);
}

const Rule *standard_counterpoint_fifth_species_rhythm_rule(StandardCounterpointFifthSpeciesRhythmRule which) {
    return standard_counterpoint_rule_table_rule(&TABLE, (int)which);
}

const Constraint *standard_counterpoint_fifth_species_rhythm_constraint(StandardCounterpointFifthSpeciesRhythmRule which) {
    return standard_counterpoint_rule_table_constraint(&TABLE, (int)which);
}

const char *standard_counterpoint_fifth_species_rhythm_rule_name(StandardCounterpointFifthSpeciesRhythmRule which) {
    return standard_counterpoint_rule_table_name(&TABLE, (int)which);
}

typedef StandardCounterpointFifthSpeciesExercise Exercise;

static bool check_eighths_stepwise(const Exercise *exercise) {
    StandardCounterpointFifthSpeciesNote notes[STANDARD_COUNTERPOINT_FIFTH_SPECIES_MAX_NOTES];
    size_t count;
    if (!standard_counterpoint_fifth_species_notes(exercise, notes, STANDARD_COUNTERPOINT_FIFTH_SPECIES_MAX_NOTES, &count)) {
        return false;
    }
    for (size_t n = 0; n < count; n++) {
        if (notes[n].duration != 1) {
            continue;
        }
        for (size_t side = 0; side < 2; side++) {
            if ((side == 0 && n == 0) || (side == 1 && n + 1 >= count)) {
                continue;
            }
            const PitchSpelling *from = side == 0 ? notes[n - 1].pitch : notes[n].pitch;
            const PitchSpelling *to = side == 0 ? notes[n].pitch : notes[n + 1].pitch;
            StandardCounterpointInterval interval;
            if (!standard_counterpoint_interval_between(from, to, &interval) || interval.size > 2) {
                return false;
            }
        }
    }
    return true;
}

static bool check_mixture(const Exercise *exercise) {
    size_t counts[STANDARD_COUNTERPOINT_NOTE_VALUE_COUNT];
    if (!standard_counterpoint_fifth_species_value_counts(exercise, counts)) {
        return false;
    }
    size_t used = 0;
    for (int i = 0; i < STANDARD_COUNTERPOINT_NOTE_VALUE_COUNT; i++) {
        used += counts[i] > 0 ? 1 : 0;
    }
    return exercise->cantus->count < STANDARD_COUNTERPOINT_FIFTH_SPECIES_RHYTHM_MIN_BARS || used >= 3;
}

static bool check_no_long_run(const Exercise *exercise) {
    size_t run = 0;
    StandardCounterpointBarPattern previous = STANDARD_COUNTERPOINT_BAR_PATTERN_OTHER;
    for (size_t bar = 1; bar + 1 < exercise->cantus->count; bar++) {
        StandardCounterpointBarPattern pattern;
        if (!standard_counterpoint_fifth_species_bar_pattern(exercise, bar, &pattern)) {
            return false;
        }
        if (pattern == STANDARD_COUNTERPOINT_BAR_PATTERN_SYNCOPATED) {
            run = 0;
            continue;
        }
        run = run > 0 && pattern == previous ? run + 1 : 1;
        previous = pattern;
        if (run > STANDARD_COUNTERPOINT_FIFTH_SPECIES_RHYTHM_MAX_RUN) {
            return false;
        }
    }
    return true;
}

static bool check_eighths_sparing(const Exercise *exercise) {
    bool previous_had = false;
    for (size_t bar = 0; bar < exercise->cantus->count; bar++) {
        StandardCounterpointBarPattern pattern;
        if (!standard_counterpoint_fifth_species_bar_pattern(exercise, bar, &pattern)) {
            return false;
        }
        bool has = pattern == STANDARD_COUNTERPOINT_BAR_PATTERN_EIGHTHS;
        if (has && previous_had) {
            return false;
        }
        previous_had = has;
    }
    return true;
}

bool standard_counterpoint_fifth_species_rhythm_rule_holds(
    StandardCounterpointFifthSpeciesRhythmRule which, const StandardCounterpointFifthSpeciesExercise *exercise) {
    if (!valid_rule(which) || !standard_counterpoint_fifth_species_exercise_valid(exercise)) {
        return false;
    }
    switch (which) {
        case STANDARD_COUNTERPOINT_FIFTH_SPECIES_RHYTHM_EIGHTHS_STEPWISE:
            return check_eighths_stepwise(exercise);
        case STANDARD_COUNTERPOINT_FIFTH_SPECIES_RHYTHM_MIXTURE:
            return check_mixture(exercise);
        case STANDARD_COUNTERPOINT_FIFTH_SPECIES_RHYTHM_NO_LONG_RUN:
            return check_no_long_run(exercise);
        case STANDARD_COUNTERPOINT_FIFTH_SPECIES_RHYTHM_EIGHTHS_SPARING:
            return check_eighths_sparing(exercise);
        default:
            return false;
    }
}

static bool holds_adapter(int which, const void *subject) {
    return standard_counterpoint_fifth_species_rhythm_rule_holds((StandardCounterpointFifthSpeciesRhythmRule)which,
                                                                 subject);
}

bool standard_counterpoint_fifth_species_rhythm_check(const StandardCounterpointFifthSpeciesExercise *exercise,
                                                           unsigned *hard_violations, unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL ||
        !standard_counterpoint_fifth_species_exercise_valid(exercise)) {
        return false;
    }
    standard_counterpoint_rule_table_check(&TABLE, holds_adapter, exercise, hard_violations,
                                           soft_violations);
    return true;
}
