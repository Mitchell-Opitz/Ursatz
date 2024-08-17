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
 * Streams. The separation is run per importer TRACK, the recommended setup (a track is usually a hand). The importer gives a multi-track
 * file's notes no Voice reference, so the track of each note is read back from the file: the note-ons of every MTrk chunk, matched to the
 * imported notes by tick and pitch (the way tests/analysis/test_voice_separation_corpus.c does). The note's track is that stream (the
 * chunk's index), its voice is the separation's voice index inside it (0 the top) and its confidence the separation's. A piano file's
 * two tracks are therefore two hands, each split into the voices it needs. Without a file path every note is in stream 0.
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

typedef struct {
    long long tick;
    int pitch;
    size_t track;
    bool used;
} TextureMidiOn;

static inline unsigned long texture_be(const unsigned char *bytes, size_t count) {
    unsigned long value = 0;
    for (size_t i = 0; i < count; i++) {
        value = (value << 8) | bytes[i];
    }
    return value;
}

static inline size_t texture_vlq(const unsigned char *data, size_t at, unsigned long *value) {
    *value = 0;
    size_t used = 0;
    unsigned char c;
    do {
        c = data[at + used];
        *value = (*value << 7) | (c & 0x7f);
        used++;
    } while ((c & 0x80) != 0 && used < 5);
    return used;
}

/* The note-ons of a standard MIDI file (format 1, running status) with the track each is in; the file's ticks per quarter in *division.
   The caller frees the result. */
static inline TextureMidiOn *texture_read_note_ons(const unsigned char *data, size_t size, size_t *out_count, unsigned long *division) {
    assert(size >= 14 && memcmp(data, "MThd", 4) == 0);
    size_t tracks = texture_be(data + 10, 2);
    *division = texture_be(data + 12, 2);
    size_t capacity = 4096;
    size_t count = 0;
    TextureMidiOn *ons = (TextureMidiOn *)calloc(capacity, sizeof(TextureMidiOn));
    assert(ons != NULL);
    size_t at = 14;
    for (size_t t = 0; t < tracks && at + 8 <= size; t++) {
        assert(memcmp(data + at, "MTrk", 4) == 0);
        size_t length = texture_be(data + at + 4, 4);
        size_t p = at + 8;
        size_t end = p + length;
        long long tick = 0;
        unsigned char running = 0;
        while (p < end) {
            unsigned long delta = 0;
            p += texture_vlq(data, p, &delta);
            tick += (long long)delta;
            unsigned char status = data[p];
            if (status == 0xff) {
                unsigned long meta_length = 0;
                p += 2;
                p += texture_vlq(data, p, &meta_length);
                p += meta_length;
            } else if (status == 0xf0 || status == 0xf7) {
                unsigned long sysex_length = 0;
                p += 1;
                p += texture_vlq(data, p, &sysex_length);
                p += sysex_length;
            } else {
                if ((status & 0x80) != 0) {
                    running = status;
                    p++;
                } else {
                    status = running;
                }
                unsigned char kind = status & 0xf0;
                if (kind == 0xc0 || kind == 0xd0) {
                    p++;
                } else {
                    unsigned char key = data[p];
                    unsigned char velocity = data[p + 1];
                    p += 2;
                    if (kind == 0x90 && velocity > 0) {
                        if (count == capacity) {
                            capacity *= 2;
                            ons = (TextureMidiOn *)realloc(ons, capacity * sizeof(TextureMidiOn));
                            assert(ons != NULL);
                        }
                        ons[count].tick = tick;
                        ons[count].pitch = key;
                        ons[count].track = t;
                        ons[count].used = false;
                        count++;
                    }
                }
            }
        }
        at = end;
    }
    *out_count = count;
    return ons;
}

/* Builds the input from the notes of the pipeline (every note), separated per track of the MIDI file at `midi_path` (NULL: one stream),
   over the MeterMap `map`. */
static inline void texture_build_from_import(TextureBuild *build, const WeightedPipeline *p, const MeterMap *map,
                                             const char *midi_path) {
    memset(build, 0, sizeof(*build));
    assert(standard_texture_input_init(&build->input, map) == STANDARD_TEXTURE_ANALYSIS_OK);

    size_t *stream = (size_t *)calloc(p->note_count, sizeof(size_t));
    assert(stream != NULL);
    size_t highest_track = 0;
    if (midi_path != NULL) {
        size_t size = 0;
        unsigned char *data = pipeline_read_file(midi_path, &size);
        size_t on_count = 0;
        unsigned long division = 0;
        TextureMidiOn *ons = texture_read_note_ons(data, size, &on_count, &division);
        free(data);
        for (size_t i = 0; i < p->note_count; i++) {
            const MusicalTime *onset = temporal_anchor_position(note_event_temporal_anchor(p->events[i]));
            long long numerator = (long long)musical_time_numerator(onset) * 4 * (long long)division;
            long long denominator = (long long)musical_time_denominator(onset);
            assert(numerator % denominator == 0);
            long long tick = numerator / denominator;
            int pitch = (int)pitch_numeric_value_position_numerator(p->pitch_values[i]);
            bool matched = false;
            for (size_t k = 0; k < on_count && !matched; k++) {
                if (!ons[k].used && ons[k].tick == tick && ons[k].pitch == pitch) {
                    ons[k].used = true;
                    stream[i] = ons[k].track;
                    highest_track = ons[k].track > highest_track ? ons[k].track : highest_track;
                    matched = true;
                }
            }
            assert(matched);
        }
        free(ons);
    }
    size_t seen_count = highest_track + 1;
    build->stream_count = seen_count;
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
}

#endif
