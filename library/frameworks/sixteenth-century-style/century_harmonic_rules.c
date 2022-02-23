#include "century_harmonic_rules.h"

#include <stddef.h>

#include "century_rule_table.h"
#include "standard-counterpoint/counterpoint_interval.h"
#include "standard-counterpoint/counterpoint_pitch.h"

static const char *const NAMES[SIXTEENTH_CENTURY_HARMONIC_RULE_COUNT] = {
    "no-parallel-perfects", "diminished-triad-resolves", "no-direct-perfects", "final-chord"};

static bool table_is_hard(int which) {
    return which <= SIXTEENTH_CENTURY_HARMONIC_DIMINISHED_TRIAD_RESOLVES;
}

static SixteenthCenturyRulePair g_pairs[SIXTEENTH_CENTURY_HARMONIC_RULE_COUNT];
static bool g_built[SIXTEENTH_CENTURY_HARMONIC_RULE_COUNT];
static const SixteenthCenturyRuleTable TABLE = {"harmonic", NAMES, SIXTEENTH_CENTURY_HARMONIC_RULE_COUNT,
                                                table_is_hard, g_pairs, g_built};

const Rule *sixteenth_century_harmonic_rule(SixteenthCenturyHarmonicRule which) {
    return sixteenth_century_rule_table_rule(&TABLE, (int)which);
}

const Constraint *sixteenth_century_harmonic_constraint(SixteenthCenturyHarmonicRule which) {
    return sixteenth_century_rule_table_constraint(&TABLE, (int)which);
}

const char *sixteenth_century_harmonic_rule_name(SixteenthCenturyHarmonicRule which) {
    return sixteenth_century_rule_table_name(&TABLE, (int)which);
}

typedef enum { CLASS_NONE = 0, CLASS_FIFTH, CLASS_OCTAVE } PerfectClass;

/* The perfect class of the interval between two pitches: a fifth (perfect or
   diminished), an octave or unison (perfect), or none. */
static PerfectClass perfect_class(const PitchSpelling *a, const PitchSpelling *b) {
    StandardCounterpointInterval interval;
    if (!standard_counterpoint_interval_between(a, b, &interval)) {
        return CLASS_NONE;
    }
    if (interval.simple_size == 5 && (interval.quality == STANDARD_COUNTERPOINT_QUALITY_PERFECT ||
                                      interval.quality == STANDARD_COUNTERPOINT_QUALITY_DIMINISHED)) {
        return CLASS_FIFTH;
    }
    if ((interval.simple_size == 8 || interval.simple_size == 1) &&
        interval.quality == STANDARD_COUNTERPOINT_QUALITY_PERFECT) {
        return CLASS_OCTAVE;
    }
    return CLASS_NONE;
}

static bool perfect_arrival_class(const PitchSpelling *a, const PitchSpelling *b, PerfectClass *out) {
    StandardCounterpointInterval interval;
    if (!standard_counterpoint_interval_between(a, b, &interval) ||
        interval.quality != STANDARD_COUNTERPOINT_QUALITY_PERFECT) {
        return false;
    }
    if (interval.simple_size == 5) {
        *out = CLASS_FIFTH;
        return true;
    }
    if (interval.simple_size == 8 || interval.simple_size == 1) {
        *out = CLASS_OCTAVE;
        return true;
    }
    return false;
}

/* The next slot after `from` where voices a and b both sound and at least
   one of them begins a note; returns false when there is none. */
static bool next_pair_moment(const SixteenthCenturyScore *score, size_t a, size_t b, size_t from, size_t *out) {
    size_t slots = sixteenth_century_score_slot_count(score);
    for (size_t s = from + 1; s < slots; s++) {
        if (sixteenth_century_pitch_at(score, a, s) != NULL && sixteenth_century_pitch_at(score, b, s) != NULL &&
            (sixteenth_century_attacks_at(score, a, s) || sixteenth_century_attacks_at(score, b, s))) {
            *out = s;
            return true;
        }
    }
    return false;
}

static bool first_pair_moment(const SixteenthCenturyScore *score, size_t a, size_t b, size_t *out) {
    size_t slots = sixteenth_century_score_slot_count(score);
    for (size_t s = 0; s < slots; s++) {
        if (sixteenth_century_pitch_at(score, a, s) != NULL && sixteenth_century_pitch_at(score, b, s) != NULL &&
            (sixteenth_century_attacks_at(score, a, s) || sixteenth_century_attacks_at(score, b, s))) {
            *out = s;
            return true;
        }
    }
    return false;
}

