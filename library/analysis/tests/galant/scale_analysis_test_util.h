#ifndef URSATZ_SCALE_HARMONIZATION_TESTS_SCALE_ANALYSIS_TEST_UTIL_H
#define URSATZ_SCALE_HARMONIZATION_TESTS_SCALE_ANALYSIS_TEST_UTIL_H

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis/galant/galant_analysis_input.h"
#include "analysis/galant/scale_analysis_detect.h"
#include "model/time/musical_time.h"
#include "model/time/time_span.h"

/* A run of segments for the unit tests, one beat (a quarter note) each from the start. */
typedef struct {
    GalantSegmentList list;
    TimeSpan **spans;
    size_t span_count;
    char **ids; /* the segment ids the list borrows */
    size_t id_count;
    int next_id;
} Run;

static inline void run_init(Run *run) {
    memset(run, 0, sizeof(*run));
    galant_segments_init(&run->list);
}

static inline void run_destroy(Run *run) {
    galant_segments_destroy(&run->list);
    for (size_t i = 0; i < run->span_count; i++) {
        time_span_destroy(run->spans[i]);
    }
    free(run->spans);
    for (size_t i = 0; i < run->id_count; i++) {
        free(run->ids[i]);
    }
    free(run->ids);
}

static inline GalantPitchDegree deg(int degree, int alteration) {
    GalantPitchDegree d = {degree, alteration};
    return d;
}

#define HOME deg(1, 0)

/* The next segment: bass and soprano degrees in the key (soprano 0: absent), the root of its numeral (0: none) and an applied-to degree. */
static inline void run_add(Run *run, int bass, int soprano, int root, int applied_to, double uncertainty) {
    int at = run->next_id;
    MusicalTime *from = NULL;
    MusicalTime *to = NULL;
    TimeSpan *span = NULL;
    assert(musical_time_create(at, 4, &from) == MUSICAL_TIME_OK && musical_time_create(at + 1, 4, &to) == MUSICAL_TIME_OK);
    assert(time_span_create(from, to, TIME_SPAN_BOUNDARY_LEFT_CLOSED, &span) == TIME_SPAN_OK);
    musical_time_destroy(from);
    musical_time_destroy(to);
    run->spans = (TimeSpan **)realloc(run->spans, (run->span_count + 1) * sizeof(TimeSpan *));
    run->spans[run->span_count++] = span;
    GalantSegment segment;
    memset(&segment, 0, sizeof(segment));
    char id[16];
    snprintf(id, sizeof(id), "s%d", run->next_id++);
    run->ids = (char **)realloc(run->ids, (run->id_count + 1) * sizeof(char *));
    run->ids[run->id_count] = (char *)malloc(strlen(id) + 1);
    assert(run->ids != NULL && run->ids[run->id_count] != NULL);
    strcpy(run->ids[run->id_count], id);
    segment.segment.id = run->ids[run->id_count++];
    snprintf(segment.key_id, sizeof(segment.key_id), "key-1-0");
    segment.segment.span = span;
    segment.segment.strength = BEAT_STRENGTH_WEAK;
    segment.beat_numerator = 1;
    segment.beat_denominator = 4;
    segment.key = HOME;
    segment.has_bass = bass != 0;
    segment.bass = deg(bass == 0 ? 1 : bass, 0);
    segment.has_soprano = soprano != 0;
    segment.soprano = deg(soprano == 0 ? 1 : soprano, 0);
    segment.has_numeral = root != 0;
    segment.root = deg(root == 0 ? 1 : root, 0);
    segment.applied_to = applied_to;
    segment.uncertainty = uncertainty;
    assert(galant_segments_add(&run->list, &segment) == GALANT_ANALYSIS_OK);
}

static inline void run_chords(Run *run, const int *basses, const int *sopranos, const int *roots, size_t count) {
    for (size_t i = 0; i < count; i++) {
        run_add(run, basses[i], sopranos[i], roots == NULL ? 0 : roots[i], 0, 0.0);
    }
}

static inline ScaleSequenceMatches detect(const Run *run, const ScaleDetectConfig *config) {
    ScaleSequenceMatches matches = {NULL, 0};
    assert(scale_sequence_detect(&run->list, config, &matches) == GALANT_ANALYSIS_OK);
    return matches;
}

static inline bool near_to(double a, double b) {
    return fabs(a - b) < 1e-9;
}

#endif
