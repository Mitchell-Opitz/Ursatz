#include "cantus_firmus_rules.h"

#include <stddef.h>

#include "consonance_rules.h"
#include "counterpoint_interval.h"
#include "counterpoint_pitch.h"
#include "counterpoint_subject.h"
#include "rule_table.h"

static const char *const NAMES[STANDARD_COUNTERPOINT_CANTUS_RULE_COUNT] = {
    "length",        "tonic-ends",        "final-step",   "melodic-consonance", "range",
    "single-climax", "leap-recovery",     "consecutive-leaps", "leading-tone",  "mostly-stepwise",
    "no-repeated-motive"};

static bool rule_is_soft(StandardCounterpointCantusRule which) {
    return which == STANDARD_COUNTERPOINT_CANTUS_RULE_SINGLE_CLIMAX ||
           which == STANDARD_COUNTERPOINT_CANTUS_RULE_MOSTLY_STEPWISE ||
           which == STANDARD_COUNTERPOINT_CANTUS_RULE_NO_REPEATED_MOTIVE;
}

static bool table_is_hard(int which) {
    return !rule_is_soft((StandardCounterpointCantusRule)which);
}

static StandardCounterpointRulePair g_pairs[STANDARD_COUNTERPOINT_CANTUS_RULE_COUNT];
static bool g_built[STANDARD_COUNTERPOINT_CANTUS_RULE_COUNT];
static ConstraintEvalResult evaluate_candidate(const Constraint *constraint, const Value *candidate,
                                               const Context *context);
static const StandardCounterpointRuleTable TABLE = {
    "cantus-firmus", NAMES, STANDARD_COUNTERPOINT_CANTUS_RULE_COUNT, table_is_hard, g_pairs, g_built,
    evaluate_candidate};

static bool valid_rule(StandardCounterpointCantusRule which) {
    return standard_counterpoint_rule_table_valid(&TABLE, (int)which);
}

const Rule *standard_counterpoint_cantus_rule(StandardCounterpointCantusRule which) {
    return standard_counterpoint_rule_table_rule(&TABLE, (int)which);
}

const Constraint *standard_counterpoint_cantus_constraint(StandardCounterpointCantusRule which) {
    return standard_counterpoint_rule_table_constraint(&TABLE, (int)which);
}

const char *standard_counterpoint_cantus_rule_name(StandardCounterpointCantusRule which) {
    return standard_counterpoint_rule_table_name(&TABLE, (int)which);
}

static bool same_pitch_class(const PitchSpelling *a, const PitchSpelling *b) {
    return pitch_spelling_letter(a) == pitch_spelling_letter(b) &&
           pitch_spelling_accidental(a) == pitch_spelling_accidental(b);
}

/* The interval between notes i and i + 1. */
static bool interval_at(const StandardCounterpointLine *line, size_t i, StandardCounterpointInterval *out) {
    return standard_counterpoint_interval_between(line->notes[i], line->notes[i + 1], out);
}

static int direction_at(const StandardCounterpointLine *line, size_t i) {
    return standard_counterpoint_direction(line->notes[i], line->notes[i + 1]);
}

static bool is_leap_at(const StandardCounterpointLine *line, size_t i) {
    StandardCounterpointInterval interval;
    return interval_at(line, i, &interval) && standard_counterpoint_interval_is_leap(&interval);
}

static bool is_step_at(const StandardCounterpointLine *line, size_t i) {
    StandardCounterpointInterval interval;
    return interval_at(line, i, &interval) && standard_counterpoint_interval_is_step(&interval);
}

static const PitchSpelling *highest_note(const StandardCounterpointLine *line) {
    const PitchSpelling *best = line->notes[0];
    for (size_t i = 1; i < line->count; i++) {
        if (standard_counterpoint_direction(best, line->notes[i]) > 0) {
            best = line->notes[i];
        }
    }
    return best;
}

static const PitchSpelling *lowest_note(const StandardCounterpointLine *line) {
    const PitchSpelling *best = line->notes[0];
    for (size_t i = 1; i < line->count; i++) {
        if (standard_counterpoint_direction(best, line->notes[i]) < 0) {
            best = line->notes[i];
        }
    }
    return best;
}

static bool check_length(const StandardCounterpointLine *line) {
    return line->count >= STANDARD_COUNTERPOINT_CANTUS_MIN_LENGTH && line->count <= STANDARD_COUNTERPOINT_CANTUS_MAX_LENGTH;
}

