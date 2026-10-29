#include "century_rhythm_rules.h"

#include <stddef.h>

#include "century_rule_table.h"

static const char *const NAMES[SIXTEENTH_CENTURY_RHYTHM_RULE_COUNT] = {"short-notes-off-beat", "slow-start-end"};

static SixteenthCenturyRulePair g_pairs[SIXTEENTH_CENTURY_RHYTHM_RULE_COUNT];
static UrsatzOnce g_built[SIXTEENTH_CENTURY_RHYTHM_RULE_COUNT];
static const SixteenthCenturyRuleTable TABLE = {"rhythm", NAMES, SIXTEENTH_CENTURY_RHYTHM_RULE_COUNT,
                                                sixteenth_century_rule_table_never_hard, g_pairs, g_built};

const Rule *sixteenth_century_rhythm_rule(SixteenthCenturyRhythmRule which) {
    return sixteenth_century_rule_table_rule(&TABLE, (int)which);
}

const Constraint *sixteenth_century_rhythm_constraint(SixteenthCenturyRhythmRule which) {
    return sixteenth_century_rule_table_constraint(&TABLE, (int)which);
}

const char *sixteenth_century_rhythm_rule_name(SixteenthCenturyRhythmRule which) {
    return sixteenth_century_rule_table_name(&TABLE, (int)which);
}

static bool check_short_notes_off_beat(const SixteenthCenturyScore *score, const SixteenthCenturyNote *notes,
                                       size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (notes[i].length < score->slots_per_tactus &&
            sixteenth_century_position_at(score, notes[i].start) != SIXTEENTH_CENTURY_POSITION_SUB) {
            return false;
        }
    }
    return true;
}

static bool check_slow_start_end(const SixteenthCenturyScore *score, const SixteenthCenturyNote *notes,
                                 size_t count) {
    return count == 0 || (notes[0].length >= score->slots_per_tactus &&
                          notes[count - 1].length >= score->slots_per_tactus);
}

bool sixteenth_century_rhythm_rule_holds(SixteenthCenturyRhythmRule which, const SixteenthCenturyScore *score) {
    if (!sixteenth_century_rule_table_valid(&TABLE, (int)which) || !sixteenth_century_score_valid(score)) {
        return false;
    }
    for (size_t voice = 0; voice < score->voice_count; voice++) {
        SixteenthCenturyNote notes[SIXTEENTH_CENTURY_MAX_NOTES];
        size_t count;
        if (!sixteenth_century_voice_notes(score, voice, notes, SIXTEENTH_CENTURY_MAX_NOTES, &count)) {
            return false;
        }
        bool holds = which == SIXTEENTH_CENTURY_RHYTHM_SHORT_NOTES_OFF_BEAT
                         ? check_short_notes_off_beat(score, notes, count)
                         : check_slow_start_end(score, notes, count);
        if (!holds) {
            return false;
        }
    }
    return true;
}

static bool holds_adapter(int which, const void *subject) {
    return sixteenth_century_rhythm_rule_holds((SixteenthCenturyRhythmRule)which, subject);
}

bool sixteenth_century_rhythm_check(const SixteenthCenturyScore *score, unsigned *hard_violations,
                                    unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL || !sixteenth_century_score_valid(score)) {
        return false;
    }
    sixteenth_century_rule_table_check(&TABLE, holds_adapter, score, hard_violations, soft_violations);
    return true;
}
