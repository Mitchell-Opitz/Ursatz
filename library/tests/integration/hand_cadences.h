#ifndef URSATZ_TESTS_INTEGRATION_HAND_CADENCES_H
#define URSATZ_TESTS_INTEGRATION_HAND_CADENCES_H

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "analysis_cadence.h"
#include "ursatz_chain/chain_inputs.h"

/*
 * Cadences given by an annotation (a type and the measure they arrive in) as the HarmonyCadences the chain's input builders take. A hand
 * cadence arrives over the segments of its measure: from the first segment that starts in it to the last that ends in it. Nothing else of
 * a harmony result is set: the builders read the type, the arrival segments, post_cadential and uncertainty.
 */

typedef struct {
    int measure;
    CadenceType type;
} HandCadence;

static inline int hand_time_compare(const MusicalTime *a, const MusicalTime *b) {
    long long left = (long long)musical_time_numerator(a) * (long long)musical_time_denominator(b);
    long long right = (long long)musical_time_numerator(b) * (long long)musical_time_denominator(a);
    return left < right ? -1 : (left > right ? 1 : 0);
}

static inline HarmonyCadences hand_cadences_make(const ChainSegments *segments, const MeterMap *map, const HandCadence *hand, size_t count) {
    HarmonyCadences cadences = {NULL, count};
    cadences.items = (HarmonyCadenceResult *)calloc(count > 0 ? count : 1, sizeof(HarmonyCadenceResult));
    assert(cadences.items != NULL);
    for (size_t i = 0; i < count; i++) {
        TimeSpan *measure = NULL;
        assert(meter_map_measure_span(map, hand[i].measure, &measure) == METER_MAP_OK);
        bool found = false;
        for (size_t s = 0; s < segments->count; s++) {
            const TimeSpan *window = sonority_time_window(segments->sonorities.items[s].sonority);
            if (hand_time_compare(time_span_start(window), time_span_start(measure)) < 0 ||
                hand_time_compare(time_span_end(window), time_span_end(measure)) > 0) {
                continue;
            }
            if (!found) {
                cadences.items[i].arrival_first = s;
                found = true;
            }
            cadences.items[i].arrival_last = s;
        }
        assert(found);
        cadences.items[i].type = hand[i].type;
        time_span_destroy(measure);
    }
    return cadences;
}

static inline void hand_cadences_destroy(HarmonyCadences *cadences) {
    free(cadences->items);
    memset(cadences, 0, sizeof(*cadences));
}

#endif
