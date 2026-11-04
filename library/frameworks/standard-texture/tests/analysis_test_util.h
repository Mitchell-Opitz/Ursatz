#ifndef URSATZ_FRAMEWORKS_STANDARD_TEXTURE_TESTS_ANALYSIS_TEST_UTIL_H
#define URSATZ_FRAMEWORKS_STANDARD_TEXTURE_TESTS_ANALYSIS_TEST_UTIL_H

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "identity/entity_id.h"
#include "texture_analysis_features.h"
#include "texture_analysis_input.h"
#include "time/meter_map.h"
#include "time/musical_time.h"
#include "time/note_value.h"
#include "time/time_signature.h"
#include "time/time_span.h"

/* A piece for the unit tests: one MeterMap, a StandardTextureInput over it, and the spans it borrows. */
typedef struct {
    MeterMap *map;
    StandardTextureInput input;
    TimeSpan **spans;
    size_t span_count;
    int next_id;
} Piece;

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

/* A piece in one signature: `top` over the unit with the NoteValue exponent (4, 2 is 4/4; 3, 2 is 3/4). */
static inline void piece_init(Piece *piece, unsigned top, int unit_exponent) {
    *piece = (Piece){0};
    NoteValue *unit = NULL;
    TimeSignature *signature = NULL;
    MusicalTime *zero = NULL;
    EntityID *id = NULL;
    assert(note_value_create(unit_exponent, &unit) == NOTE_VALUE_OK);
    assert(time_signature_create(top, unit, &signature) == TIME_SIGNATURE_OK);
    assert(musical_time_create(0, 1, &zero) == MUSICAL_TIME_OK);
    assert(entity_id_create("test-meter-map", &id) == ENTITY_ID_OK);
    MeterMapChangePoint point = {signature, zero};
    assert(meter_map_create(id, &point, 1, &piece->map) == METER_MAP_OK);
    note_value_destroy(unit);
    musical_time_destroy(zero);
    time_signature_destroy(signature);
    assert(standard_texture_input_init(&piece->input, piece->map) == STANDARD_TEXTURE_ANALYSIS_OK);
}

/* A note of `track`/`voice` from sn/sd to en/ed of a whole note, with the voice's confidence. */
static inline void piece_note(Piece *piece, size_t track, size_t voice, long pitch, long sn, unsigned long sd, long en,
                              unsigned long ed, double confidence) {
    char id[32];
    snprintf(id, sizeof(id), "n%d", piece->next_id++);
    assert(standard_texture_input_add_note(&piece->input, id, piece_span(piece, sn, sd, en, ed), pitch, track, voice, confidence) ==
           STANDARD_TEXTURE_ANALYSIS_OK);
}

static inline void piece_destroy(Piece *piece) {
    standard_texture_input_destroy(&piece->input);
    for (size_t i = 0; i < piece->span_count; i++) {
        time_span_destroy(piece->spans[i]);
    }
    free(piece->spans);
    meter_map_destroy(piece->map);
}

static inline Rational tq(long n, long d) {
    return rational_ratio(n, d);
}

/* An event on the grid. */
static inline StandardTextureEvent event(long on_n, long on_d, long off_n, long off_d, long pitch, size_t voice, double confidence) {
    StandardTextureEvent e = {tq(on_n, on_d), tq(off_n, off_d), pitch, voice, confidence};
    return e;
}

/* An event of a track. */
static inline StandardTextureEvent event_in_track(long on_n, long on_d, long off_n, long off_d, long pitch, size_t voice, size_t track,
                                                  double confidence) {
    StandardTextureEvent e = {tq(on_n, on_d), tq(off_n, off_d), pitch, voice, confidence, track};
    return e;
}

/* The window of one measure of 4/4: [0, 1) with a quarter-note beat. */
static inline StandardTextureWindow window_four_four(void) {
    StandardTextureWindow window = {tq(0, 1), tq(1, 1), tq(1, 4)};
    return window;
}

#endif
