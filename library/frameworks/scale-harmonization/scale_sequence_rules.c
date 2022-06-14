#include "scale_sequence_rules.h"

#include <stdlib.h>

#include "scale_rule_table.h"
#include "standard-counterpoint/counterpoint_pitch.h"

static const char *const NAMES[SCALE_SEQUENCE_RULE_COUNT] = {"direction", "bass-by-step", "intervals",
                                                             "repeats",   "suspended",    "whole-units"};

static bool table_is_hard(int which) {
    return which <= SCALE_SEQUENCE_SUSPENDED;
}

static ScaleRulePair g_pairs[SCALE_SEQUENCE_RULE_COUNT];
static bool g_built[SCALE_SEQUENCE_RULE_COUNT];
static const ScaleRuleTable TABLE = {"sequence", NAMES, SCALE_SEQUENCE_RULE_COUNT, table_is_hard, g_pairs, g_built};

const Rule *scale_sequence_rule(ScaleSequenceRule which) {
    return scale_rule_table_rule(&TABLE, (int)which);
}

const Constraint *scale_sequence_constraint(ScaleSequenceRule which) {
    return scale_rule_table_constraint(&TABLE, (int)which);
}

const char *scale_sequence_rule_name(ScaleSequenceRule which) {
    return scale_rule_table_name(&TABLE, (int)which);
}

bool scale_sequence_valid(const ScaleSequence *sequence) {
    return sequence != NULL && sequence->pattern != NULL && sequence->pattern->unit_length > 0 &&
           sequence->pattern->unit_length <= SCALE_MAX_UNIT && scale_harmonization_valid(sequence->harmonization);
}

static const ScalePatternStage *pattern_stage(const ScaleSequence *s, size_t stage) {
    return &s->pattern->unit[stage % s->pattern->unit_length];
}

static const PitchSpelling *bass_at(const ScaleSequence *s, size_t stage) {
    return s->harmonization->stages[stage].pitches[0];
}

static const PitchSpelling *upper_at(const ScaleSequence *s, size_t stage) {
    return s->harmonization->stages[stage].pitches[1];
}

static bool check_bass_by_step(const ScaleSequence *s) {
    int direction = s->harmonization->direction == SCALE_DIRECTION_ASCENDING ? 1 : -1;
    for (size_t i = 1; i < s->harmonization->stage_count; i++) {
        const PitchSpelling *previous = bass_at(s, i - 1);
        const PitchSpelling *current = bass_at(s, i);
        if (pattern_stage(s, i)->bass_moves) {
            int steps = standard_counterpoint_step_number(current) - standard_counterpoint_step_number(previous);
            int semitones = abs(standard_counterpoint_semitone_number(current) -
                                standard_counterpoint_semitone_number(previous));
            if (steps != direction || (semitones != 1 && semitones != 2)) {
                return false;
            }
        } else if (!standard_counterpoint_same_pitch(previous, current)) {
            return false;
        }
    }
    return true;
}

static bool check_intervals(const ScaleSequence *s) {
    for (size_t i = 0; i < s->harmonization->stage_count; i++) {
        int interval;
        if (!scale_interval_above_bass(s->harmonization, i, 1, &interval) ||
            !(pattern_stage(s, i)->intervals & (1u << interval))) {
            return false;
        }
    }
    return true;
}

static bool check_repeats(const ScaleSequence *s) {
    size_t unit = s->pattern->unit_length;
    for (size_t i = unit; i < s->harmonization->stage_count; i++) {
        int now, before;
        if (!scale_interval_above_bass(s->harmonization, i, 1, &now) ||
            !scale_interval_above_bass(s->harmonization, i - unit, 1, &before) || now != before) {
            return false;
        }
    }
    return true;
}

static bool check_suspended(const ScaleSequence *s) {
    for (size_t i = 1; i < s->harmonization->stage_count; i++) {
        if (pattern_stage(s, i)->suspended && !standard_counterpoint_same_pitch(upper_at(s, i - 1), upper_at(s, i))) {
            return false;
        }
    }
    return true;
}

bool scale_sequence_rule_holds(ScaleSequenceRule which, const ScaleSequence *sequence) {
    if (!scale_rule_table_valid(&TABLE, (int)which) || !scale_sequence_valid(sequence)) {
        return false;
    }
    switch (which) {
        case SCALE_SEQUENCE_DIRECTION:
            return scale_pattern_allows_direction(sequence->pattern, sequence->harmonization->direction);
        case SCALE_SEQUENCE_BASS_BY_STEP:
            return check_bass_by_step(sequence);
        case SCALE_SEQUENCE_INTERVALS:
            return check_intervals(sequence);
        case SCALE_SEQUENCE_REPEATS:
            return check_repeats(sequence);
        case SCALE_SEQUENCE_SUSPENDED:
            return check_suspended(sequence);
        case SCALE_SEQUENCE_WHOLE_UNITS:
            return sequence->harmonization->stage_count % sequence->pattern->unit_length == 0;
        default:
            return false;
    }
}

static bool holds_adapter(int which, const void *subject) {
    return scale_sequence_rule_holds((ScaleSequenceRule)which, subject);
}

bool scale_sequence_check(const ScaleSequence *sequence, unsigned *hard_violations, unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL || !scale_sequence_valid(sequence)) {
        return false;
    }
    scale_rule_table_check(&TABLE, holds_adapter, sequence, hard_violations, soft_violations);
    return true;
}
