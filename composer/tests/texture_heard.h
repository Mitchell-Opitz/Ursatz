#ifndef URSATZ_COMPOSER_TESTS_TEXTURE_HEARD_H
#define URSATZ_COMPOSER_TESTS_TEXTURE_HEARD_H

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <standard-texture/analysis/texture_analysis_classify.h>
#include <standard-texture/analysis/texture_analysis_input.h>
#include <standard-texture/texture_rules.h>

#include <identity/entity_id.h>
#include <time/meter_map.h>
#include <time/musical_time.h>
#include <time/note_value.h>
#include <time/time_signature.h>
#include <time/time_span.h>

#include <ursatz_composer/lanes.h>
#include <ursatz_composer/plan_state.h>

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

static inline void heard_add(StandardTextureInput *input, TimeSpan ***spans, size_t *span_count, unsigned *next, size_t track,
                             size_t voice, Rational start, Rational length, int pitch) {
    MusicalTime *from = NULL;
    MusicalTime *to = NULL;
    TimeSpan *span = NULL;
    char id[STANDARD_TEXTURE_ID_MAX];
    Rational end;
    assert(plan_span_end((PlanSpan){start, length}, &end) == PLAN_OK);
    assert(musical_time_create(start.num, start.den, &from) == MUSICAL_TIME_OK);
    assert(musical_time_create(end.num, end.den, &to) == MUSICAL_TIME_OK);
    assert(time_span_create(from, to, TIME_SPAN_BOUNDARY_LEFT_CLOSED, &span) == TIME_SPAN_OK);
    musical_time_destroy(from);
    musical_time_destroy(to);
    *spans = realloc(*spans, (*span_count + 1) * sizeof(**spans));
    assert(*spans);
    (*spans)[(*span_count)++] = span;
    snprintf(id, sizeof(id), "n%u", (*next)++);
    assert(standard_texture_input_add_note(input, id, span, pitch, track, voice, 1.0) == STANDARD_TEXTURE_ANALYSIS_OK);
}

/* The texture of every measure of span, and how many of them are the texture asked for. */
static inline TextureHeard texture_heard(const PiecePlan *plan, PlanSpan span, PlanMeter meter, int asked) {
    TextureHeard result = {0, 0, {0}};
    NoteValue *unit = NULL;
    TimeSignature *signature = NULL;
    MusicalTime *zero = NULL;
    EntityID *id = NULL;
    MeterMap *map = NULL;
    StandardTextureInput input;
    StandardTextureAnalysis analysis;
    TimeSpan **spans = NULL;
    size_t span_count = 0;
    unsigned next = 0;
    int exponent = 0;
    for (int beat = meter.beat_unit; beat > 1; beat /= 2) {
        exponent++;
    }
    assert(note_value_create(exponent, &unit) == NOTE_VALUE_OK);
    assert(time_signature_create((unsigned)meter.beats_per_bar, unit, &signature) == TIME_SIGNATURE_OK);
    assert(musical_time_create(0, 1, &zero) == MUSICAL_TIME_OK && entity_id_create("test-texture-meter", &id) == ENTITY_ID_OK);
    MeterMapChangePoint point = {signature, zero};
    assert(meter_map_create(id, &point, 1, &map) == METER_MAP_OK);
    assert(standard_texture_input_init(&input, map) == STANDARD_TEXTURE_ANALYSIS_OK);

    for (int part = 0; part < 5; part++) {
        PlanLaneKind lane = part == 0 ? PLAN_LANE_MELODY : (part == 1 ? PLAN_LANE_BASS : PLAN_LANE_INNER_VOICE);
        int voice = part < 2 ? 0 : part - 1;
        LaneSpanId ids[256];
        size_t found = plan_lane_find(plan, lane, voice, span, ids, 256);
        assert(found <= 256);
        for (size_t i = 0; i < found; i++) {
            PlanSpan lane_span = plan_lane_span_span(plan, ids[i]);
            const PlanNotes *notes = &plan_lane_span_payload(plan, ids[i])->notes;
            for (size_t n = 0; n < notes->note_count; n++) {
                Rational start;
                assert(rational_add(lane_span.start, notes->notes[n].offset, &start) == RATIONAL_OK);
                heard_add(&input, &spans, &span_count, &next, part < 2 ? (size_t)part : 2, part < 2 ? 0 : (size_t)voice - 1,
                          start, notes->notes[n].duration, notes->notes[n].midi);
            }
        }
    }
    memset(&analysis, 0, sizeof(analysis));
    assert(standard_texture_analyze(&input, NULL, &analysis) == STANDARD_TEXTURE_ANALYSIS_OK);
    for (size_t w = 0; w < analysis.windows.count; w++) {
        result.textures[w < 64 ? w : 63] = analysis.window_texture[w];
        result.measures++;
        result.heard += texture_matches(analysis.window_texture[w], asked) ? 1 : 0;
    }
    standard_texture_analysis_destroy(&analysis);
    standard_texture_input_destroy(&input);
    for (size_t i = 0; i < span_count; i++) {
        time_span_destroy(spans[i]);
    }
    free(spans);
    meter_map_destroy(map);
    note_value_destroy(unit);
    time_signature_destroy(signature);
    musical_time_destroy(zero);
    return result;
}

#endif
