#ifndef URSATZ_GALANT_SCHEMAS_TESTS_GALANT_ANALYSIS_TEST_UTIL_H
#define URSATZ_GALANT_SCHEMAS_TESTS_GALANT_ANALYSIS_TEST_UTIL_H

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "galant_analysis_input.h"
#include "galant_analysis_match.h"
#include "time/musical_time.h"
#include "time/time_span.h"

/* A run of segments for the unit tests. Time is counted in eighth notes ("units"); a beat is two units (a quarter note). */
typedef struct {
    GalantSegmentList list;
    TimeSpan **spans;
    size_t span_count;
    int next_id;
} Seq;

static inline void seq_init(Seq *seq) {
    memset(seq, 0, sizeof(*seq));
    galant_segments_init(&seq->list);
}

static inline void seq_destroy(Seq *seq) {
    galant_segments_destroy(&seq->list);
    for (size_t i = 0; i < seq->span_count; i++) {
        time_span_destroy(seq->spans[i]);
    }
    free(seq->spans);
}

static inline GalantPitchDegree deg(int degree, int alteration) {
    GalantPitchDegree d = {degree, alteration};
    return d;
}

/* A segment from unit `start` for `length` units; `bass` and `soprano` are degrees in the local key `key` (soprano degree 0: absent). */
static inline void seq_add(Seq *seq, int start, int length, BeatStrength strength, GalantPitchDegree bass, GalantPitchDegree soprano,
                           GalantPitchDegree key, double uncertainty) {
    MusicalTime *from = NULL;
    MusicalTime *to = NULL;
    TimeSpan *span = NULL;
    assert(musical_time_create(start, 8, &from) == MUSICAL_TIME_OK && musical_time_create(start + length, 8, &to) == MUSICAL_TIME_OK);
    assert(time_span_create(from, to, TIME_SPAN_BOUNDARY_LEFT_CLOSED, &span) == TIME_SPAN_OK);
    musical_time_destroy(from);
    musical_time_destroy(to);
    seq->spans = (TimeSpan **)realloc(seq->spans, (seq->span_count + 1) * sizeof(TimeSpan *));
    seq->spans[seq->span_count++] = span;
    GalantSegment segment;
    memset(&segment, 0, sizeof(segment));
    snprintf(segment.id, sizeof(segment.id), "s%d", seq->next_id++);
    snprintf(segment.key_id, sizeof(segment.key_id), "key-%d-%d", key.degree, key.alteration);
    segment.span = span;
    segment.strength = strength;
    segment.beat_numerator = 1;
    segment.beat_denominator = 4;
    segment.key = key;
    segment.has_bass = bass.degree != 0;
    segment.bass = bass;
    segment.has_soprano = soprano.degree != 0;
    segment.soprano = soprano;
    segment.uncertainty = uncertainty;
    assert(galant_segments_add(&seq->list, &segment) == GALANT_ANALYSIS_OK);
}

#define HOME deg(1, 0)
#define NO_SOPRANO deg(0, 0)

/* One stage per beat in the home key: four stages from unit 0, strengths S W M W (a downbeat, a weak beat, the secondary beat, a weak beat). */
static inline void seq_stages(Seq *seq, int start, const BeatStrength *strengths, const int *basses, const int *sopranos, size_t count) {
    for (size_t k = 0; k < count; k++) {
        seq_add(seq, start + 2 * (int)k, 2, strengths[k], deg(basses[k], 0), sopranos[k] == 0 ? NO_SOPRANO : deg(sopranos[k], 0), HOME, 0.0);
    }
}

static inline GalantSchemaMatches find_matches(const Seq *seq, const GalantMatchConfig *config) {
    GalantSchemaMatches matches = {NULL, 0};
    assert(galant_schema_match(&seq->list, config, &matches) == GALANT_ANALYSIS_OK);
    return matches;
}

static inline const GalantSchemaMatch *find_schema(const GalantSchemaMatches *matches, const char *name) {
    for (size_t i = 0; i < matches->count; i++) {
        if (strcmp(matches->items[i].claim.name, name) == 0) {
            return &matches->items[i];
        }
    }
    return NULL;
}

static inline bool near_to(double a, double b) {
    return fabs(a - b) < 1e-9;
}

#endif
