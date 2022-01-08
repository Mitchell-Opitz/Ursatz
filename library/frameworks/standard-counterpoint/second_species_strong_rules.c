#include "second_species_strong_rules.h"

#include <stddef.h>

#include "consonance_rules.h"
#include "counterpoint_pitch.h"
#include "rule_table.h"

static const char *const NAMES[STANDARD_COUNTERPOINT_SECOND_SPECIES_STRONG_COUNT] = {
    "consonant", "imperfect-preferred", "no-strong-unison", "no-repeated-perfect", "no-dissonant-outline",
    "max-same-imperfect-run"};

static bool rule_is_soft(StandardCounterpointSecondSpeciesStrongRule which) {
    return which == STANDARD_COUNTERPOINT_SECOND_SPECIES_STRONG_IMPERFECT_PREFERRED ||
           which == STANDARD_COUNTERPOINT_SECOND_SPECIES_STRONG_NO_STRONG_UNISON;
}

static bool table_is_hard(int which) {
    return !rule_is_soft((StandardCounterpointSecondSpeciesStrongRule)which);
}

static StandardCounterpointRulePair g_pairs[STANDARD_COUNTERPOINT_SECOND_SPECIES_STRONG_COUNT];
static bool g_built[STANDARD_COUNTERPOINT_SECOND_SPECIES_STRONG_COUNT];
static const StandardCounterpointRuleTable TABLE = {
    "second-species-strong", NAMES, STANDARD_COUNTERPOINT_SECOND_SPECIES_STRONG_COUNT, table_is_hard, g_pairs, g_built};

static bool valid_rule(StandardCounterpointSecondSpeciesStrongRule which) {
    return standard_counterpoint_rule_table_valid(&TABLE, (int)which);
}

const Rule *standard_counterpoint_second_species_strong_rule(StandardCounterpointSecondSpeciesStrongRule which) {
    return standard_counterpoint_rule_table_rule(&TABLE, (int)which);
}

const Constraint *standard_counterpoint_second_species_strong_constraint(StandardCounterpointSecondSpeciesStrongRule which) {
    return standard_counterpoint_rule_table_constraint(&TABLE, (int)which);
}

const char *standard_counterpoint_second_species_strong_rule_name(StandardCounterpointSecondSpeciesStrongRule which) {
    return standard_counterpoint_rule_table_name(&TABLE, (int)which);
}

typedef StandardCounterpointSecondSpeciesExercise Exercise;

static bool downbeat_category(const Exercise *exercise, size_t bar, StandardCounterpointInterval *interval,
                              StandardCounterpointIntervalCategory *category) {
    return standard_counterpoint_second_species_downbeat_interval(exercise, bar, interval) &&
           standard_counterpoint_classify_interval(interval, category);
}

static bool check_consonant(const Exercise *exercise) {
    for (size_t bar = 0; bar < exercise->cantus->count; bar++) {
        if (standard_counterpoint_second_species_downbeat(exercise, bar) == NULL) {
            continue;
        }
        StandardCounterpointInterval interval;
        StandardCounterpointIntervalCategory category;
        if (!downbeat_category(exercise, bar, &interval, &category) ||
            category == STANDARD_COUNTERPOINT_DISSONANCE) {
            return false;
        }
    }
    return true;
}

static bool check_imperfect_preferred(const Exercise *exercise) {
    for (size_t bar = 1; bar + 1 < exercise->cantus->count; bar++) {
        StandardCounterpointInterval interval;
        StandardCounterpointIntervalCategory category;
        if (!downbeat_category(exercise, bar, &interval, &category) ||
            category == STANDARD_COUNTERPOINT_PERFECT_CONSONANCE) {
            return false;
        }
    }
    return true;
}

static bool check_no_strong_unison(const Exercise *exercise) {
    for (size_t bar = 1; bar + 1 < exercise->cantus->count; bar++) {
        StandardCounterpointInterval interval;
        if (!standard_counterpoint_second_species_downbeat_interval(exercise, bar, &interval) || interval.size == 1) {
            return false;
        }
    }
    return true;
}

static bool is_perfect_consonance(const StandardCounterpointInterval *interval) {
    StandardCounterpointIntervalCategory category;
    return standard_counterpoint_classify_interval(interval, &category) &&
           category == STANDARD_COUNTERPOINT_PERFECT_CONSONANCE;
}

static bool check_no_repeated_perfect(const Exercise *exercise) {
    for (size_t bar = 0; bar + 1 < exercise->cantus->count; bar++) {
        StandardCounterpointInterval next;
        if (!standard_counterpoint_second_species_downbeat_interval(exercise, bar + 1, &next)) {
            return false;
        }
        if (!is_perfect_consonance(&next)) {
            continue;
        }
        StandardCounterpointInterval downbeat, weak;
        if (standard_counterpoint_second_species_downbeat_interval(exercise, bar, &downbeat) &&
            is_perfect_consonance(&downbeat) && downbeat.simple_size == next.simple_size) {
            return false;
        }
        if (standard_counterpoint_second_species_weak_interval(exercise, bar, &weak) && is_perfect_consonance(&weak) &&
            weak.simple_size == next.simple_size) {
            return false;
        }
    }
    return true;
}

