#ifndef URSATZ_TESTS_INTEGRATION_TEXTURE_UTIL_H
#define URSATZ_TESTS_INTEGRATION_TEXTURE_UTIL_H

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis/voice_separation.h"
#include "texture_analysis_input.h"
#include "weighted_pipeline.h"

/*
 * Glue for the texture analysis: builds the neutral input of standard-texture's adapter (texture_analysis_input.h) from an imported
 * file and the kernel's rule-based voice separation (L2.7). standard-texture includes neither; this glue, which includes both, lives in
 * the tests.
 *
 * Streams. The separation is run per importer TRACK, the recommended setup (a track is usually a hand): each note's stream is the
 * index of its Voice reference's id, in order of first appearance (0 for a note with none). The note's track is that stream, its voice
 * is the separation's voice index inside it (0 the top) and its confidence the separation's. A piano file's two tracks are therefore
 * two hands, each split into the voices it needs.
 */

typedef struct {
    StandardTextureInput input;
    TimeSpan **spans; /* owned: every span copied into the input */
    size_t span_count;
    VoiceAssignment assignment;
    size_t stream_count;
} TextureBuild;

static inline void texture_build_destroy(TextureBuild *build) {
    standard_texture_input_destroy(&build->input);
    for (size_t i = 0; i < build->span_count; i++) {
        time_span_destroy(build->spans[i]);
    }
    free(build->spans);
    voice_assignment_destroy(&build->assignment);
    memset(build, 0, sizeof(*build));
}

/* Builds the input from the notes of the pipeline (every note), separated per track, over the MeterMap `map`. */
static inline void texture_build_from_import(TextureBuild *build, const WeightedPipeline *p, const MeterMap *map) {
    memset(build, 0, sizeof(*build));
    assert(standard_texture_input_init(&build->input, map) == STANDARD_TEXTURE_ANALYSIS_OK);

    size_t *stream = (size_t *)calloc(p->note_count, sizeof(size_t));
    const char **seen = (const char **)calloc(p->note_count + 1, sizeof(char *));
    assert(stream != NULL && seen != NULL);
    size_t seen_count = 0;
    for (size_t i = 0; i < p->note_count; i++) {
        const VoiceReference *voice = note_event_optional_voice(p->events[i]);
        if (voice == NULL) {
            stream[i] = 0;
            continue;
        }
        const char *id = entity_id_value(voice_reference_target_id(voice));
        size_t k = 0;
        while (k < seen_count && strcmp(seen[k], id) != 0) {
            k++;
        }
        if (k == seen_count) {
            seen[seen_count++] = id;
        }
        stream[i] = k;
    }
    build->stream_count = seen_count == 0 ? 1 : seen_count;
    assert(voice_separate(p->events, p->pitch_values, p->note_count, stream, map, standard_meter_segmentation_beat_grid, NULL, NULL,
                          &build->assignment) == VOICE_SEPARATION_OK);

    build->spans = (TimeSpan **)calloc(p->note_count, sizeof(TimeSpan *));
    assert(build->spans != NULL);
    for (size_t i = 0; i < p->note_count; i++) {
        const NoteEvent *event = p->events[i];
        const MusicalTime *onset = temporal_anchor_position(note_event_temporal_anchor(event));
        MusicalTime *release = NULL;
        assert(musical_time_add(onset, note_event_duration(event), &release) == MUSICAL_TIME_OK);
        TimeSpan *span = NULL;
        assert(time_span_create(onset, release, TIME_SPAN_BOUNDARY_LEFT_CLOSED, &span) == TIME_SPAN_OK);
        musical_time_destroy(release);
        build->spans[build->span_count++] = span;
        const VoiceAssignmentEntry *entry = &build->assignment.entries[i];
        assert(standard_texture_input_add_note(&build->input, entity_id_value(note_event_id(event)), span,
                                               pitch_numeric_value_position_numerator(p->pitch_values[i]), stream[i], entry->voice,
                                               entry->confidence) == STANDARD_TEXTURE_ANALYSIS_OK);
    }
    free(stream);
    free(seen);
}

#endif
