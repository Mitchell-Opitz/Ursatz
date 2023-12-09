#ifndef URSATZ_STANDARD_HARMONY_TESTS_ANALYSIS_TEST_UTIL_H
#define URSATZ_STANDARD_HARMONY_TESTS_ANALYSIS_TEST_UTIL_H

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "analysis_function.h"
#include "analysis_input.h"
#include "time/duration.h"
#include "time/musical_time.h"

/*
 * A small fixture of segments for the adapter tests: each segment is a quarter note in 4/4 (measure 1, beats
 * 1..4, then measure 2 ...), so the span, the MetricalPosition and the BeatStrength are consistent. The segment
 * owns its TimeSpan and MetricalPosition; the Fixture owns the segments.
 */

#define FIXTURE_MAX 32

typedef struct {
    HarmonySegmentInput inputs[FIXTURE_MAX];
    char ids[FIXTURE_MAX][16];
    char key_ref[8];
    TimeSpan *spans[FIXTURE_MAX];
    MetricalPosition *positions[FIXTURE_MAX];
    size_t count;
} Fixture;

static const int TRIAD_MAJOR[3] = {0, 4, 7};
static const int TRIAD_MINOR[3] = {0, 3, 7};
static const int TRIAD_DIM[3] = {0, 3, 6};
static const int SEVENTH_DOMINANT[4] = {0, 4, 7, 10};

static inline void fixture_init(Fixture *f) {
    memset(f, 0, sizeof(*f));
    snprintf(f->key_ref, sizeof(f->key_ref), "C");
}

/* The beat strength of beat `beat` (1..4) of a 4/4 measure. */
static inline BeatStrength fixture_strength(int beat) {
    return beat == 1 ? BEAT_STRENGTH_STRONG : (beat == 3 ? BEAT_STRENGTH_MEDIUM : BEAT_STRENGTH_WEAK);
}

/* Adds a quarter-note segment: index n (0-based) is measure n/4 + 1, beat n%4 + 1. */
static inline HarmonySegmentInput *fixture_add(Fixture *f, int degree, const int *intervals, size_t interval_count,
                                               int inversion, int bass_degree, int soprano_degree) {
    size_t n = f->count++;
    assert(n < FIXTURE_MAX);
    HarmonySegmentInput *in = &f->inputs[n];
    snprintf(f->ids[n], sizeof(f->ids[n]), "seg-%zu", n);
    MusicalTime *start = NULL;
    MusicalTime *end = NULL;
    assert(musical_time_create((long)n, 4, &start) == MUSICAL_TIME_OK);
    assert(musical_time_create((long)n + 1, 4, &end) == MUSICAL_TIME_OK);
    assert(time_span_create(start, end, TIME_SPAN_BOUNDARY_LEFT_CLOSED, &f->spans[n]) == TIME_SPAN_OK);
    musical_time_destroy(start);
    musical_time_destroy(end);
    Duration *offset = NULL;
    assert(duration_create((long)(n % 4), 4, &offset) == DURATION_OK);
    assert(metrical_position_create((int)(n / 4) + 1, offset, &f->positions[n]) == METRICAL_POSITION_OK);
    duration_destroy(offset);

    memset(in, 0, sizeof(*in));
    in->segment_id = f->ids[n];
    in->span = f->spans[n];
    in->position = f->positions[n];
    in->beat_strength = fixture_strength((int)(n % 4) + 1);
    in->degree = degree;
    for (size_t i = 0; i < interval_count; i++) {
        in->intervals[i] = intervals[i];
    }
    in->interval_count = interval_count;
    in->inversion = inversion;
    in->bass_degree = bass_degree;
    in->soprano_degree = soprano_degree;
    in->key_ref = f->key_ref;
    in->mode = HARMONY_MODE_MAJOR;
    in->uncertainty = 0.0;
    return in;
}

static inline void fixture_destroy(Fixture *f) {
    for (size_t i = 0; i < f->count; i++) {
        time_span_destroy(f->spans[i]);
        metrical_position_destroy(f->positions[i]);
    }
}

#endif
