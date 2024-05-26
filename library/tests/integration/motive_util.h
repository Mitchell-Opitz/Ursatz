#ifndef URSATZ_TESTS_INTEGRATION_MOTIVE_UTIL_H
#define URSATZ_TESTS_INTEGRATION_MOTIVE_UTIL_H

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "form_util.h"
#include "outer_scale_degrees.h"
#include "time/duration.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"

/*
 * Glue for the motive analysis: builds standard-form's input (analysis_input.h) with a NOTE-LEVEL soprano, which the motive analysis needs
 * and the L2 outer-voice line cannot give (it keeps one note per segment, so a run of sixteenths is thinned to the notes that fall
 * on the segmentation; on Bach 114 36 of 68 notes survive). The soprano here is the highest sounding note at each attack: at every onset,
 * among the notes sounding then, the highest (the later attack among equals); it is a soprano note when it is attacked then. So a
 * left-hand note is the line where the right hand rests, as in L2's. Each note carries its scale degree in the home key (L2.6's label of
 * its pitch class), a chromatic note its altered degree.
 *
 * Only notes starting in the measures [first, last] are used.
 */

typedef struct {
    size_t index; /* into the pipeline's events */
    StandardFormQ start;
    StandardFormQ end;
    long semitone;
} MotiveNote;

static inline StandardFormQ motive_q_of_time(const MusicalTime *time) {
    return standard_form_q(musical_time_numerator(time), (long long)musical_time_denominator(time));
}

static inline void motive_parse_degree(const char *label, int *degree, int *alteration) {
    assert(label != NULL);
    *alteration = 0;
    if (*label == 'b') {
        *alteration = -1;
        label++;
    } else if (*label == '#') {
        *alteration = 1;
        label++;
    }
    assert(*label >= '1' && *label <= '7');
    *degree = *label - '0';
}

/* The soprano notes of the measures [first, last], in time order. *out is allocated; its count is returned. */
static inline size_t motive_soprano(const WeightedPipeline *p, const MeterMap *map, int first_measure, int last_measure,
                                    MotiveNote **out) {
    MotiveNote *all = (MotiveNote *)calloc(p->note_count, sizeof(MotiveNote));
    assert(all != NULL);
    size_t count = 0;
    for (size_t i = 0; i < p->note_count; i++) {
        const MusicalTime *time = temporal_anchor_position(note_event_temporal_anchor(p->events[i]));
        int measure = 0;
        assert(meter_map_measure_index_at(map, time, &measure) == METER_MAP_OK);
        if (measure < first_measure || measure > last_measure) {
            continue;
        }
        all[count].index = i;
        all[count].start = motive_q_of_time(time);
        const Duration *duration = note_event_duration(p->events[i]);
        all[count].end = standard_form_q_add(all[count].start,
                                             standard_form_q(duration_numerator(duration), (long long)duration_denominator(duration)));
        all[count].semitone = pitch_numeric_value_position_numerator(p->pitch_values[i]);
        count++;
    }
    MotiveNote *line = (MotiveNote *)calloc(count > 0 ? count : 1, sizeof(MotiveNote));
    assert(line != NULL);
    size_t kept = 0;
    for (size_t i = 0; i < count; i++) {
        /* the first note of each onset group decides: skip an onset already handled */
        if (i > 0 && standard_form_q_compare(all[i - 1].start, all[i].start) == 0) {
            continue;
        }
        StandardFormQ onset = all[i].start;
        const MotiveNote *top = NULL;
        for (size_t j = 0; j < count; j++) {
            if (standard_form_q_compare(all[j].start, onset) > 0) {
                break;
            }
            if (standard_form_q_compare(all[j].end, onset) <= 0) {
                continue;
            }
            if (top == NULL || all[j].semitone > top->semitone ||
                (all[j].semitone == top->semitone && standard_form_q_compare(all[j].start, top->start) > 0)) {
                top = &all[j];
            }
        }
        if (top != NULL && standard_form_q_compare(top->start, onset) == 0) {
            line[kept++] = *top;
        }
    }
    free(all);
    *out = line;
    return kept;
}

/* Builds the soprano into the build's input, with the scale degrees in the key on `tonic_pitch_class`. */
static inline void motive_build_soprano(FormBuild *build, const WeightedPipeline *p, int first_measure, int last_measure,
                                        int tonic_pitch_class, CommonPracticeKeyMode mode) {
    MotiveNote *line = NULL;
    size_t count = motive_soprano(p, build->input.map, first_measure, last_measure, &line);
    for (size_t k = 0; k < count; k++) {
        const NoteEvent *event = p->events[line[k].index];
        MusicalTime *start = NULL;
        MusicalTime *end = NULL;
        assert(musical_time_create((long)line[k].start.n, (unsigned long)line[k].start.d, &start) == MUSICAL_TIME_OK);
        assert(musical_time_create((long)line[k].end.n, (unsigned long)line[k].end.d, &end) == MUSICAL_TIME_OK);
        const TimeSpan *span = form_build_span(build, start, end);
        musical_time_destroy(start);
        musical_time_destroy(end);
        assert(standard_form_input_add_note(&build->input, entity_id_value(note_event_id(event)), event, span, NULL,
                                            line[k].semitone) == STANDARD_FORM_ANALYSIS_OK);
        int pitch_class = (int)(((line[k].semitone % 12) + 12) % 12);
        int degree = 0;
        int alteration = 0;
        motive_parse_degree(common_practice_scale_degree_label(pitch_class, tonic_pitch_class, mode), &degree, &alteration);
        assert(standard_form_input_set_degree(&build->input, false, k, degree, alteration) == STANDARD_FORM_ANALYSIS_OK);
    }
    free(line);
}

#endif
