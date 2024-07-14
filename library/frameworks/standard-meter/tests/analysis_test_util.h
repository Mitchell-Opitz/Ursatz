#ifndef URSATZ_FRAMEWORKS_STANDARD_METER_TESTS_ANALYSIS_TEST_UTIL_H
#define URSATZ_FRAMEWORKS_STANDARD_METER_TESTS_ANALYSIS_TEST_UTIL_H

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "identity/entity_id.h"
#include "meter_analysis_input.h"
#include "meter_test_util.h"
#include "time/meter_map.h"
#include "time/musical_time.h"
#include "time/time_span.h"

/* A piece for the unit tests: one MeterMap, a StandardMeterInput over it, and the spans and times it borrows. */
typedef struct {
    MeterMap *map;
    StandardMeterInput input;
    TimeSpan **spans;
    size_t span_count;
    MusicalTime **times;
    size_t time_count;
    int next_id;
} Piece;

static inline MusicalTime *piece_time(Piece *piece, long n, unsigned long d) {
    MusicalTime *time = NULL;
    assert(musical_time_create(n, d, &time) == MUSICAL_TIME_OK);
    piece->times = (MusicalTime **)realloc(piece->times, (piece->time_count + 1) * sizeof(MusicalTime *));
    piece->times[piece->time_count++] = time;
    return time;
}

static inline const TimeSpan *piece_span(Piece *piece, long sn, unsigned long sd, long en, unsigned long ed) {
    MusicalTime *start = NULL;
    MusicalTime *end = NULL;
    TimeSpan *span = NULL;
    assert(musical_time_create(sn, sd, &start) == MUSICAL_TIME_OK && musical_time_create(en, ed, &end) == MUSICAL_TIME_OK);
    assert(time_span_create(start, end, TIME_SPAN_BOUNDARY_LEFT_CLOSED, &span) == TIME_SPAN_OK);
    musical_time_destroy(start);
    musical_time_destroy(end);
    piece->spans = (TimeSpan **)realloc(piece->spans, (piece->span_count + 1) * sizeof(TimeSpan *));
    piece->spans[piece->span_count++] = span;
    return span;
}

/* A piece in one signature: `top` over the unit with the NoteValue exponent (3, 2 is 3/4; 6, 3 is 6/8). */
static inline void piece_init(Piece *piece, unsigned top, int unit_exponent) {
    *piece = (Piece){0};
    TimeSignature *signature = make_signature(top, unit_exponent);
    MusicalTime *zero = NULL;
    EntityID *id = NULL;
    assert(musical_time_create(0, 1, &zero) == MUSICAL_TIME_OK);
    assert(entity_id_create("test-meter-map", &id) == ENTITY_ID_OK);
    MeterMapChangePoint point = {signature, zero};
    assert(meter_map_create(id, &point, 1, &piece->map) == METER_MAP_OK);
    musical_time_destroy(zero);
    time_signature_destroy(signature);
    assert(standard_meter_input_init(&piece->input, piece->map) == STANDARD_METER_ANALYSIS_OK);
}

/* A note on `line` from sn/sd to en/ed of a whole note. */
static inline void piece_note(Piece *piece, int line, long sn, unsigned long sd, long en, unsigned long ed) {
    char id[32];
    snprintf(id, sizeof(id), "n%d", piece->next_id++);
    assert(standard_meter_input_add_note(&piece->input, id, piece_span(piece, sn, sd, en, ed), 60, line) ==
           STANDARD_METER_ANALYSIS_OK);
}

static inline void piece_chord_change(Piece *piece, long n, unsigned long d) {
    char id[32];
    snprintf(id, sizeof(id), "c%d", piece->next_id++);
    assert(standard_meter_input_add_chord_change(&piece->input, id, piece_time(piece, n, d)) == STANDARD_METER_ANALYSIS_OK);
}

static inline void piece_phrase(Piece *piece, long sn, unsigned long sd, long en, unsigned long ed, bool cadence,
                                long cn, unsigned long cd, long cen, unsigned long ced, StandardMeterExpansionKind expansion,
                                int expansion_measures, bool elided) {
    char id[32];
    snprintf(id, sizeof(id), "p%d", piece->next_id++);
    const TimeSpan *span = piece_span(piece, sn, sd, en, ed);
    const TimeSpan *arrival = cadence ? piece_span(piece, cn, cd, cen, ced) : NULL;
    assert(standard_meter_input_add_phrase(&piece->input, id, span, arrival, expansion, expansion_measures, elided) ==
           STANDARD_METER_ANALYSIS_OK);
}

static inline void piece_destroy(Piece *piece) {
    standard_meter_input_destroy(&piece->input);
    for (size_t i = 0; i < piece->span_count; i++) {
        time_span_destroy(piece->spans[i]);
    }
    for (size_t i = 0; i < piece->time_count; i++) {
        musical_time_destroy(piece->times[i]);
    }
    free(piece->spans);
    free(piece->times);
    meter_map_destroy(piece->map);
}

#endif