/* The common direction of both voices between slots s1 and s2: 1 up, -1 down,
   0 when either holds its pitch or they move apart. */
static int common_direction(const SixteenthCenturyScore *score, size_t a, size_t b, size_t s1, size_t s2) {
    int da = standard_counterpoint_direction(sixteenth_century_pitch_at(score, a, s1),
                                             sixteenth_century_pitch_at(score, a, s2));
    int db = standard_counterpoint_direction(sixteenth_century_pitch_at(score, b, s1),
                                             sixteenth_century_pitch_at(score, b, s2));
    return da != 0 && da == db ? da : 0;
}

static bool check_no_parallel_perfects(const SixteenthCenturyScore *score) {
    for (size_t a = 0; a < score->voice_count; a++) {
        for (size_t b = a + 1; b < score->voice_count; b++) {
            size_t s1;
            if (!first_pair_moment(score, a, b, &s1)) {
                continue;
            }
            size_t s2;
            while (next_pair_moment(score, a, b, s1, &s2)) {
                PerfectClass before = perfect_class(sixteenth_century_pitch_at(score, a, s1),
                                                    sixteenth_century_pitch_at(score, b, s1));
                PerfectClass after = perfect_class(sixteenth_century_pitch_at(score, a, s2),
                                                   sixteenth_century_pitch_at(score, b, s2));
                if (before != CLASS_NONE && before == after && common_direction(score, a, b, s1, s2) != 0) {
                    return false;
                }
                s1 = s2;
            }
        }
    }
    return true;
}

static bool other_voice_contrary(const SixteenthCenturyScore *score, size_t a, size_t b, size_t s1, size_t s2,
                                 int direction) {
    for (size_t v = 0; v < score->voice_count; v++) {
        if (v == a || v == b) {
            continue;
        }
        const PitchSpelling *from = sixteenth_century_pitch_at(score, v, s1);
        const PitchSpelling *to = sixteenth_century_pitch_at(score, v, s2);
        if (from != NULL && to != NULL && standard_counterpoint_direction(from, to) == -direction) {
            return true;
        }
    }
    return false;
}

static bool check_no_direct_perfects(const SixteenthCenturyScore *score) {
    for (size_t a = 0; a < score->voice_count; a++) {
        for (size_t b = a + 1; b < score->voice_count; b++) {
            size_t s1;
            if (!first_pair_moment(score, a, b, &s1)) {
                continue;
            }
            size_t s2;
            while (next_pair_moment(score, a, b, s1, &s2)) {
                size_t s3;
                bool last = !next_pair_moment(score, a, b, s2, &s3);
                PerfectClass arrival;
                int direction = common_direction(score, a, b, s1, s2);
                if (!last && direction != 0 &&
                    perfect_arrival_class(sixteenth_century_pitch_at(score, a, s2),
                                          sixteenth_century_pitch_at(score, b, s2), &arrival) &&
                    !other_voice_contrary(score, a, b, s1, s2, direction)) {
                    return false;
                }
                s1 = s2;
            }
        }
    }
    return true;
}

static bool all_sound_at(const SixteenthCenturyScore *score, size_t slot) {
    for (size_t v = 0; v < score->voice_count; v++) {
        if (sixteenth_century_pitch_at(score, v, slot) == NULL) {
            return false;
        }
    }
    return true;
}

static bool any_attacks_at(const SixteenthCenturyScore *score, size_t slot) {
    for (size_t v = 0; v < score->voice_count; v++) {
        if (sixteenth_century_attacks_at(score, v, slot)) {
            return true;
        }
    }
    return false;
}

static int pitch_class(const PitchSpelling *p) {
    int n = standard_counterpoint_semitone_number(p) % 12;
    return n < 0 ? n + 12 : n;
}

/* True when the sounding pitch classes at `slot` are exactly the three of a
   diminished triad; sets *root to the pitch class of its root. */
