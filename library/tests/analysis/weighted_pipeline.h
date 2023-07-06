#ifndef URSATZ_TESTS_ANALYSIS_WEIGHTED_PIPELINE_H
#define URSATZ_TESTS_ANALYSIS_WEIGHTED_PIPELINE_H

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "events/note_event.h"
#include "io/midi_importer.h"
#include "metric_position_rules.h"
#include "pitch/pitch_spelling.h"
#include "segmentation_beat_grid.h"
#include "semantics/sonority_segmentation.h"
#include "time/exact_rational.h"
#include "time/metrical_position.h"
#include "time/temporal_anchor.h"
#include "time/time_span.h"
#include "weighted_chord_identification.h"

/*
 * Test glue for weighted chord identification: imports a corpus MIDI file, cuts its notes into adaptive
 * sonority segments on the beat grid of a MeterMap (standard-meter's provider), and turns every segment into
 * CommonPracticeMembers: the pitch from the PitchSpelling, the duration sounding inside the segment, the
 * held flag, the BeatStrength of the attack (standard-meter, from the metrical position of the onset), the
 * Voice id and the onset. common-practice-minimal reads only these plain members: it includes no other
 * framework, and this glue (which does) lives in the tests.
 */

typedef struct {
    NoteEvent **events;
    PitchNumericValue **pitch_values;
    size_t note_count;
    MidiMeterImport meter;
    AdaptiveSonorities sonorities;
    CommonPracticeMember *members; /* every segment's members, one after another */
    CommonPracticeSegment *segments;
    size_t segment_count;
    char (*segment_ids)[32];
    double *starts; /* each segment's window, in whole notes */
    double *ends;
} WeightedPipeline;

static inline double pipeline_as_double(ExactRational r) {
    return (double)r.n / (double)r.d;
}

static inline unsigned char *pipeline_read_file(const char *path, size_t *out_size) {
    FILE *file = fopen(path, "rb");
    assert(file != NULL);
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    assert(length > 0);
    unsigned char *data = (unsigned char *)malloc((size_t)length + 1);
    assert(data != NULL);
    size_t read_count = fread(data, 1, (size_t)length, file);
    fclose(file);
    assert(read_count == (size_t)length);
    data[length] = '\0';
    *out_size = (size_t)length;
    return data;
}

typedef struct {
    const char *id;
    size_t index;
} PipelineNoteKey;

static inline int pipeline_compare_keys(const void *a, const void *b) {
    return strcmp(((const PipelineNoteKey *)a)->id, ((const PipelineNoteKey *)b)->id);
}

static inline int pipeline_semitone(const PitchSpelling *spelling) {
    static const int LETTER_SEMITONE[7] = {9, 11, 0, 2, 4, 5, 7};
    return 12 * (pitch_spelling_octave(spelling) + 1) + LETTER_SEMITONE[pitch_spelling_letter(spelling)] +
           pitch_spelling_accidental(spelling);
}

/* The strength of an attack at `onset`, read in the signature of its measure; weak if it cannot be read. */
static inline BeatStrength pipeline_attack_strength(const MeterMap *map, const MusicalTime *onset) {
    MetricalPosition *position = NULL;
    BeatStrength strength = BEAT_STRENGTH_WEAK;
    if (metrical_position_from_time(map, onset, &position) != METRICAL_POSITION_OK) {
        return strength;
    }
    const TimeSignature *reading = NULL;
    if (metrical_position_reading_signature(map, metrical_position_measure(position), &reading) ==
        METRICAL_POSITION_OK) {
        BeatStrength found;
        if (standard_meter_beat_strength(reading, position, &found)) {
            strength = found;
        }
    }
    metrical_position_destroy(position);
    return strength;
}

/* Runs the pipeline over a corpus file. `map` is the MeterMap to segment and read strengths with, or NULL for
   the file's own. */
