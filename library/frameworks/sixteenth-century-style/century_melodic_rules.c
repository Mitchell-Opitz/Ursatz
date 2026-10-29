#include "century_melodic_rules.h"

#include <stddef.h>
#include <stdlib.h>

#include "century_rule_table.h"
#include "standard-counterpoint/counterpoint_interval.h"
#include "standard-counterpoint/counterpoint_pitch.h"

static const char *const NAMES[SIXTEENTH_CENTURY_MELODIC_RULE_COUNT] = {
    "no-dissonant-leaps", "no-outlined-dissonance", "range-limit", "sixths",        "leap-order",
    "leap-recovery",      "no-triad-outline",       "arch",        "range-preferred", "final-step",
    "leading-note"};

static bool table_is_hard(int which) {
    return which <= SIXTEENTH_CENTURY_MELODIC_RANGE_LIMIT;
}

static SixteenthCenturyRulePair g_pairs[SIXTEENTH_CENTURY_MELODIC_RULE_COUNT];
static UrsatzOnce g_built[SIXTEENTH_CENTURY_MELODIC_RULE_COUNT];
static const SixteenthCenturyRuleTable TABLE = {"melodic", NAMES, SIXTEENTH_CENTURY_MELODIC_RULE_COUNT,
                                                table_is_hard, g_pairs, g_built};

const Rule *sixteenth_century_melodic_rule(SixteenthCenturyMelodicRule which) {
    return sixteenth_century_rule_table_rule(&TABLE, (int)which);
}

const Constraint *sixteenth_century_melodic_constraint(SixteenthCenturyMelodicRule which) {
    return sixteenth_century_rule_table_constraint(&TABLE, (int)which);
}

const char *sixteenth_century_melodic_rule_name(SixteenthCenturyMelodicRule which) {
    return sixteenth_century_rule_table_name(&TABLE, (int)which);
}

typedef struct {
    const PitchSpelling *pitches[SIXTEENTH_CENTURY_MAX_NOTES];
    size_t count;
} Melody;

static bool interval_of(const Melody *m, size_t i, StandardCounterpointInterval *out) {
    return standard_counterpoint_interval_between(m->pitches[i], m->pitches[i + 1], out);
}

static int direction_of(const Melody *m, size_t i) {
    return standard_counterpoint_direction(m->pitches[i], m->pitches[i + 1]);
}

static int semitones_of(const Melody *m, size_t i) {
    return abs(standard_counterpoint_semitone_number(m->pitches[i + 1]) -
               standard_counterpoint_semitone_number(m->pitches[i]));
}

/* True for a seventh, or an augmented or diminished interval. */
static bool is_dissonant_melodic(const StandardCounterpointInterval *interval) {
    return interval->simple_size == 7 || interval->quality == STANDARD_COUNTERPOINT_QUALITY_AUGMENTED ||
           interval->quality == STANDARD_COUNTERPOINT_QUALITY_DIMINISHED;
}

static bool check_no_dissonant_leaps(const Melody *m, const SixteenthCenturyScore *score) {
    (void)score;
    for (size_t i = 0; i + 1 < m->count; i++) {
        StandardCounterpointInterval interval;
        if (!interval_of(m, i, &interval) || is_dissonant_melodic(&interval)) {
            return false;
        }
    }
    return true;
}

static bool check_no_outlined_dissonance(const Melody *m, const SixteenthCenturyScore *score) {
    (void)score;
    size_t run_start = 0;
    for (size_t i = 0; i + 1 < m->count; i++) {
        int dir = direction_of(m, i);
        bool run_ends = i + 2 >= m->count || direction_of(m, i + 1) != dir;
        if (dir == 0) {
            run_start = i + 1;
            continue;
        }
        if (run_ends) {
            StandardCounterpointInterval interval;
            if (!standard_counterpoint_interval_between(m->pitches[run_start], m->pitches[i + 1], &interval) ||
                is_dissonant_melodic(&interval)) {
                return false;
            }
            run_start = i + 1;
        }
    }
    return true;
}

static bool check_range_limit(const Melody *m, const SixteenthCenturyScore *score) {
    (void)score;
    int low = standard_counterpoint_step_number(m->pitches[0]);
    int high = low;
    for (size_t i = 1; i < m->count; i++) {
        int step = standard_counterpoint_step_number(m->pitches[i]);
        low = step < low ? step : low;
        high = step > high ? step : high;
    }
    return high - low <= SIXTEENTH_CENTURY_MAX_RANGE_STEPS;
}

static bool check_sixths(const Melody *m, const SixteenthCenturyScore *score) {
    (void)score;
    for (size_t i = 0; i + 1 < m->count; i++) {
        StandardCounterpointInterval interval;
        if (!interval_of(m, i, &interval)) {
            return false;
        }
        if (interval.simple_size == 6 &&
            !(interval.size == 6 && interval.quality == STANDARD_COUNTERPOINT_QUALITY_MINOR &&
              direction_of(m, i) > 0)) {
            return false;
        }
    }
    return true;
}

static bool is_leap_at(const Melody *m, size_t i) {
    StandardCounterpointInterval interval;
    return interval_of(m, i, &interval) && standard_counterpoint_interval_is_leap(&interval);
}

static bool check_leap_order(const Melody *m, const SixteenthCenturyScore *score) {
    (void)score;
    for (size_t i = 0; i + 2 < m->count; i++) {
        int dir = direction_of(m, i);
        if (dir == 0 || dir != direction_of(m, i + 1) || !is_leap_at(m, i) || !is_leap_at(m, i + 1)) {
            continue;
        }
        int first = semitones_of(m, i);
        int second = semitones_of(m, i + 1);
        if (dir > 0 ? first < second : second < first) {
            return false;
        }
    }
    return true;
}