static bool diminished_triad_at(const SixteenthCenturyScore *score, size_t slot, int *root) {
    bool present[12] = {false};
    for (size_t v = 0; v < score->voice_count; v++) {
        present[pitch_class(sixteenth_century_pitch_at(score, v, slot))] = true;
    }
    int distinct = 0;
    for (int i = 0; i < 12; i++) {
        distinct += present[i] ? 1 : 0;
    }
    if (distinct != 3) {
        return false;
    }
    for (int r = 0; r < 12; r++) {
        if (present[r] && present[(r + 3) % 12] && present[(r + 6) % 12]) {
            *root = r;
            return true;
        }
    }
    return false;
}

static bool check_diminished_triad_resolves(const SixteenthCenturyScore *score) {
    if (score->voice_count < 3) {
        return true;
    }
    size_t slots = sixteenth_century_score_slot_count(score);
    for (size_t s = 0; s < slots; s++) {
        int root;
        if (!all_sound_at(score, s) || !diminished_triad_at(score, s, &root)) {
            continue;
        }
        size_t next = s + 1;
        while (next < slots && !(any_attacks_at(score, next) && all_sound_at(score, next))) {
            next++;
        }
        if (next >= slots) {
            continue;
        }
        for (size_t v = 0; v < score->voice_count; v++) {
            const PitchSpelling *from = sixteenth_century_pitch_at(score, v, s);
            const PitchSpelling *to = sixteenth_century_pitch_at(score, v, next);
            int change = standard_counterpoint_semitone_number(to) - standard_counterpoint_semitone_number(from);
            int pc = pitch_class(from);
            if (pc == root && change != 1) {
                return false;
            }
            if (pc == (root + 6) % 12 && change != -1) {
                return false;
            }
            if (pc == (root + 3) % 12 && change != 0 && (change > 0 || change < -2)) {
                return false;
            }
        }
    }
    return true;
}

static bool check_final_chord(const SixteenthCenturyScore *score) {
    size_t last = sixteenth_century_score_slot_count(score) - 1;
    if (score->voice_count < 2 || !all_sound_at(score, last)) {
        return true;
    }
    size_t lowest = 0;
    for (size_t v = 1; v < score->voice_count; v++) {
        if (standard_counterpoint_semitone_number(sixteenth_century_pitch_at(score, v, last)) <
            standard_counterpoint_semitone_number(sixteenth_century_pitch_at(score, lowest, last))) {
            lowest = v;
        }
    }
    for (size_t v = 0; v < score->voice_count; v++) {
        if (v == lowest) {
            continue;
        }
        StandardCounterpointInterval interval;
        if (!standard_counterpoint_interval_between(sixteenth_century_pitch_at(score, lowest, last),
                                                    sixteenth_century_pitch_at(score, v, last), &interval)) {
            return false;
        }
        bool perfect = interval.quality == STANDARD_COUNTERPOINT_QUALITY_PERFECT &&
                       (interval.simple_size == 1 || interval.simple_size == 5 || interval.simple_size == 8);
        bool major_third = interval.quality == STANDARD_COUNTERPOINT_QUALITY_MAJOR && interval.simple_size == 3;
        if (!perfect && !major_third) {
            return false;
        }
    }
    return true;
}

bool sixteenth_century_harmonic_rule_holds(SixteenthCenturyHarmonicRule which, const SixteenthCenturyScore *score) {
    if (!sixteenth_century_rule_table_valid(&TABLE, (int)which) || !sixteenth_century_score_valid(score)) {
        return false;
    }
    switch (which) {
        case SIXTEENTH_CENTURY_HARMONIC_NO_PARALLEL_PERFECTS:
            return check_no_parallel_perfects(score);
        case SIXTEENTH_CENTURY_HARMONIC_DIMINISHED_TRIAD_RESOLVES:
            return check_diminished_triad_resolves(score);
        case SIXTEENTH_CENTURY_HARMONIC_NO_DIRECT_PERFECTS:
            return check_no_direct_perfects(score);
        case SIXTEENTH_CENTURY_HARMONIC_FINAL_CHORD:
            return check_final_chord(score);
        default:
            return false;
    }
}

static bool holds_adapter(int which, const void *subject) {
    return sixteenth_century_harmonic_rule_holds((SixteenthCenturyHarmonicRule)which, subject);
}

bool sixteenth_century_harmonic_check(const SixteenthCenturyScore *score, unsigned *hard_violations,
                                      unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL || !sixteenth_century_score_valid(score)) {
        return false;
    }
    sixteenth_century_rule_table_check(&TABLE, holds_adapter, score, hard_violations, soft_violations);
    return true;
}