static inline WeightedPipeline pipeline_run(const char *corpus_dir, const char *file_name, const MeterMap *map) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", corpus_dir, file_name);
    size_t size = 0;
    unsigned char *data = pipeline_read_file(path, &size);

    WeightedPipeline p;
    memset(&p, 0, sizeof(p));
    assert(midi_importer_import_with_meter(data, size, &p.events, &p.pitch_values, &p.note_count, &p.meter) ==
           MIDI_IMPORT_OK);
    free(data);
    if (map == NULL) {
        map = p.meter.meter_map;
    }

    assert(sonority_builder_build_adaptive(p.events, p.note_count, map, standard_meter_segmentation_beat_grid, NULL,
                                           NULL, &p.sonorities) == AD_SEGMENTATION_OK);

    PipelineNoteKey *keys = (PipelineNoteKey *)malloc(p.note_count * sizeof(PipelineNoteKey));
    for (size_t i = 0; i < p.note_count; i++) {
        keys[i].id = entity_id_value(note_event_id(p.events[i]));
        keys[i].index = i;
    }
    qsort(keys, p.note_count, sizeof(PipelineNoteKey), pipeline_compare_keys);

    size_t total_members = 0;
    for (size_t i = 0; i < p.sonorities.count; i++) {
        total_members += sonority_event_id_count(p.sonorities.items[i].sonority);
    }
    p.segment_count = p.sonorities.count;
    p.members = (CommonPracticeMember *)calloc(total_members > 0 ? total_members : 1, sizeof(CommonPracticeMember));
    p.segments = (CommonPracticeSegment *)calloc(p.segment_count, sizeof(CommonPracticeSegment));
    p.segment_ids = calloc(p.segment_count, sizeof(char[32]));
    p.starts = (double *)calloc(p.segment_count, sizeof(double));
    p.ends = (double *)calloc(p.segment_count, sizeof(double));

    size_t next = 0;
    for (size_t s = 0; s < p.segment_count; s++) {
        const AdaptiveSonority *segment = &p.sonorities.items[s];
        const TimeSpan *window = sonority_time_window(segment->sonority);
        ExactRational start = er_of_time(time_span_start(window));
        ExactRational end = er_of_time(time_span_end(window));
        p.starts[s] = pipeline_as_double(start);
        p.ends[s] = pipeline_as_double(end);
        snprintf(p.segment_ids[s], sizeof(p.segment_ids[s]), "seg-%zu", s);

        size_t count = sonority_event_id_count(segment->sonority);
        p.segments[s].members = &p.members[next];
        p.segments[s].member_count = count;
        p.segments[s].id = p.segment_ids[s];
        for (size_t j = 0; j < count; j++) {
            PipelineNoteKey probe = {entity_id_value(sonority_event_id_at(segment->sonority, j)), 0};
            const PipelineNoteKey *found = (const PipelineNoteKey *)bsearch(&probe, keys, p.note_count,
                                                                            sizeof(PipelineNoteKey),
                                                                            pipeline_compare_keys);
            assert(found != NULL);
            const NoteEvent *note = p.events[found->index];
            const MusicalTime *onset_time = temporal_anchor_position(note_event_temporal_anchor(note));
            ExactRational onset = er_of_time(onset_time);
            ExactRational release = {0, 1};
            assert(er_add(onset, er_of_duration(note_event_duration(note)), &release) == 0);
            ExactRational from = er_cmp(onset, start) > 0 ? onset : start;
            ExactRational to = er_cmp(release, end) < 0 ? release : end;
            ExactRational overlap = {0, 1};
            assert(er_cmp(to, from) > 0 && er_sub(to, from, &overlap) == 0);

            CommonPracticeMember *member = &p.members[next++];
            member->note_id = entity_id_value(note_event_id(note));
            member->pitch = pipeline_semitone(note_event_optional_pitch_spelling(note));
            member->held = segment->held[j];
            member->duration = pipeline_as_double(overlap);
            member->strength = member->held ? BEAT_STRENGTH_STRONG : pipeline_attack_strength(map, onset_time);
            const VoiceReference *voice = note_event_optional_voice(note);
            member->voice_id = voice != NULL ? entity_id_value(voice_reference_target_id(voice)) : NULL;
            member->onset = pipeline_as_double(onset);
        }
    }
    free(keys);
    return p;
}

static inline void pipeline_destroy(WeightedPipeline *p) {
    free(p->members);
    free(p->segments);
    free(p->segment_ids);
    free(p->starts);
    free(p->ends);
    adaptive_sonorities_destroy(&p->sonorities);
    for (size_t i = 0; i < p->note_count; i++) {
        note_event_destroy(p->events[i]);
        pitch_numeric_value_destroy(p->pitch_values[i]);
    }
    free(p->events);
    free(p->pitch_values);
    midi_meter_import_destroy(&p->meter);
}

#endif
