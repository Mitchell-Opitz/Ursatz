#include "century_dissonance_rules.h"

#include <stddef.h>

#include "century_rule_table.h"
#include "standard-counterpoint/counterpoint_interval.h"
#include "standard-counterpoint/counterpoint_pitch.h"

static const char *const NAMES[SIXTEENTH_CENTURY_DISSONANCE_RULE_COUNT] = {"figured", "proper-suspensions",
                                                                         "figure-resolves"};

static bool table_is_hard(int which) {
    return which <= SIXTEENTH_CENTURY_DISSONANCE_PROPER_SUSPENSIONS;
}

static SixteenthCenturyRulePair g_pairs[SIXTEENTH_CENTURY_DISSONANCE_RULE_COUNT];
static bool g_built[SIXTEENTH_CENTURY_DISSONANCE_RULE_COUNT];
static const SixteenthCenturyRuleTable TABLE = {"dissonance", NAMES, SIXTEENTH_CENTURY_DISSONANCE_RULE_COUNT,
                                                table_is_hard, g_pairs, g_built};

const Rule *sixteenth_century_dissonance_rule(SixteenthCenturyDissonanceRule which) {
    return sixteenth_century_rule_table_rule(&TABLE, (int)which);
}

const Constraint *sixteenth_century_dissonance_constraint(SixteenthCenturyDissonanceRule which) {
    return sixteenth_century_rule_table_constraint(&TABLE, (int)which);
}

const char *sixteenth_century_dissonance_rule_name(SixteenthCenturyDissonanceRule which) {
    return sixteenth_century_rule_table_name(&TABLE, (int)which);
}

bool sixteenth_century_consonant_at(const SixteenthCenturyScore *score, size_t a, size_t b, size_t slot) {
    const PitchSpelling *pa = sixteenth_century_pitch_at(score, a, slot);
    const PitchSpelling *pb = sixteenth_century_pitch_at(score, b, slot);
    if (pa == NULL || pb == NULL) {
        return false;
    }
    StandardCounterpointInterval interval;
    if (!standard_counterpoint_interval_between(pa, pb, &interval)) {
        return false;
    }
    switch (interval.simple_size) {
        case 1:
        case 5:
        case 8:
            return interval.quality == STANDARD_COUNTERPOINT_QUALITY_PERFECT;
        case 3:
        case 6:
            return interval.quality == STANDARD_COUNTERPOINT_QUALITY_MAJOR ||
                   interval.quality == STANDARD_COUNTERPOINT_QUALITY_MINOR;
        case 4: {
            if (interval.quality != STANDARD_COUNTERPOINT_QUALITY_PERFECT) {
                return false;
            }
            /* A perfect fourth is consonant between two upper voices only. */
            int lower = standard_counterpoint_semitone_number(pa);
            int other = standard_counterpoint_semitone_number(pb);
            lower = other < lower ? other : lower;
            for (size_t v = 0; v < score->voice_count; v++) {
                const PitchSpelling *p = sixteenth_century_pitch_at(score, v, slot);
                if (p != NULL && standard_counterpoint_semitone_number(p) < lower) {
                    return true;
                }
            }
            return false;
        }
        default:
            return false;
    }
}

static int position_rank(const SixteenthCenturyScore *score, size_t slot) {
    switch (sixteenth_century_position_at(score, slot)) {
        case SIXTEENTH_CENTURY_POSITION_STRONG:
            return 2;
        case SIXTEENTH_CENTURY_POSITION_WEAK:
            return 1;
        default:
            return 0;
    }
}

/* Where the note of `voice` that contains `slot` sits in its list of notes.
   False when the voice rests there or the notes do not fit. */
static bool find_note(const SixteenthCenturyScore *score, size_t voice, size_t slot, SixteenthCenturyNote *notes,
                      size_t *count, size_t *index) {
    if (!sixteenth_century_voice_notes(score, voice, notes, SIXTEENTH_CENTURY_MAX_NOTES, count)) {
        return false;
    }
    for (size_t i = 0; i < *count; i++) {
        if (notes[i].start <= slot && slot < notes[i].start + notes[i].length) {
            *index = i;
            return true;
        }
    }
    return false;
}

static bool contiguous(const SixteenthCenturyNote *first, const SixteenthCenturyNote *second) {
    return first->start + first->length == second->start;
}

static bool is_step(const PitchSpelling *a, const PitchSpelling *b, int direction) {
    StandardCounterpointInterval interval;
    return standard_counterpoint_interval_between(a, b, &interval) && interval.size == 2 &&
           (direction == 0 || standard_counterpoint_direction(a, b) == direction);
}

static bool is_third_leap_down(const PitchSpelling *a, const PitchSpelling *b) {
    StandardCounterpointInterval interval;
    return standard_counterpoint_interval_between(a, b, &interval) && interval.size == 3 &&
           standard_counterpoint_direction(a, b) < 0;
}

/* True when the note beginning at `slot` in `voice` is a passing, neighbor,
   or nota cambiata figure. Sets *next_start to where its next note begins. */
