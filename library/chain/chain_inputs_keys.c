#include "chain/chain_inputs_internal.h"

#include <stdlib.h>

ChainInputStatus chain_build_key_segments(const ChainSegments *segments, size_t count, const CommonPracticeWeightedConfig *weighting,
                                          CommonPracticeKeySegment **out) {
    if (segments == NULL || out == NULL) {
        return CHAIN_INPUT_ERR_NULL_ARGUMENT;
    }
    if (segments->segments == NULL || count == 0 || count > segments->count) {
        return CHAIN_INPUT_ERR_BAD_NOTES;
    }
    CommonPracticeWeightedConfig defaults = common_practice_weighted_config_default();
    if (weighting == NULL) {
        weighting = &defaults;
    }
    CommonPracticeWeightedConfig evidence = *weighting;
    evidence.weigh_resolved_accents = true; /* the key reads the accented tones too: only chords skip them (R-386) */
    CommonPracticeKeySegment *built = calloc(count, sizeof(CommonPracticeKeySegment));
    if (built == NULL) {
        return CHAIN_INPUT_ERR_ALLOC;
    }
    for (size_t s = 0; s < count; s++) {
        const TimeSpan *window = sonority_time_window(segments->sonorities.items[s].sonority);
        built[s].start = window->start;
        built[s].end = window->end;
        built[s].source = &segments->segments[s];
        if (common_practice_weight_segment(segments->segments[s].members, segments->segments[s].member_count, &evidence,
                                           &built[s].weights, NULL) != COMMON_PRACTICE_WEIGHTED_OK) {
            free(built);
            return CHAIN_INPUT_ERR_WEIGHTS;
        }
    }
    *out = built;
    return CHAIN_INPUT_OK;
}

void chain_key_segments_destroy(CommonPracticeKeySegment *key_segments) {
    free(key_segments);
}

ChainInputStatus chain_estimate_key_regions(const CommonPracticeKeySegment *key_segments, size_t count, const MeterMap *map,
                                            const AnalysisKeyEvent *events, size_t event_count,
                                            const CommonPracticeKeyRegionsConfig *config, CommonPracticeKeyRegions *out) {
    if (key_segments == NULL || map == NULL || out == NULL || (event_count > 0 && events == NULL)) {
        return CHAIN_INPUT_ERR_NULL_ARGUMENT;
    }
    CommonPracticeKeySignatureEvent *signatures = NULL;
    if (event_count > 0) {
        signatures = calloc(event_count, sizeof(*signatures));
        if (signatures == NULL) {
            return CHAIN_INPUT_ERR_ALLOC;
        }
        for (size_t i = 0; i < event_count; i++) {
            if (!common_practice_key_signature_event_from_analysis(&events[i], &signatures[i])) {
                free(signatures);
                return CHAIN_INPUT_ERR_KEYS;
            }
        }
    }
    CommonPracticeKeyRegionsStatus status = key_regions_estimate(key_segments, count, map, signatures, event_count, config, out);
    free(signatures);
    return status == COMMON_PRACTICE_KEY_REGIONS_OK ? CHAIN_INPUT_OK : CHAIN_INPUT_ERR_KEYS;
}
