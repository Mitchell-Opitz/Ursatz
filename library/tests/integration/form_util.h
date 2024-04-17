#ifndef URSATZ_TESTS_INTEGRATION_FORM_UTIL_H
#define URSATZ_TESTS_INTEGRATION_FORM_UTIL_H

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis/outer_voice.h"
#include "analysis_input.h"
#include "form_kernel_bridge.h"
#include "weighted_pipeline.h"

/*
 * Glue for the form analysis: builds the neutral input of standard-form's adapter (analysis_input.h) from the L2 outer-voice
 * soprano, the cadences (the standard-harmony adapter's, or a hand list), the segments and the MeterMap. standard-form includes
 * none of these; this glue, which includes them all, lives in the tests.
 *
 * The soprano. Consecutive segments in which the soprano is the same note are ONE note (a held note); the note's span runs from
 * the start of its first segment to the end of its last, so the notes are in time order and do not overlap; its semitone is the
 * line's pitch; its NoteEvent is found by id. A segment where the soprano is absent is a gap. Only segments that start in the
 * measures [first, last] are used.
 *
 * The cadences are added by the caller, each with the arrival TimeSpan (copied here): standard-harmony cadences with the arrival
 * run's span, hand cadences with the arrival measure's.
 */

typedef struct {
    StandardFormInput input;
    TimeSpan **spans; /* owned: every span copied into the input */
    size_t span_count;
    size_t span_capacity;
} FormBuild;

static inline void form_build_init(FormBuild *build, const MeterMap *map) {
    memset(build, 0, sizeof(*build));
    standard_form_input_init(&build->input, map);
}

static inline void form_build_destroy(FormBuild *build) {
    standard_form_input_destroy(&build->input);
    for (size_t i = 0; i < build->span_count; i++) {
        time_span_destroy(build->spans[i]);
    }
    free(build->spans);
    memset(build, 0, sizeof(*build));
}

/* A span [start, end) kept by the build. */
static inline const TimeSpan *form_build_span(FormBuild *build, const MusicalTime *start, const MusicalTime *end) {
    if (build->span_count == build->span_capacity) {
        build->span_capacity = build->span_capacity == 0 ? 256 : build->span_capacity * 2;
        build->spans = (TimeSpan **)realloc(build->spans, build->span_capacity * sizeof(TimeSpan *));
        assert(build->spans != NULL);
    }
    TimeSpan *span = NULL;
    assert(time_span_create(start, end, TIME_SPAN_BOUNDARY_LEFT_CLOSED, &span) == TIME_SPAN_OK);
    build->spans[build->span_count++] = span;
    return span;
}

static inline int form_measure_of(const MeterMap *map, const MusicalTime *time) {
    int measure = 0;
    assert(meter_map_measure_index_at(map, time, &measure) == METER_MAP_OK);
    return measure;
}

static inline const NoteEvent *form_find_event(const WeightedPipeline *p, const char *id) {
    for (size_t i = 0; i < p->note_count; i++) {
        if (strcmp(entity_id_value(note_event_id(p->events[i])), id) == 0) {
            return p->events[i];
        }
    }
    return NULL;
}

/* The soprano of segments starting in measures [first, last] as notes; every segment of the pipeline as a segment. */
static inline void form_build_melody(FormBuild *build, const WeightedPipeline *p, const OuterVoiceLines *lines,
                                     int first_measure, int last_measure) {
    const char *current_id = NULL;
    const MusicalTime *current_start = NULL;
    const MusicalTime *current_end = NULL;
    long current_semitone = 0;
    for (size_t s = 0; s <= p->segment_count; s++) {
        const TimeSpan *window = s < p->segment_count ? sonority_time_window(p->sonorities.items[s].sonority) : NULL;
        bool in_range = window != NULL && form_measure_of(build->input.map, time_span_start(window)) >= first_measure &&
                        form_measure_of(build->input.map, time_span_start(window)) <= last_measure;
        const char *id = in_range && lines->soprano[s].present ? entity_id_value(lines->soprano[s].note_id) : NULL;
        if (current_id != NULL && (id == NULL || strcmp(id, current_id) != 0)) {
            const NoteEvent *event = form_find_event(p, current_id);
            assert(event != NULL);
            const TimeSpan *span = form_build_span(build, current_start, current_end);
            assert(standard_form_input_add_note(&build->input, current_id, event, span, NULL, current_semitone) ==
                   STANDARD_FORM_ANALYSIS_OK);
            current_id = NULL;
        }
        if (id == NULL) {
            continue;
        }
        if (current_id == NULL) {
            current_id = id;
            current_start = time_span_start(window);
            current_semitone = pitch_numeric_value_position_numerator(lines->soprano[s].pitch);
        }
        current_end = time_span_end(window);
    }
    for (size_t s = 0; s < p->segment_count; s++) {
        const TimeSpan *window = sonority_time_window(p->sonorities.items[s].sonority);
        int measure = form_measure_of(build->input.map, time_span_start(window));
        if (measure >= first_measure && measure <= last_measure) {
            assert(standard_form_input_add_segment(&build->input, p->segment_ids[s], window, 0, 0) ==
                   STANDARD_FORM_ANALYSIS_OK);
        }
    }
}

/* A cadence with its arrival span from `start` to `end` (copied). */
static inline void form_build_cadence(FormBuild *build, const char *id, const MusicalTime *start, const MusicalTime *end,
                                      CadenceType type, int strength_rank, bool post_cadential, double uncertainty) {
    const TimeSpan *span = form_build_span(build, start, end);
    assert(standard_form_input_add_cadence(&build->input, id, span, type, strength_rank, post_cadential, uncertainty) ==
           STANDARD_FORM_ANALYSIS_OK);
}

#endif