static bool check_leap_recovery(const Melody *m, const SixteenthCenturyScore *score) {
    (void)score;
    for (size_t i = 0; i + 2 < m->count; i++) {
        StandardCounterpointInterval interval;
        if (!interval_of(m, i, &interval)) {
            return false;
        }
        if (interval.size > 4 && direction_of(m, i + 1) != -direction_of(m, i)) {
            return false;
        }
    }
    return true;
}

static bool check_no_triad_outline(const Melody *m, const SixteenthCenturyScore *score) {
    (void)score;
    for (size_t i = 0; i + 2 < m->count; i++) {
        StandardCounterpointInterval a, b;
        if (!interval_of(m, i, &a) || !interval_of(m, i + 1, &b)) {
            return false;
        }
        if (a.size == 3 && b.size == 3 && direction_of(m, i) != 0 && direction_of(m, i) == direction_of(m, i + 1)) {
            return false;
        }
    }
    return true;
}

static bool check_arch(const Melody *m, const SixteenthCenturyScore *score) {
    (void)score;
    if (m->count < SIXTEENTH_CENTURY_ARCH_MIN_NOTES) {
        return true;
    }
    int high = standard_counterpoint_semitone_number(m->pitches[0]);
    size_t high_index = 0;
    for (size_t i = 1; i < m->count; i++) {
        int s = standard_counterpoint_semitone_number(m->pitches[i]);
        if (s > high) {
            high = s;
            high_index = i;
        }
    }
    return high_index != 0 && high_index != m->count - 1 &&
           standard_counterpoint_semitone_number(m->pitches[m->count - 1]) != high;
}

static bool check_range_preferred(const Melody *m, const SixteenthCenturyScore *score) {
    (void)score;
    int low = standard_counterpoint_semitone_number(m->pitches[0]);
    int high = low;
    for (size_t i = 1; i < m->count; i++) {
        int s = standard_counterpoint_semitone_number(m->pitches[i]);
        low = s < low ? s : low;
        high = s > high ? s : high;
    }
    return high - low <= SIXTEENTH_CENTURY_PREFERRED_RANGE_SEMITONES;
}

static bool check_final_step(const Melody *m, const SixteenthCenturyScore *score) {
    (void)score;
    if (m->count < 2) {
        return true;
    }
    StandardCounterpointInterval interval;
    return interval_of(m, m->count - 2, &interval) && interval.size <= 2;
}

static bool same_letter_and_accidental(const PitchSpelling *a, const PitchSpelling *b) {
    return pitch_spelling_letter(a) == pitch_spelling_letter(b) &&
           pitch_spelling_accidental(a) == pitch_spelling_accidental(b);
}

static bool check_leading_note(const Melody *m, const SixteenthCenturyScore *score) {
    if (score->tonic == NULL || score->phrygian || m->count < 2) {
        return true;
    }
    const PitchSpelling *last = m->pitches[m->count - 1];
    const PitchSpelling *before = m->pitches[m->count - 2];
    if (!same_letter_and_accidental(last, score->tonic)) {
        return true;
    }
    StandardCounterpointInterval interval;
    if (!interval_of(m, m->count - 2, &interval) || interval.size != 2 || direction_of(m, m->count - 2) <= 0) {
        return true;
    }
    return standard_counterpoint_semitone_number(last) - standard_counterpoint_semitone_number(before) == 1;
}

typedef bool (*MelodyCheck)(const Melody *m, const SixteenthCenturyScore *score);

static const MelodyCheck CHECKS[SIXTEENTH_CENTURY_MELODIC_RULE_COUNT] = {
    check_no_dissonant_leaps, check_no_outlined_dissonance, check_range_limit, check_sixths,
    check_leap_order,         check_leap_recovery,          check_no_triad_outline, check_arch,
    check_range_preferred,    check_final_step,             check_leading_note};

bool sixteenth_century_melodic_rule_holds(SixteenthCenturyMelodicRule which, const SixteenthCenturyScore *score) {
    if (!sixteenth_century_rule_table_valid(&TABLE, (int)which) || !sixteenth_century_score_valid(score)) {
        return false;
    }
    for (size_t voice = 0; voice < score->voice_count; voice++) {
        SixteenthCenturyNote notes[SIXTEENTH_CENTURY_MAX_NOTES];
        size_t count;
        if (!sixteenth_century_voice_notes(score, voice, notes, SIXTEENTH_CENTURY_MAX_NOTES, &count)) {
            return false;
        }
        if (count == 0) {
            continue;
        }
        Melody melody;
        melody.count = count;
        for (size_t i = 0; i < count; i++) {
            melody.pitches[i] = notes[i].pitch;
        }
        if (!CHECKS[which](&melody, score)) {
            return false;
        }
    }
    return true;
}

static bool holds_adapter(int which, const void *subject) {
    return sixteenth_century_melodic_rule_holds((SixteenthCenturyMelodicRule)which, subject);
}

bool sixteenth_century_melodic_check(const SixteenthCenturyScore *score, unsigned *hard_violations,
                                     unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL || !sixteenth_century_score_valid(score)) {
        return false;
    }
    sixteenth_century_rule_table_check(&TABLE, holds_adapter, score, hard_violations, soft_violations);
    return true;
}
