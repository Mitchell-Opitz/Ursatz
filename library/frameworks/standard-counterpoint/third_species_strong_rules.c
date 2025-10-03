#include "third_species_strong_rules.h"

#include <stddef.h>

#include "consonance_rules.h"
#include "counterpoint_pitch.h"
#include "motion_rules.h"
#include "rule_table.h"

static const char *const NAMES[STANDARD_COUNTERPOINT_THIRD_SPECIES_STRONG_COUNT] = {
    "consonant",
    "imperfect-preferred",
    "no-strong-unison",
    "no-three-perfect",
    "max-same-imperfect-run",
    "no-dissonant-outline",
    "previous-bar-perfects",
    "bar-line-repeated-perfect",
    "bar-line-direct-perfect"};

static bool rule_is_soft(StandardCounterpointThirdSpeciesStrongRule which) {
    return which == STANDARD_COUNTERPOINT_THIRD_SPECIES_STRONG_IMPERFECT_PREFERRED ||
           which == STANDARD_COUNTERPOINT_THIRD_SPECIES_STRONG_NO_STRONG_UNISON;
}

static bool table_is_hard(int which) {
    return !rule_is_soft((StandardCounterpointThirdSpeciesStrongRule)which);
}

static StandardCounterpointRulePair g_pairs[STANDARD_COUNTERPOINT_THIRD_SPECIES_STRONG_COUNT];
static bool g_built[STANDARD_COUNTERPOINT_THIRD_SPECIES_STRONG_COUNT];
static ConstraintEvalResult evaluate_candidate(const Constraint *constraint, const Value *candidate,
                                               const Context *context);
static const StandardCounterpointRuleTable TABLE = {
    "third-species-strong", NAMES, STANDARD_COUNTERPOINT_THIRD_SPECIES_STRONG_COUNT, table_is_hard, g_pairs, g_built,
    evaluate_candidate};

static bool valid_rule(StandardCounterpointThirdSpeciesStrongRule which) {
    return standard_counterpoint_rule_table_valid(&TABLE, (int)which);
}

const Rule *standard_counterpoint_third_species_strong_rule(StandardCounterpointThirdSpeciesStrongRule which) {
    return standard_counterpoint_rule_table_rule(&TABLE, (int)which);
}

const Constraint *standard_counterpoint_third_species_strong_constraint(StandardCounterpointThirdSpeciesStrongRule which) {
    return standard_counterpoint_rule_table_constraint(&TABLE, (int)which);
}

const char *standard_counterpoint_third_species_strong_rule_name(StandardCounterpointThirdSpeciesStrongRule which) {
    return standard_counterpoint_rule_table_name(&TABLE, (int)which);
}

typedef StandardCounterpointThirdSpeciesExercise Exercise;

static bool category_of(const StandardCounterpointInterval *interval, StandardCounterpointIntervalCategory *category) {
    return standard_counterpoint_classify_interval(interval, category);
}

static bool is_perfect_consonance(const StandardCounterpointInterval *interval) {
    StandardCounterpointIntervalCategory category;
    return category_of(interval, &category) && category == STANDARD_COUNTERPOINT_PERFECT_CONSONANCE;
}

/* A perfect unison or octave, simple or compound. */
static bool is_octave_class(const StandardCounterpointInterval *interval) {
    return interval->quality == STANDARD_COUNTERPOINT_QUALITY_PERFECT &&
           (interval->simple_size == 1 || interval->simple_size == 8);
}

static bool is_perfect_fifth(const StandardCounterpointInterval *interval) {
    return interval->quality == STANDARD_COUNTERPOINT_QUALITY_PERFECT && interval->simple_size == 5;
}

static bool downbeat_interval(const Exercise *exercise, size_t bar, StandardCounterpointInterval *out) {
    return standard_counterpoint_third_species_beat_interval(exercise, bar, 0, out);
}

