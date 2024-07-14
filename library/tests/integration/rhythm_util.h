#ifndef URSATZ_TESTS_INTEGRATION_RHYTHM_UTIL_H
#define URSATZ_TESTS_INTEGRATION_RHYTHM_UTIL_H

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis/outer_voice.h"
#include "analysis_expansion.h"
#include "analysis_phrase.h"
#include "meter_analysis_input.h"
#include "weighted_pipeline.h"

/*
 * Glue for the rhythm and meter analysis: builds the neutral input of standard-meter's adapter (meter_analysis_input.h) from the
 * imported notes, the L2 outer-voice lines, the harmonic segments and the standard-form phrases. standard-meter includes none of
 * these; this glue, which includes them all, lives in the tests.
 *
 *   notes         every imported note whose onset is in the measures [first, last]: its span is the onset to the release, its pitch the
 *                 semitone number, and its line 0 when it is a note of the outer-voice soprano, 1 for the bass and 2 for any other
 *   chord changes the start of every segment whose chord differs from the one before (both identified), the chords as the caller's
 *                 labels (compared as text, NULL for none)
 *   phrases       by measures (the annotation's) or from the form adapter's phrases (a span with its extension, the arrival chord of
 *                 its cadence, its length change). The form adapter does not report elided cadences, so a detected phrase is never
 *                 elided
 */

typedef struct {
    StandardMeterInput input;
    TimeSpan **spans; /* owned: every span copied into the input */
    size_t span_count;
    size_t span_capacity;
    MusicalTime **times; /* owned: every time copied into the input */
    size_t time_count;
    size_t time_capacity;
} RhythmBuild;

static inline void rhythm_build_init(RhythmBuild *build, const MeterMap *map) {
    memset(build, 0, sizeof(*build));
    assert(standard_meter_input_init(&build->input, map) == STANDARD_METER_ANALYSIS_OK);
}

static inline void rhythm_build_destroy(RhythmBuild *build) {
    standard_meter_input_destroy(&build->input);
    for (size_t i = 0; i < build->span_count; i++) {
        time_span_destroy(build->spans[i]);
    }
    for (size_t i = 0; i < build->time_count; i++) {
        musical_time_destroy(build->times[i]);
    }
    free(build->spans);
    free(build->times);
    memset(build, 0, sizeof(*build));
}

/* A span [start, end) kept by the build. */
static inline const TimeSpan *rhythm_build_span(RhythmBuild *build, const MusicalTime *start, const MusicalTime *end) {
    if (build->span_count == build->span_capacity) {
        build->span_capacity = build->span_capacity == 0 ? 512 : build->span_capacity * 2;
        build->spans = (TimeSpan **)realloc(build->spans, build->span_capacity * sizeof(TimeSpan *));
        assert(build->spans != NULL);
    }
    TimeSpan *span = NULL;
    assert(time_span_create(start, end, TIME_SPAN_BOUNDARY_LEFT_CLOSED, &span) == TIME_SPAN_OK);
    build->spans[build->span_count++] = span;
    return span;
}

/* A copy of `time` kept by the build. */
static inline const MusicalTime *rhythm_build_time(RhythmBuild *build, const MusicalTime *time) {
    if (build->time_count == build->time_capacity) {
        build->time_capacity = build->time_capacity == 0 ? 128 : build->time_capacity * 2;
        build->times = (MusicalTime **)realloc(build->times, build->time_capacity * sizeof(MusicalTime *));
        assert(build->times != NULL);
    }
    MusicalTime *copy = NULL;
    assert(musical_time_create(musical_time_numerator(time), musical_time_denominator(time), &copy) == MUSICAL_TIME_OK);
    build->times[build->time_count++] = copy;
    return copy;
}

static inline int rhythm_measure_of(const MeterMap *map, const MusicalTime *time) {
    int measure = 0;
    assert(meter_map_measure_index_at(map, time, &measure) == METER_MAP_OK);
    return measure;
}

