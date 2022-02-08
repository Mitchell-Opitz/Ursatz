#include "century_rules.h"

#include "century_dissonance_rules.h"
#include "century_harmonic_rules.h"
#include "century_melodic_rules.h"
#include "century_rhythm_rules.h"

static const char *const NAMES[SIXTEENTH_CENTURY_GROUP_COUNT] = {"melodic", "harmonic", "dissonance", "rhythm",
                                                                 "imitation"};

const char *sixteenth_century_group_name(SixteenthCenturyGroup group) {
    return group >= 0 && group < SIXTEENTH_CENTURY_GROUP_COUNT ? NAMES[group] : NULL;
}

static void record(SixteenthCenturyResult *result, unsigned group, unsigned hard, unsigned soft) {
    if (hard != 0) {
        result->hard_violations |= 1u << group;
    }
    if (soft != 0) {
        result->soft_violations |= 1u << group;
    }
}

bool sixteenth_century_check(const SixteenthCenturyInput *input, SixteenthCenturyResult *out) {
    if (input == NULL || out == NULL || !sixteenth_century_score_valid(input->score) ||
        (input->point_count > 0 && input->points == NULL)) {
        return false;
    }
    for (size_t i = 0; i < input->point_count; i++) {
        if (input->points[i].score != input->score || !sixteenth_century_point_valid(&input->points[i])) {
            return false;
        }
    }

    SixteenthCenturyResult result = {0, 0};
    unsigned hard, soft;

    if (!sixteenth_century_melodic_check(input->score, &hard, &soft)) {
        return false;
    }
    record(&result, SIXTEENTH_CENTURY_GROUP_MELODIC, hard, soft);

    if (!sixteenth_century_harmonic_check(input->score, &hard, &soft)) {
        return false;
    }
    record(&result, SIXTEENTH_CENTURY_GROUP_HARMONIC, hard, soft);

    if (!sixteenth_century_dissonance_check(input->score, &hard, &soft)) {
        return false;
    }
    record(&result, SIXTEENTH_CENTURY_GROUP_DISSONANCE, hard, soft);

    if (!sixteenth_century_rhythm_check(input->score, &hard, &soft)) {
        return false;
    }
    record(&result, SIXTEENTH_CENTURY_GROUP_RHYTHM, hard, soft);

    for (size_t i = 0; i < input->point_count; i++) {
        if (!sixteenth_century_imitation_check(&input->points[i], &hard, &soft)) {
            return false;
        }
        record(&result, SIXTEENTH_CENTURY_GROUP_IMITATION, hard, soft);
    }

    *out = result;
    return true;
}

bool sixteenth_century_valid(const SixteenthCenturyInput *input) {
    SixteenthCenturyResult result;
    return sixteenth_century_check(input, &result) && result.hard_violations == 0;
}
