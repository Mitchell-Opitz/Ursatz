#include "seventh_identification.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#define SEVENTH_INTERVAL_COUNT 4

/* Root-relative semitone intervals (0-11) for each diatonic seventh-chord
   quality, compared as an unordered set -- see header for the tie-break
   rule. */
static const int SEVENTH_INTERVALS[COMMON_PRACTICE_SEVENTH_MAJOR_KEY_COUNT][SEVENTH_INTERVAL_COUNT] = {
    {0, 4, 7, 10}, /* dominant7 */
    {0, 4, 7, 11}, /* major7 */
    {0, 3, 7, 10}, /* minor7 */
    {0, 3, 6, 10}, /* half-diminished7 */
};

static bool contains_interval(const int *intervals, size_t count, int interval) {
    for (size_t i = 0; i < count; i++) {
        if (intervals[i] == interval) {
            return true;
        }
    }
    return false;
}

static size_t count_distinct(const int *root_relative_intervals, size_t interval_count, int *out_distinct,
                              size_t out_capacity) {
    size_t distinct_count = 0;
    for (size_t i = 0; i < interval_count; i++) {
        int interval = root_relative_intervals[i];
        if (interval < 0 || interval > 11) {
            continue;
        }
        if (!contains_interval(out_distinct, distinct_count, interval) && distinct_count < out_capacity) {
            out_distinct[distinct_count++] = interval;
        }
    }
    return distinct_count;
}

bool common_practice_identify_seventh(const int *root_relative_intervals, size_t interval_count,
                                       CommonPracticeSeventh *out) {
    if (root_relative_intervals == NULL || out == NULL) {
        return false;
    }

    int distinct[12];
    size_t distinct_count = count_distinct(root_relative_intervals, interval_count, distinct, 12);
    if (distinct_count < SEVENTH_INTERVAL_COUNT) {
        return false;
    }

    bool found = false;
    CommonPracticeSeventh picked = (CommonPracticeSeventh)0;
    for (int seventh = 0; seventh < COMMON_PRACTICE_SEVENTH_MAJOR_KEY_COUNT; seventh++) {
        bool matches = true;
        for (int i = 0; i < SEVENTH_INTERVAL_COUNT; i++) {
            if (!contains_interval(distinct, distinct_count, SEVENTH_INTERVALS[seventh][i])) {
                matches = false;
                break;
            }
        }
        if (!matches) {
            continue;
        }
        if (!found) {
            found = true;
            picked = (CommonPracticeSeventh)seventh;
        } else {
            fprintf(stderr,
                    "common_practice_identify_seventh: ambiguous subset match -- picked %s, also fits %s "
                    "(scan-order tie-break, see seventh_identification.h)\n",
                    common_practice_seventh_label(picked), common_practice_seventh_label((CommonPracticeSeventh)seventh));
        }
    }

    if (!found) {
        return false;
    }
    *out = picked;
    return true;
}

bool common_practice_identify_seventh_chord(const int *root_relative_intervals, size_t interval_count,
                                             EntityID *sonority_target_id, Chord **out_chord) {
    CommonPracticeSeventh seventh;
    if (!common_practice_identify_seventh(root_relative_intervals, interval_count, &seventh)) {
        return false;
    }
    return common_practice_build_seventh_chord(seventh, sonority_target_id, out_chord) == CHORD_OK;
}