static inline int rhythm_compare_ids(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* The ids of the notes the line takes, sorted. */
static inline const char **rhythm_line_ids(const OuterVoiceEntry *entries, size_t count, size_t *out_count) {
    const char **ids = (const char **)malloc((count == 0 ? 1 : count) * sizeof(char *));
    size_t n = 0;
    assert(ids != NULL);
    for (size_t s = 0; s < count; s++) {
        if (entries[s].present && entries[s].note_id != NULL) {
            ids[n++] = entity_id_value(entries[s].note_id);
        }
    }
    qsort(ids, n, sizeof(char *), rhythm_compare_ids);
    *out_count = n;
    return ids;
}

static inline bool rhythm_in_ids(const char **ids, size_t count, const char *id) {
    return bsearch(&id, ids, count, sizeof(char *), rhythm_compare_ids) != NULL;
}

/* The notes of the measures [first, last], as above. */
static inline void rhythm_build_notes(RhythmBuild *build, const WeightedPipeline *p, const OuterVoiceLines *lines, int first,
                                      int last) {
    size_t soprano_count = 0;
    size_t bass_count = 0;
    const char **soprano = rhythm_line_ids(lines->soprano, lines->count, &soprano_count);
    const char **bass = rhythm_line_ids(lines->bass, lines->count, &bass_count);
    for (size_t i = 0; i < p->note_count; i++) {
        const NoteEvent *event = p->events[i];
        const MusicalTime *onset = temporal_anchor_position(note_event_temporal_anchor(event));
        int measure = rhythm_measure_of(build->input.map, onset);
        if (measure < first || measure > last) {
            continue;
        }
        MusicalTime *release = NULL;
        assert(musical_time_add(onset, note_event_duration(event), &release) == MUSICAL_TIME_OK);
        const TimeSpan *span = rhythm_build_span(build, onset, release);
        musical_time_destroy(release);
        const char *id = entity_id_value(note_event_id(event));
        int line = rhythm_in_ids(soprano, soprano_count, id) ? 0 : (rhythm_in_ids(bass, bass_count, id) ? 1 : 2);
        assert(standard_meter_input_add_note(&build->input, id, span, pitch_numeric_value_position_numerator(p->pitch_values[i]),
                                             line) == STANDARD_METER_ANALYSIS_OK);
    }
    free(soprano);
    free(bass);
}

/* A chord change at the start of every segment (in the measures [first, last]) whose chord differs from the previous segment's. */
static inline void rhythm_build_chord_changes(RhythmBuild *build, const WeightedPipeline *p, const char *const *label_of_segment,
                                              int first, int last) {
    for (size_t s = 1; s < p->segment_count; s++) {
        if (label_of_segment[s] == NULL || label_of_segment[s - 1] == NULL ||
            strcmp(label_of_segment[s], label_of_segment[s - 1]) == 0) {
            continue;
        }
        const MusicalTime *start = time_span_start(sonority_time_window(p->sonorities.items[s].sonority));
        int measure = rhythm_measure_of(build->input.map, start);
        if (measure < first || measure > last) {
            continue;
        }
        char id[32];
        snprintf(id, sizeof(id), "chord-change-%zu", s);
        assert(standard_meter_input_add_chord_change(&build->input, id, rhythm_build_time(build, start)) ==
               STANDARD_METER_ANALYSIS_OK);
    }
}

/* A phrase over the measures [first, last] whose cadence arrives in `arrival` (0: none). */
static inline void rhythm_build_phrase(RhythmBuild *build, int index, int first, int last, int arrival,
                                       StandardMeterExpansionKind expansion, int expansion_measures, bool elided) {
    TimeSpan *from = NULL;
    TimeSpan *to = NULL;
    TimeSpan *cadence = NULL;
    assert(meter_map_measure_span(build->input.map, first, &from) == METER_MAP_OK);
    assert(meter_map_measure_span(build->input.map, last, &to) == METER_MAP_OK);
    const TimeSpan *span = rhythm_build_span(build, time_span_start(from), time_span_end(to));
    const TimeSpan *arrival_span = NULL;
    if (arrival > 0) {
        assert(meter_map_measure_span(build->input.map, arrival, &cadence) == METER_MAP_OK);
        arrival_span = rhythm_build_span(build, time_span_start(cadence), time_span_end(cadence));
    }
    char id[32];
    snprintf(id, sizeof(id), "phrase-%d", index);
    assert(standard_meter_input_add_phrase(&build->input, id, span, arrival_span, expansion, expansion_measures, elided) ==
           STANDARD_METER_ANALYSIS_OK);
    time_span_destroy(from);
    time_span_destroy(to);
    time_span_destroy(cadence);
}

/* The form adapter's phrases (its input `form`, for the arrival chords) and length changes as phrases. */
static inline void rhythm_build_detected_phrases(RhythmBuild *build, const StandardFormInput *form,
                                                 const StandardFormPhrases *phrases, const StandardFormExpansions *expansions) {
    for (size_t i = 0; i < phrases->count; i++) {
        const StandardFormPhrase *phrase = &phrases->items[i];
        const TimeSpan *span = rhythm_build_span(build, time_span_start(phrase->span), time_span_end(phrase->span));
        const TimeSpan *arrival = NULL;
        if (phrase->claim.cadence_index >= 0) {
            const TimeSpan *chord = form->cadences[phrase->claim.cadence_index].arrival;
            arrival = rhythm_build_span(build, time_span_start(chord), time_span_end(chord));
        }
        StandardMeterExpansionKind expansion = STANDARD_METER_EXPANSION_NONE;
        int measures = 0;
        for (size_t e = 0; expansions != NULL && e < expansions->count; e++) {
            if (expansions->items[e].claim.phrase != phrase->claim.index) {
                continue;
            }
            switch (expansions->items[e].claim.kind) {
            case STANDARD_FORM_EXPANSION_INTERNAL:
                expansion = STANDARD_METER_EXPANSION_INTERNAL;
                break;
            case STANDARD_FORM_EXPANSION_EXTERNAL:
                expansion = STANDARD_METER_EXPANSION_EXTERNAL;
                break;
            default:
                expansion = STANDARD_METER_EXPANSION_CONTRACTION;
                break;
            }
            measures = expansions->items[e].claim.measures_changed;
            break;
        }
        char id[32];
        snprintf(id, sizeof(id), "phrase-%zu", i);
        assert(standard_meter_input_add_phrase(&build->input, id, span, arrival, expansion, measures, false) ==
               STANDARD_METER_ANALYSIS_OK);
    }
}

#endif
