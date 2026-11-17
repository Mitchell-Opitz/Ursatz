#ifndef URSATZ_FRAMEWORKS_STANDARD_TEXTURE_TESTS_ANALYSIS_TEST_UTIL_H
#define URSATZ_FRAMEWORKS_STANDARD_TEXTURE_TESTS_ANALYSIS_TEST_UTIL_H

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "model/identity/entity_id.h"
#include "analysis/notes/analysis_note.h"
#include "analysis/texture/texture_analysis_features.h"
#include "analysis/texture/texture_analysis_input.h"
#include "model/time/meter_map.h"
#include "model/time/musical_time.h"
#include "model/time/note_value.h"
#include "model/time/time_signature.h"
#include "model/time/time_span.h"

/* A piece for the unit tests: one MeterMap, the AnalysisNoteSet over its notes (kept current by piece_note), and the
   spans and ids the notes borrow. */
typedef struct {
    MeterMap *map;
    AnalysisNoteSet input;
    AnalysisNote *notes;
    char **ids;
    size_t capacity;
    int next_id;
} Piece;

static inline TimeSpan whole_span(long sn, unsigned long sd, long en, unsigned long ed) {
    MusicalTime start;
    MusicalTime end;
    TimeSpan span;
    assert(musical_time_make(sn, (long long)sd, &start) == MUSICAL_TIME_OK && musical_time_make(en, (long long)ed, &end) == MUSICAL_TIME_OK);
    assert(time_span_make(start, end, &span) == TIME_SPAN_OK);
    return span;
}

/* A piece in one signature: `top` over the unit with the NoteValue exponent (4, 2 is 4/4; 3, 2 is 3/4). */
static inline void piece_init(Piece *piece, unsigned top, int unit_exponent) {
    *piece = (Piece){0};
    NoteValue *unit = NULL;
    TimeSignature *signature = NULL;
    MusicalTime zero;
    EntityID *id = NULL;
    assert(note_value_create(unit_exponent, &unit) == NOTE_VALUE_OK);
    assert(time_signature_create(top, unit, &signature) == TIME_SIGNATURE_OK);
    assert(musical_time_make(0, 1, &zero) == MUSICAL_TIME_OK);
    assert(entity_id_create("test-meter-map", &id) == ENTITY_ID_OK);
    MeterMapChangePoint point = {signature, zero};
    assert(meter_map_create(id, &point, 1, &piece->map) == METER_MAP_OK);
    note_value_destroy(unit);
    time_signature_destroy(signature);
    piece->input.map = piece->map;
}

/* A note of `track`/`voice` from sn/sd to en/ed of a whole note, with the voice's confidence. */
static inline void piece_note(Piece *piece, size_t track, size_t voice, long pitch, long sn, unsigned long sd, long en,
                              unsigned long ed, double confidence) {
    if (piece->input.count == piece->capacity) {
        piece->capacity = piece->capacity == 0 ? 64 : piece->capacity * 2;
        piece->notes = (AnalysisNote *)realloc(piece->notes, piece->capacity * sizeof(AnalysisNote));
        piece->ids = (char **)realloc(piece->ids, piece->capacity * sizeof(char *));
        assert(piece->notes != NULL && piece->ids != NULL);
    }
    char id[32];
    snprintf(id, sizeof(id), "n%d", piece->next_id++);
    size_t at = piece->input.count;
    piece->ids[at] = (char *)malloc(strlen(id) + 1);
    assert(piece->ids[at] != NULL);
    strcpy(piece->ids[at], id);
    AnalysisNote note = {piece->ids[at], NULL, whole_span(sn, sd, en, ed), (int)pitch, NULL, NULL, track, voice, confidence};
    piece->notes[at] = note;
    piece->input.notes = piece->notes;
    piece->input.count = at + 1;
}

static inline void piece_destroy(Piece *piece) {
    for (size_t i = 0; i < piece->input.count; i++) {
        free(piece->ids[i]);
    }
    free(piece->ids);
    free(piece->notes);
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
