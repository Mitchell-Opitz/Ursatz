#ifndef URSATZ_TESTS_SUPPORT_KEY_REGIONS_TEST_ACCESS_H
#define URSATZ_TESTS_SUPPORT_KEY_REGIONS_TEST_ACCESS_H

#include "analysis/harmony/key_regions.h"
#include "analysis/harmony/key_regions_internal.h"

/* What the tests read of the key regions beyond the public entry: the window scores, over the four stages of key_regions_internal.h
   (validate, gather the measures, score the windows, release). */

/*
 * Scores every window: from the first measure that holds a segment start to the last, window i covers
 * measures [first + i, first + i + window_measures - 1], and a piece with fewer measures than the window gets
 * one window over what it has. BAD_CONFIG for a zero window, a non-finite profile value, or a negative or
 * non-finite evidence_weight; BAD_SEGMENT for a
 * segment with a missing or negative time, an end not after its start, a segment out of order, a negative
 * or non-finite weight, or a chord with a root outside 0..11. config may be NULL for the defaults. On OK, *out owns its windows
 * (common_practice_key_windows_destroy); otherwise *out is untouched.
 */
static inline CommonPracticeKeyRegionsStatus key_window_scores(const CommonPracticeKeySegment *segments, size_t segment_count,
                                                               const MeterMap *map, const CommonPracticeKeyRegionsConfig *config,
                                                               CommonPracticeKeyWindows *out) {
    if (out == NULL) {
        return COMMON_PRACTICE_KEY_REGIONS_ERR_NULL_ARGUMENT;
    }
    CommonPracticeKeyRegionsConfig effective =
        config != NULL ? *config : common_practice_key_regions_config_default(NULL); /* the window scores read no materials */
    CommonPracticeKeyRegionsStatus status = key_regions_validate_inputs(segments, segment_count, map, &effective);
    if (status != COMMON_PRACTICE_KEY_REGIONS_OK) {
        return status;
    }
    MeasureWeights measures;
    status = key_regions_gather_measures(segments, segment_count, map, &measures);
    if (status != COMMON_PRACTICE_KEY_REGIONS_OK) {
        return status;
    }
    status = key_regions_score_windows(&measures, &effective, out);
    key_regions_measures_release(&measures);
    return status;
}

#endif