static bool check_tonic_ends(const StandardCounterpointLine *line, const PitchSpelling *tonic) {
    return same_pitch_class(line->notes[0], tonic) && same_pitch_class(line->notes[line->count - 1], tonic);
}

static bool check_final_step(const StandardCounterpointLine *line) {
    if (line->count < 2) {
        return false;
    }
    size_t last = line->count - 2;
    StandardCounterpointInterval interval;
    if (!interval_at(line, last, &interval) || !standard_counterpoint_interval_is_step(&interval)) {
        return false;
    }
    if (interval.quality != STANDARD_COUNTERPOINT_QUALITY_MAJOR &&
        interval.quality != STANDARD_COUNTERPOINT_QUALITY_MINOR) {
        return false;
    }
    /* From below the approach is the leading tone, a half step. */
    if (direction_at(line, last) > 0) {
        return interval.quality == STANDARD_COUNTERPOINT_QUALITY_MINOR;
    }
    return true;
}

static bool check_melodic_consonance(const StandardCounterpointLine *line) {
    for (size_t i = 0; i + 1 < line->count; i++) {
        StandardCounterpointInterval interval;
        if (!interval_at(line, i, &interval) || !standard_counterpoint_interval_melodic_consonant(&interval)) {
            return false;
        }
    }
    return true;
}

static bool check_range(const StandardCounterpointLine *line) {
    StandardCounterpointInterval interval;
    return standard_counterpoint_interval_between(lowest_note(line), highest_note(line), &interval) &&
           interval.size <= STANDARD_COUNTERPOINT_CANTUS_MAX_RANGE;
}

static bool check_single_climax(const StandardCounterpointLine *line) {
    const PitchSpelling *top = highest_note(line);
    size_t occurrences = 0;
    for (size_t i = 0; i < line->count; i++) {
        if (standard_counterpoint_direction(line->notes[i], top) == 0) {
            occurrences++;
        }
    }
    return occurrences == 1;
}

static bool check_leap_recovery(const StandardCounterpointLine *line) {
    for (size_t i = 0; i + 2 < line->count; i++) {
        StandardCounterpointInterval interval;
        if (!interval_at(line, i, &interval)) {
            return false;
        }
        if (interval.size >= 4) {
            if (!is_step_at(line, i + 1) || direction_at(line, i + 1) == direction_at(line, i)) {
                return false;
            }
        }
    }
    return true;
}

/* Two same-direction leaps, each a third, that together span a perfect
   fifth: the notes outline a consonant triad. */
static bool outlines_triad(const StandardCounterpointLine *line, size_t i) {
    StandardCounterpointInterval first, second, outer;
    if (!interval_at(line, i, &first) || !interval_at(line, i + 1, &second) ||
        !standard_counterpoint_interval_between(line->notes[i], line->notes[i + 2], &outer)) {
        return false;
    }
    return first.size == 3 && second.size == 3 && outer.size == 5 &&
           outer.quality == STANDARD_COUNTERPOINT_QUALITY_PERFECT;
}

static bool check_consecutive_leaps(const StandardCounterpointLine *line) {
    for (size_t i = 0; i + 2 < line->count; i++) {
        if (!is_leap_at(line, i) || !is_leap_at(line, i + 1)) {
            continue;
        }
        if (i + 3 < line->count && is_leap_at(line, i + 2)) {
            return false;
        }
        if (direction_at(line, i) == direction_at(line, i + 1) && !outlines_triad(line, i)) {
            return false;
        }
    }
    return true;
}

/* The seventh degree is the note on the letter just below the tonic's. It
   is a leading tone when a half step below the tonic, and unraised when a
   whole step below. */
static bool check_leading_tone(const StandardCounterpointLine *line, const PitchSpelling *tonic) {
    int tonic_step = standard_counterpoint_step_number(tonic);
    int tonic_semitone = standard_counterpoint_semitone_number(tonic);
    for (size_t i = 0; i < line->count; i++) {
        const PitchSpelling *note = line->notes[i];
        int letters_above = (((standard_counterpoint_step_number(note) - tonic_step) % 7) + 7) % 7;
        if (letters_above != 6) {
            continue;
        }
        int below = ((((tonic_semitone - standard_counterpoint_semitone_number(note)) % 12) + 12) % 12);
        bool leading = below == 1;
        bool unraised = below == 2;
        if (i + 1 >= line->count) {
            continue;
        }
        bool tonic_next = same_pitch_class(line->notes[i + 1], tonic);
        bool steps_up = is_step_at(line, i) && direction_at(line, i) > 0;
        if (leading && !(tonic_next && steps_up)) {
            return false;
        }
        if (unraised && tonic_next && steps_up) {
            return false;
        }
    }
    return true;
}

