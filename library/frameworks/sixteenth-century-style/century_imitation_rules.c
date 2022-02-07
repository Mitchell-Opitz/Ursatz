#include "century_imitation_rules.h"

#include <stdlib.h>

#include "century_rule_table.h"
#include "standard-counterpoint/counterpoint_interval.h"
#include "standard-counterpoint/counterpoint_pitch.h"

static const char *const NAMES[SIXTEENTH_CENTURY_IMITATION_RULE_COUNT] = {
    "rhythm-preserved", "contour-preserved", "not-free", "entry-interval", "metric-match", "overlap"};

static bool table_is_hard(int which) {
    return which <= SIXTEENTH_CENTURY_IMITATION_CONTOUR_PRESERVED;
}

static SixteenthCenturyRulePair g_pairs[SIXTEENTH_CENTURY_IMITATION_RULE_COUNT];
static bool g_built[SIXTEENTH_CENTURY_IMITATION_RULE_COUNT];
static const SixteenthCenturyRuleTable TABLE = {"imitation", NAMES, SIXTEENTH_CENTURY_IMITATION_RULE_COUNT,
                                                table_is_hard, g_pairs, g_built};

const Rule *sixteenth_century_imitation_rule(SixteenthCenturyImitationRule which) {
    return sixteenth_century_rule_table_rule(&TABLE, (int)which);
}

const Constraint *sixteenth_century_imitation_constraint(SixteenthCenturyImitationRule which) {
    return sixteenth_century_rule_table_constraint(&TABLE, (int)which);
}

const char *sixteenth_century_imitation_rule_name(SixteenthCenturyImitationRule which) {
    return sixteenth_century_rule_table_name(&TABLE, (int)which);
}

bool sixteenth_century_point_valid(const SixteenthCenturyPoint *point) {
    if (point == NULL || !sixteenth_century_score_valid(point->score) || point->entries == NULL ||
        point->entry_count < 2 || point->entry_count > SIXTEENTH_CENTURY_MAX_VOICES || point->note_count < 2) {
        return false;
    }
    size_t previous_start = 0;
    for (size_t e = 0; e < point->entry_count; e++) {
        SixteenthCenturyNote notes[SIXTEENTH_CENTURY_MAX_NOTES];
        size_t count;
        const SixteenthCenturyEntry *entry = &point->entries[e];
        if (entry->voice >= point->score->voice_count ||
            !sixteenth_century_voice_notes(point->score, entry->voice, notes, SIXTEENTH_CENTURY_MAX_NOTES, &count) ||
            entry->note + point->note_count > count) {
            return false;
        }
        size_t start = notes[entry->note].start;
        if (e > 0 && start <= previous_start) {
            return false;
        }
        previous_start = start;
    }
    return true;
}

/* The notes of one entry's point. */
static bool entry_notes(const SixteenthCenturyPoint *point, size_t entry, SixteenthCenturyNote *out) {
    SixteenthCenturyNote notes[SIXTEENTH_CENTURY_MAX_NOTES];
    size_t count;
    const SixteenthCenturyEntry *e = &point->entries[entry];
    if (!sixteenth_century_voice_notes(point->score, e->voice, notes, SIXTEENTH_CENTURY_MAX_NOTES, &count)) {
        return false;
    }
    for (size_t i = 0; i < point->note_count; i++) {
        out[i] = notes[e->note + i];
    }
    return true;
}

static size_t core_notes(const SixteenthCenturyPoint *point) {
    return point->note_count < SIXTEENTH_CENTURY_IMITATION_CORE_NOTES ? point->note_count
                                                                       : SIXTEENTH_CENTURY_IMITATION_CORE_NOTES;
}

bool sixteenth_century_point_answer(const SixteenthCenturyPoint *point, size_t entry, ImitationAnswer *out) {
    if (out == NULL || !sixteenth_century_point_valid(point) || entry == 0 || entry >= point->entry_count) {
        return false;
    }
    SixteenthCenturyNote leader[SIXTEENTH_CENTURY_MAX_NOTES];
    SixteenthCenturyNote answer[SIXTEENTH_CENTURY_MAX_NOTES];
    if (!entry_notes(point, 0, leader) || !entry_notes(point, entry, answer)) {
        return false;
    }
    bool real = true;
    bool tonal = true;
    for (size_t i = 0; i + 1 < point->note_count; i++) {
        int dl = standard_counterpoint_direction(leader[i].pitch, leader[i + 1].pitch);
        int da = standard_counterpoint_direction(answer[i].pitch, answer[i + 1].pitch);
        int sl = standard_counterpoint_semitone_number(leader[i + 1].pitch) -
                 standard_counterpoint_semitone_number(leader[i].pitch);
        int sa = standard_counterpoint_semitone_number(answer[i + 1].pitch) -
                 standard_counterpoint_semitone_number(answer[i].pitch);
        StandardCounterpointInterval il, ia;
        if (!standard_counterpoint_interval_between(leader[i].pitch, leader[i + 1].pitch, &il) ||
            !standard_counterpoint_interval_between(answer[i].pitch, answer[i + 1].pitch, &ia)) {
            return false;
        }
        real = real && sl == sa;
        tonal = tonal && dl == da && abs(il.size - ia.size) <= 1;
    }
    *out = real ? IMITATION_ANSWER_REAL : tonal ? IMITATION_ANSWER_TONAL : IMITATION_ANSWER_FREE;
    return true;
}