static bool check_consonant(const Exercise *exercise) {
    for (size_t bar = 0; bar < exercise->cantus->count; bar++) {
        StandardCounterpointInterval interval;
        if (standard_counterpoint_third_species_beat(exercise, bar, 0) == NULL) {
            continue;
        }
        StandardCounterpointIntervalCategory category;
        if (!downbeat_interval(exercise, bar, &interval) || !category_of(&interval, &category) ||
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
        if (!downbeat_interval(exercise, bar, &interval) || !category_of(&interval, &category) ||
            category == STANDARD_COUNTERPOINT_PERFECT_CONSONANCE) {
            return false;
        }
    }
    return true;
}

static bool check_no_strong_unison(const Exercise *exercise) {
    for (size_t bar = 1; bar + 1 < exercise->cantus->count; bar++) {
        StandardCounterpointInterval interval;
        if (!downbeat_interval(exercise, bar, &interval) || interval.size == 1) {
            return false;
        }
    }
    return true;
}

static bool check_no_three_perfect(const Exercise *exercise) {
    size_t run = 0;
    int previous_size = 0;
    for (size_t bar = 0; bar < exercise->cantus->count; bar++) {
        StandardCounterpointInterval interval;
        if (!downbeat_interval(exercise, bar, &interval) || !is_perfect_consonance(&interval)) {
            run = 0;
            continue;
        }
        run = (run > 0 && interval.simple_size == previous_size) ? run + 1 : 1;
        previous_size = interval.simple_size;
        if (run > STANDARD_COUNTERPOINT_THIRD_SPECIES_MAX_PERFECT_RUN) {
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
        if (!downbeat_interval(exercise, bar, &interval) || !category_of(&interval, &category) ||
            category != STANDARD_COUNTERPOINT_IMPERFECT_CONSONANCE) {
            run = 0;
            continue;
        }
        run = (run > 0 && interval.simple_size == previous_size) ? run + 1 : 1;
        previous_size = interval.simple_size;
        if (run > STANDARD_COUNTERPOINT_THIRD_SPECIES_MAX_IMPERFECT_RUN) {
            return false;
        }
    }
    return true;
}

static bool check_no_dissonant_outline(const Exercise *exercise) {
    for (size_t bar = 0; bar + 1 < exercise->cantus->count; bar++) {
        const PitchSpelling *from = standard_counterpoint_third_species_beat(exercise, bar, 0);
        const PitchSpelling *to = standard_counterpoint_third_species_beat(exercise, bar + 1, 0);
        StandardCounterpointInterval outline;
        if (from == NULL || to == NULL) {
            continue;
        }
        if (!standard_counterpoint_interval_between(from, to, &outline) ||
            !standard_counterpoint_interval_melodic_consonant(&outline)) {
            return false;
        }
    }
    return true;
}

static bool check_previous_bar_perfects(const Exercise *exercise) {
    for (size_t bar = 0; bar + 1 < exercise->cantus->count; bar++) {
        StandardCounterpointInterval next;
        if (!downbeat_interval(exercise, bar + 1, &next)) {
            return false;
        }
        bool next_fifth = is_perfect_fifth(&next);
        bool next_octave = is_octave_class(&next);
        if (!next_fifth && !next_octave) {
            continue;
        }
        /* Beats 2, 3, and 4 are 1, 2, and 3 from zero. A fifth rules out
           beats 3 and 4; an octave rules out beats 2, 3, and 4. */
        for (size_t beat = next_octave ? 1 : 2; beat < STANDARD_COUNTERPOINT_THIRD_SPECIES_SLOTS_PER_BAR; beat++) {
            StandardCounterpointInterval earlier;
            if (!standard_counterpoint_third_species_beat_interval(exercise, bar, beat, &earlier)) {
                return false;
            }
            if ((next_fifth && is_perfect_fifth(&earlier)) || (next_octave && is_octave_class(&earlier))) {
                return false;
            }
        }
    }
    return true;
}

static bool check_bar_line_repeated_perfect(const Exercise *exercise) {
    for (size_t bar = 0; bar + 1 < exercise->cantus->count; bar++) {
        StandardCounterpointInterval last, next;
        if (!standard_counterpoint_third_species_beat_interval(exercise, bar, 3, &last) ||
            !downbeat_interval(exercise, bar + 1, &next)) {
            return false;
        }
        if (is_perfect_consonance(&last) && is_perfect_consonance(&next) && last.simple_size == next.simple_size) {
            return false;
        }
    }
    return true;
}

static bool check_bar_line_direct_perfect(const Exercise *exercise) {
    bool cantus_lower = exercise->cantus_part == STANDARD_COUNTERPOINT_CANTUS_IN_LOWER;
    for (size_t bar = 0; bar + 1 < exercise->cantus->count; bar++) {
        StandardCounterpointInterval next;
        const PitchSpelling *beat_four = standard_counterpoint_third_species_beat(exercise, bar, 3);
        const PitchSpelling *downbeat = standard_counterpoint_third_species_beat(exercise, bar + 1, 0);
        const PitchSpelling *cantus_from = exercise->cantus->notes[bar];
        const PitchSpelling *cantus_to = exercise->cantus->notes[bar + 1];
        if (!downbeat_interval(exercise, bar + 1, &next)) {
            return false;
        }
        if (!is_perfect_consonance(&next)) {
            continue;
        }
        MotionType motion;
        bool classified = cantus_lower
                              ? standard_counterpoint_classify_motion(cantus_from, cantus_to, beat_four, downbeat, &motion)
                              : standard_counterpoint_classify_motion(beat_four, downbeat, cantus_from, cantus_to, &motion);
        if (!classified) {
            return false;
        }
        if (motion_type_is_same_direction(motion)) {
            return false;
        }
    }
    return true;
}

bool standard_counterpoint_third_species_strong_rule_holds(StandardCounterpointThirdSpeciesStrongRule which,
                                                           const StandardCounterpointThirdSpeciesExercise *exercise) {
    if (!valid_rule(which) || !standard_counterpoint_third_species_exercise_valid(exercise)) {
        return false;
    }
    switch (which) {
        case STANDARD_COUNTERPOINT_THIRD_SPECIES_STRONG_CONSONANT:
            return check_consonant(exercise);
        case STANDARD_COUNTERPOINT_THIRD_SPECIES_STRONG_IMPERFECT_PREFERRED:
            return check_imperfect_preferred(exercise);
        case STANDARD_COUNTERPOINT_THIRD_SPECIES_STRONG_NO_STRONG_UNISON:
            return check_no_strong_unison(exercise);
        case STANDARD_COUNTERPOINT_THIRD_SPECIES_STRONG_NO_THREE_PERFECT:
            return check_no_three_perfect(exercise);
        case STANDARD_COUNTERPOINT_THIRD_SPECIES_STRONG_MAX_SAME_IMPERFECT_RUN:
            return check_max_same_imperfect_run(exercise);
        case STANDARD_COUNTERPOINT_THIRD_SPECIES_STRONG_NO_DISSONANT_OUTLINE:
            return check_no_dissonant_outline(exercise);
        case STANDARD_COUNTERPOINT_THIRD_SPECIES_STRONG_PREVIOUS_BAR_PERFECTS:
            return check_previous_bar_perfects(exercise);
        case STANDARD_COUNTERPOINT_THIRD_SPECIES_STRONG_BAR_LINE_REPEATED_PERFECT:
            return check_bar_line_repeated_perfect(exercise);
        case STANDARD_COUNTERPOINT_THIRD_SPECIES_STRONG_BAR_LINE_DIRECT_PERFECT:
            return check_bar_line_direct_perfect(exercise);
        default:
            return false;
    }
}

static bool holds_adapter(int which, const void *subject) {
    return standard_counterpoint_third_species_strong_rule_holds((StandardCounterpointThirdSpeciesStrongRule)which,
                                                                 subject);
}

STANDARD_COUNTERPOINT_RULE_TABLE_EVALUATOR(evaluate_candidate, TABLE, holds_adapter, StandardCounterpointThirdSpeciesExercise)

bool standard_counterpoint_third_species_strong_check(const StandardCounterpointThirdSpeciesExercise *exercise,
                                                      unsigned *hard_violations, unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL ||
        !standard_counterpoint_third_species_exercise_valid(exercise)) {
        return false;
    }
    standard_counterpoint_rule_table_check(&TABLE, holds_adapter, exercise, hard_violations,
                                           soft_violations);
    return true;
}