/* A leap of an octave from the downbeat to the weak beat, then a step in
   the opposite direction. */
static bool octave_leap_then_step_back(const Exercise *exercise, size_t bar) {
    const PitchSpelling *downbeat = standard_counterpoint_second_species_downbeat(exercise, bar);
    const PitchSpelling *weak = standard_counterpoint_second_species_weak(exercise, bar);
    const PitchSpelling *next = standard_counterpoint_second_species_downbeat(exercise, bar + 1);
    if (downbeat == NULL || weak == NULL || next == NULL || !standard_counterpoint_second_species_weak_is_attack(exercise, bar)) {
        return false;
    }
    StandardCounterpointInterval leap, step;
    if (!standard_counterpoint_interval_between(downbeat, weak, &leap) ||
        !standard_counterpoint_interval_between(weak, next, &step)) {
        return false;
    }
    int out = standard_counterpoint_direction(downbeat, weak);
    int back = standard_counterpoint_direction(weak, next);
    return leap.size == 8 && leap.quality == STANDARD_COUNTERPOINT_QUALITY_PERFECT &&
           standard_counterpoint_interval_is_step(&step) && out != 0 && back != 0 && out != back;
}

static bool check_no_dissonant_outline(const Exercise *exercise) {
    for (size_t bar = 0; bar + 1 < exercise->cantus->count; bar++) {
        const PitchSpelling *from = standard_counterpoint_second_species_downbeat(exercise, bar);
        const PitchSpelling *to = standard_counterpoint_second_species_downbeat(exercise, bar + 1);
        if (from == NULL || to == NULL) {
            continue;
        }
        StandardCounterpointInterval outline;
        if (!standard_counterpoint_interval_between(from, to, &outline)) {
            return false;
        }
        if (!standard_counterpoint_interval_melodic_consonant(&outline) &&
            !octave_leap_then_step_back(exercise, bar)) {
            return false;
        }
    }
    return true;
}

static bool check_max_same_imperfect_run(const Exercise *exercise) {
    size_t run = 0;
    int previous_size = 0;
    for (size_t bar = 0; bar < exercise->cantus->count; bar++) {
        StandardCounterpointInterval interval;
        StandardCounterpointIntervalCategory category;
        if (standard_counterpoint_second_species_downbeat(exercise, bar) == NULL ||
            !downbeat_category(exercise, bar, &interval, &category) ||
            category != STANDARD_COUNTERPOINT_IMPERFECT_CONSONANCE) {
            run = 0;
            continue;
        }
        run = (run > 0 && interval.simple_size == previous_size) ? run + 1 : 1;
        previous_size = interval.simple_size;
        if (run > STANDARD_COUNTERPOINT_SECOND_SPECIES_MAX_IMPERFECT_RUN) {
            return false;
        }
    }
    return true;
}

bool standard_counterpoint_second_species_strong_rule_holds(StandardCounterpointSecondSpeciesStrongRule which,
                                                            const StandardCounterpointSecondSpeciesExercise *exercise) {
    if (!valid_rule(which) || !standard_counterpoint_second_species_exercise_valid(exercise)) {
        return false;
    }
    switch (which) {
        case STANDARD_COUNTERPOINT_SECOND_SPECIES_STRONG_CONSONANT:
            return check_consonant(exercise);
        case STANDARD_COUNTERPOINT_SECOND_SPECIES_STRONG_IMPERFECT_PREFERRED:
            return check_imperfect_preferred(exercise);
        case STANDARD_COUNTERPOINT_SECOND_SPECIES_STRONG_NO_STRONG_UNISON:
            return check_no_strong_unison(exercise);
        case STANDARD_COUNTERPOINT_SECOND_SPECIES_STRONG_NO_REPEATED_PERFECT:
            return check_no_repeated_perfect(exercise);
        case STANDARD_COUNTERPOINT_SECOND_SPECIES_STRONG_NO_DISSONANT_OUTLINE:
            return check_no_dissonant_outline(exercise);
        case STANDARD_COUNTERPOINT_SECOND_SPECIES_STRONG_MAX_SAME_IMPERFECT_RUN:
            return check_max_same_imperfect_run(exercise);
        default:
            return false;
    }
}

static bool holds_adapter(int which, const void *subject) {
    return standard_counterpoint_second_species_strong_rule_holds((StandardCounterpointSecondSpeciesStrongRule)which,
                                                                  subject);
}

bool standard_counterpoint_second_species_strong_check(const StandardCounterpointSecondSpeciesExercise *exercise,
                                                       unsigned *hard_violations, unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL ||
        !standard_counterpoint_second_species_exercise_valid(exercise)) {
        return false;
    }
    standard_counterpoint_rule_table_check(&TABLE, holds_adapter, exercise, hard_violations,
                                           soft_violations);
    return true;
}
