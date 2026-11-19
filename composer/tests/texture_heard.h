#ifndef URSATZ_COMPOSER_TESTS_TEXTURE_HEARD_H
#define URSATZ_COMPOSER_TESTS_TEXTURE_HEARD_H

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <analysis/texture/texture_analysis_classify.h>
#include <analysis/texture/texture_analysis_input.h>
#include <theory/vocabulary/texture_rules.h>

#include <model/identity/entity_id.h>
#include <model/time/meter_map.h>
#include <model/time/musical_time.h>
#include <model/time/note_value.h>
#include <model/time/time_signature.h>
#include <model/time/time_span.h>

#include <plan/plan_lanes.h>
#include <plan/plan_state.h>

/*
 * How standard-texture hears a written plan: the melody, the bass and the inner voices over a span as three tracks (the
 * inner voices the voices of the third), read from the lanes of the plan, with the library's defaults. Worked out here
 * and not from the operator.
 */

typedef struct {
    size_t measures;      /* the windows the analysis made over the span */
    size_t heard;         /* those heard as the texture asked for, or as a texture whose category it is */
    int textures[64];     /* the texture of each window, in order, -1 for none (at most 64 are kept) */
} TextureHeard;

static inline bool texture_matches(int heard, int asked) {
    StandardTexture category;
    return heard >= 0 && (heard == asked || (standard_texture_category((StandardTexture)heard, &category) &&
                                              (int)category == asked));
}

/* Adds one note to the array, without its id (texture_heard names them once all are in, so the array may move). */
static inline void heard_add(AnalysisNote **notes, size_t *note_count, size_t track, size_t voice, Rational start,
                             Rational length, int pitch) {
    TimeSpan span;
    Rational end;
    assert(plan_span_end((PlanSpan){start, length}, &end) == PLAN_OK);
    assert(time_span_make(musical_time_of(start), musical_time_of(end), &span) == TIME_SPAN_OK);
    *notes = realloc(*notes, (*note_count + 1) * sizeof(**notes));
    assert(*notes);
    AnalysisNote note = {NULL, NULL, span, pitch, NULL, NULL, track, voice, 1.0};
    (*notes)[(*note_count)++] = note;
}

/* The texture of every measure of span, and how many of them are the texture asked for. */
static inline TextureHeard texture_heard(const PiecePlan *plan, PlanSpan span, PlanMeter meter, int asked) {
    TextureHeard result = {0, 0, {0}};
    NoteValue *unit = NULL;
    TimeSignature *signature = NULL;
    MusicalTime zero = musical_time_of(rational_ratio(0, 1));
    EntityID *id = NULL;
    MeterMap *map = NULL;
    AnalysisNote *notes = NULL;
    size_t note_count = 0;
    StandardTextureAnalysis analysis;
    int exponent = 0;
    for (int beat = meter.beat_unit; beat > 1; beat /= 2) {
        exponent++;
    }
    assert(note_value_create(exponent, &unit) == NOTE_VALUE_OK);
    assert(time_signature_create((unsigned)meter.beats_per_bar, unit, &signature) == TIME_SIGNATURE_OK);
    assert(entity_id_create("test-texture-meter", &id) == ENTITY_ID_OK);
    MeterMapChangePoint point = {signature, zero};
    assert(meter_map_create(id, &point, 1, &map) == METER_MAP_OK);
    for (int part = 0; part < 5; part++) {
        PlanLaneKind lane = part == 0 ? PLAN_LANE_MELODY : (part == 1 ? PLAN_LANE_BASS : PLAN_LANE_INNER_VOICE);
        int voice = part < 2 ? 0 : part - 1;
        LaneSpanId ids[256];
        size_t found = plan_lane_find(plan, lane, voice, span, ids, 256);
        assert(found <= 256);
        for (size_t i = 0; i < found; i++) {
            PlanSpan lane_span = plan_lane_span_span(plan, ids[i]);
            const PlanNotes *lane_notes = &plan_lane_span_payload(plan, ids[i])->notes;
            for (size_t n = 0; n < lane_notes->note_count; n++) {
                Rational start;
                assert(rational_add(lane_span.start, lane_notes->notes[n].offset, &start) == RATIONAL_OK);
                heard_add(&notes, &note_count, part < 2 ? (size_t)part : 2,
                          part < 2 ? 0 : (size_t)voice - 1, start, lane_notes->notes[n].duration, lane_notes->notes[n].midi);
            }
        }
    }
    char(*names)[STANDARD_TEXTURE_ID_MAX] = calloc(note_count > 0 ? note_count : 1, sizeof(*names));
    assert(names);
    for (size_t n = 0; n < note_count; n++) {
        snprintf(names[n], sizeof(names[n]), "n%zu", n);
        notes[n].note_id = names[n];
    }
    AnalysisNoteSet input = {notes, note_count, map};
    memset(&analysis, 0, sizeof(analysis));
    assert(standard_texture_analyze(&input, NULL, &analysis) == STANDARD_TEXTURE_ANALYSIS_OK);
    for (size_t w = 0; w < analysis.windows.count; w++) {
        result.textures[w < 64 ? w : 63] = analysis.window_texture[w];
        result.measures++;
        result.heard += texture_matches(analysis.window_texture[w], asked) ? 1 : 0;
    }
    standard_texture_analysis_destroy(&analysis);
    free(names);
    free(notes);
    meter_map_destroy(map);
    note_value_destroy(unit);
    time_signature_destroy(signature);
    return result;
}

#endif