static bool check_rhythm_preserved(const SixteenthCenturyPoint *point) {
    SixteenthCenturyNote leader[SIXTEENTH_CENTURY_MAX_NOTES];
    SixteenthCenturyNote answer[SIXTEENTH_CENTURY_MAX_NOTES];
    if (!entry_notes(point, 0, leader)) {
        return false;
    }
    for (size_t e = 1; e < point->entry_count; e++) {
        if (!entry_notes(point, e, answer)) {
            return false;
        }
        for (size_t i = 0; i < core_notes(point); i++) {
            if (answer[i].length != leader[i].length) {
                return false;
            }
        }
    }
    return true;
}

static bool check_contour_preserved(const SixteenthCenturyPoint *point) {
    SixteenthCenturyNote leader[SIXTEENTH_CENTURY_MAX_NOTES];
    SixteenthCenturyNote answer[SIXTEENTH_CENTURY_MAX_NOTES];
    if (!entry_notes(point, 0, leader)) {
        return false;
    }
    for (size_t e = 1; e < point->entry_count; e++) {
        if (!entry_notes(point, e, answer)) {
            return false;
        }
        for (size_t i = 0; i + 1 < core_notes(point); i++) {
            if (standard_counterpoint_direction(leader[i].pitch, leader[i + 1].pitch) !=
                standard_counterpoint_direction(answer[i].pitch, answer[i + 1].pitch)) {
                return false;
            }
        }
    }
    return true;
}

static bool check_not_free(const SixteenthCenturyPoint *point) {
    for (size_t e = 1; e < point->entry_count; e++) {
        ImitationAnswer answer;
        if (!sixteenth_century_point_answer(point, e, &answer) || answer == IMITATION_ANSWER_FREE) {
            return false;
        }
    }
    return true;
}

static bool check_entry_interval(const SixteenthCenturyPoint *point) {
    SixteenthCenturyNote leader[SIXTEENTH_CENTURY_MAX_NOTES];
    SixteenthCenturyNote answer[SIXTEENTH_CENTURY_MAX_NOTES];
    if (!entry_notes(point, 0, leader) || !entry_notes(point, 1, answer)) {
        return false;
    }
    StandardCounterpointInterval interval;
    if (!standard_counterpoint_interval_between(leader[0].pitch, answer[0].pitch, &interval)) {
        return false;
    }
    return interval.quality == STANDARD_COUNTERPOINT_QUALITY_PERFECT &&
           (interval.simple_size == 1 || interval.simple_size == 4 || interval.simple_size == 5 ||
            interval.simple_size == 8);
}

static bool check_metric_match(const SixteenthCenturyPoint *point) {
    SixteenthCenturyNote leader[SIXTEENTH_CENTURY_MAX_NOTES];
    SixteenthCenturyNote answer[SIXTEENTH_CENTURY_MAX_NOTES];
    if (!entry_notes(point, 0, leader)) {
        return false;
    }
    for (size_t e = 1; e < point->entry_count; e++) {
        if (!entry_notes(point, e, answer) ||
            sixteenth_century_position_at(point->score, answer[0].start) !=
                sixteenth_century_position_at(point->score, leader[0].start)) {
            return false;
        }
    }
    return true;
}

static bool check_overlap(const SixteenthCenturyPoint *point) {
    SixteenthCenturyNote previous[SIXTEENTH_CENTURY_MAX_NOTES];
    SixteenthCenturyNote current[SIXTEENTH_CENTURY_MAX_NOTES];
    if (!entry_notes(point, 0, previous)) {
        return false;
    }
    for (size_t e = 1; e < point->entry_count; e++) {
        if (!entry_notes(point, e, current)) {
            return false;
        }
        const SixteenthCenturyNote *last = &previous[point->note_count - 1];
        if (current[0].start >= last->start + last->length) {
            return false;
        }
        for (size_t i = 0; i < point->note_count; i++) {
            previous[i] = current[i];
        }
    }
    return true;
}

bool sixteenth_century_imitation_rule_holds(SixteenthCenturyImitationRule which,
                                            const SixteenthCenturyPoint *point) {
    if (!sixteenth_century_rule_table_valid(&TABLE, (int)which) || !sixteenth_century_point_valid(point)) {
        return false;
    }
    switch (which) {
        case SIXTEENTH_CENTURY_IMITATION_RHYTHM_PRESERVED:
            return check_rhythm_preserved(point);
        case SIXTEENTH_CENTURY_IMITATION_CONTOUR_PRESERVED:
            return check_contour_preserved(point);
        case SIXTEENTH_CENTURY_IMITATION_NOT_FREE:
            return check_not_free(point);
        case SIXTEENTH_CENTURY_IMITATION_ENTRY_INTERVAL:
            return check_entry_interval(point);
        case SIXTEENTH_CENTURY_IMITATION_METRIC_MATCH:
            return check_metric_match(point);
        case SIXTEENTH_CENTURY_IMITATION_OVERLAP:
            return check_overlap(point);
        default:
            return false;
    }
}

static bool holds_adapter(int which, const void *subject) {
    return sixteenth_century_imitation_rule_holds((SixteenthCenturyImitationRule)which, subject);
}

bool sixteenth_century_imitation_check(const SixteenthCenturyPoint *point, unsigned *hard_violations,
                                       unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL || !sixteenth_century_point_valid(point)) {
        return false;
    }
    sixteenth_century_rule_table_check(&TABLE, holds_adapter, point, hard_violations, soft_violations);
    return true;
}