static bool figure_at(const SixteenthCenturyScore *score, size_t voice, size_t slot, size_t *next_start) {
    SixteenthCenturyNote notes[SIXTEENTH_CENTURY_MAX_NOTES];
    size_t count, i;
    if (!find_note(score, voice, slot, notes, &count, &i) || notes[i].start != slot || i == 0 ||
        i + 1 >= count || !contiguous(&notes[i - 1], &notes[i]) || !contiguous(&notes[i], &notes[i + 1])) {
        return false;
    }
    *next_start = notes[i + 1].start;
    const PitchSpelling *prev = notes[i - 1].pitch;
    const PitchSpelling *cur = notes[i].pitch;
    const PitchSpelling *next = notes[i + 1].pitch;
    if (is_step(prev, cur, 0) && is_step(cur, next, 0)) {
        return true;
    }
    if (is_step(prev, cur, -1) && is_third_leap_down(cur, next)) {
        return i + 2 >= count || !contiguous(&notes[i + 1], &notes[i + 2]) ||
               is_step(next, notes[i + 2].pitch, 1);
    }
    return false;
}

/* True when `holder`, tied into `slot` against `other`, is a prepared and
   resolved suspension. */
static bool suspension_at(const SixteenthCenturyScore *score, size_t holder, size_t other, size_t slot) {
    SixteenthCenturyNote notes[SIXTEENTH_CENTURY_MAX_NOTES];
    size_t count, i;
    if (!find_note(score, holder, slot, notes, &count, &i) || notes[i].start >= slot) {
        return false;
    }
    size_t start = notes[i].start;
    if (position_rank(score, start) >= position_rank(score, slot)) {
        return false;
    }
    if (sixteenth_century_pitch_at(score, other, start) != NULL &&
        !sixteenth_century_consonant_at(score, holder, other, start)) {
        return false;
    }
    if (i + 1 >= count || !contiguous(&notes[i], &notes[i + 1]) ||
        !is_step(notes[i].pitch, notes[i + 1].pitch, -1)) {
        return false;
    }
    size_t resolution = notes[i + 1].start;
    return sixteenth_century_pitch_at(score, other, resolution) == NULL ||
           sixteenth_century_consonant_at(score, holder, other, resolution);
}

static bool check(SixteenthCenturyDissonanceRule which, const SixteenthCenturyScore *score) {
    size_t slots = sixteenth_century_score_slot_count(score);
    for (size_t a = 0; a < score->voice_count; a++) {
        for (size_t b = a + 1; b < score->voice_count; b++) {
            for (size_t s = 0; s < slots; s++) {
                bool attack_a = sixteenth_century_attacks_at(score, a, s);
                bool attack_b = sixteenth_century_attacks_at(score, b, s);
                if ((!attack_a && !attack_b) || sixteenth_century_pitch_at(score, a, s) == NULL ||
                    sixteenth_century_pitch_at(score, b, s) == NULL ||
                    sixteenth_century_consonant_at(score, a, b, s)) {
                    continue;
                }
                bool tactus = sixteenth_century_position_at(score, s) != SIXTEENTH_CENTURY_POSITION_SUB;
                bool suspension = tactus && (!attack_a || !attack_b);
                size_t next_a = 0, next_b = 0;
                bool figure_a = attack_a && !tactus && figure_at(score, a, s, &next_a);
                bool figure_b = attack_b && !tactus && figure_at(score, b, s, &next_b);
                switch (which) {
                    case SIXTEENTH_CENTURY_DISSONANCE_FIGURED:
                        if (suspension) {
                            break;
                        }
                        if (tactus || !(figure_a || figure_b)) {
                            return false;
                        }
                        break;
                    case SIXTEENTH_CENTURY_DISSONANCE_PROPER_SUSPENSIONS:
                        if (suspension && !(attack_a ? suspension_at(score, b, a, s)
                                                      : suspension_at(score, a, b, s))) {
                            return false;
                        }
                        break;
                    case SIXTEENTH_CENTURY_DISSONANCE_FIGURE_RESOLVES: {
                        size_t next = figure_a ? next_a : next_b;
                        if ((figure_a || figure_b) && sixteenth_century_pitch_at(score, a, next) != NULL &&
                            sixteenth_century_pitch_at(score, b, next) != NULL &&
                            !sixteenth_century_consonant_at(score, a, b, next)) {
                            return false;
                        }
                        break;
                    }
                    default:
                        return false;
                }
            }
        }
    }
    return true;
}

bool sixteenth_century_dissonance_rule_holds(SixteenthCenturyDissonanceRule which,
                                             const SixteenthCenturyScore *score) {
    if (!sixteenth_century_rule_table_valid(&TABLE, (int)which) || !sixteenth_century_score_valid(score)) {
        return false;
    }
    return check(which, score);
}

static bool holds_adapter(int which, const void *subject) {
    return sixteenth_century_dissonance_rule_holds((SixteenthCenturyDissonanceRule)which, subject);
}

bool sixteenth_century_dissonance_check(const SixteenthCenturyScore *score, unsigned *hard_violations,
                                        unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL || !sixteenth_century_score_valid(score)) {
        return false;
    }
    sixteenth_century_rule_table_check(&TABLE, holds_adapter, score, hard_violations, soft_violations);
    return true;
}