static bool check_mostly_stepwise(const StandardCounterpointLine *line) {
    size_t intervals = line->count - 1;
    size_t steps = 0;
    for (size_t i = 0; i < intervals; i++) {
        if (is_step_at(line, i)) {
            steps++;
        }
    }
    return 2 * steps >= intervals;
}

#define MOTIVE_LENGTH 4

static bool motive_equal(const StandardCounterpointLine *line, size_t i, size_t j) {
    for (size_t k = 0; k < MOTIVE_LENGTH; k++) {
        if (!standard_counterpoint_same_pitch(line->notes[i + k], line->notes[j + k])) {
            return false;
        }
    }
    return true;
}

static bool check_no_repeated_motive(const StandardCounterpointLine *line) {
    for (size_t i = 0; i + MOTIVE_LENGTH <= line->count; i++) {
        for (size_t j = i + 1; j + MOTIVE_LENGTH <= line->count; j++) {
            if (motive_equal(line, i, j)) {
                return false;
            }
        }
    }
    return true;
}

bool standard_counterpoint_cantus_rule_holds(StandardCounterpointCantusRule which,
                                             const StandardCounterpointLine *line, const PitchSpelling *tonic) {
    if (!valid_rule(which) || !standard_counterpoint_line_valid(line) || tonic == NULL) {
        return false;
    }
    switch (which) {
        case STANDARD_COUNTERPOINT_CANTUS_RULE_LENGTH:
            return check_length(line);
        case STANDARD_COUNTERPOINT_CANTUS_RULE_TONIC_ENDS:
            return check_tonic_ends(line, tonic);
        case STANDARD_COUNTERPOINT_CANTUS_RULE_FINAL_STEP:
            return check_final_step(line);
        case STANDARD_COUNTERPOINT_CANTUS_RULE_MELODIC_CONSONANCE:
            return check_melodic_consonance(line);
        case STANDARD_COUNTERPOINT_CANTUS_RULE_RANGE:
            return check_range(line);
        case STANDARD_COUNTERPOINT_CANTUS_RULE_SINGLE_CLIMAX:
            return check_single_climax(line);
        case STANDARD_COUNTERPOINT_CANTUS_RULE_LEAP_RECOVERY:
            return check_leap_recovery(line);
        case STANDARD_COUNTERPOINT_CANTUS_RULE_CONSECUTIVE_LEAPS:
            return check_consecutive_leaps(line);
        case STANDARD_COUNTERPOINT_CANTUS_RULE_LEADING_TONE:
            return check_leading_tone(line, tonic);
        case STANDARD_COUNTERPOINT_CANTUS_RULE_MOSTLY_STEPWISE:
            return check_mostly_stepwise(line);
        case STANDARD_COUNTERPOINT_CANTUS_RULE_NO_REPEATED_MOTIVE:
            return check_no_repeated_motive(line);
        default:
            return false;
    }
}

typedef StandardCounterpointLineTonic Subject;

static bool holds_adapter(int which, const void *subject) {
    const Subject *s = subject;
    return standard_counterpoint_cantus_rule_holds((StandardCounterpointCantusRule)which, s->line, s->tonic);
}

STANDARD_COUNTERPOINT_RULE_TABLE_EVALUATOR(evaluate_candidate, TABLE, holds_adapter, Subject)

bool standard_counterpoint_cantus_check(const StandardCounterpointLine *line, const PitchSpelling *tonic,
                                        unsigned *hard_violations, unsigned *soft_violations) {
    if (!standard_counterpoint_line_valid(line) || tonic == NULL || hard_violations == NULL ||
        soft_violations == NULL) {
        return false;
    }
    Subject subject = {line, tonic};
    standard_counterpoint_rule_table_check(&TABLE, holds_adapter, &subject, hard_violations,
                                           soft_violations);
    return true;
}
