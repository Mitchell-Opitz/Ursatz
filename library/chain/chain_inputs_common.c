#include "chain/chain_inputs_internal.h"

#include <stdlib.h>
#include <string.h>

#include "model/time/time_rational.h"

int chain_compare_keys(const void *a, const void *b) {
    return strcmp(((const NoteKey *)a)->id, ((const NoteKey *)b)->id);
}

bool chain_rational_of(const MusicalTime *time, Rational *out) {
    return rational_of_musical_time(time, out) == RATIONAL_OK;
}

int chain_measure_of(const MeterMap *map, const MusicalTime *time) {
    int measure = 0;
    return meter_map_measure_index_at(map, time, &measure) == METER_MAP_OK ? measure : -1;
}

bool chain_parse_degree(const char *label, int *degree, int *alteration) {
    *alteration = 0;
    if (label == NULL) {
        return false;
    }
    if (*label == 'b') {
        *alteration = -1;
        label++;
    } else if (*label == '#') {
        *alteration = 1;
        label++;
    }
    if (*label < '1' || *label > '7') {
        return false;
    }
    *degree = *label - '0';
    return true;
}

const TimeSpan *chain_keep_span(TimeSpan ***spans, size_t *count, size_t *capacity, const MusicalTime *start,
                                       const MusicalTime *end) {
    if (*count == *capacity) {
        size_t grown_capacity = *capacity == 0 ? 256 : *capacity * 2;
        TimeSpan **grown = realloc(*spans, grown_capacity * sizeof(TimeSpan *));
        if (grown == NULL) {
            return NULL;
        }
        *spans = grown;
        *capacity = grown_capacity;
    }
    TimeSpan *span = NULL;
    if (time_span_create(start, end, TIME_SPAN_BOUNDARY_LEFT_CLOSED, &span) != TIME_SPAN_OK) {
        return NULL;
    }
    (*spans)[(*count)++] = span;
    return span;
}

void chain_release_spans(TimeSpan **spans, size_t count) {
    for (size_t i = 0; i < count; i++) {
        time_span_destroy(spans[i]);
    }
    free(spans);
}

