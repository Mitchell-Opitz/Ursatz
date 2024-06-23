#include "pipeline/segmentation.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "metric_position_rules.h"
#include "pipeline/pitch_class_util.h"
#include "pitch/pitch_spelling.h"
#include "segmentation_beat_grid.h"
#include "semantics/sonority_segmentation.h"
#include "time/exact_rational.h"
#include "time/meter_map.h"
#include "time/metrical_position.h"
#include "time/temporal_anchor.h"
#include "time/time_span.h"

typedef struct {
    const char *id;
    size_t index;
} NoteKey;

static int compare_note_keys(const void *a, const void *b) {
    return strcmp(((const NoteKey *)a)->id, ((const NoteKey *)b)->id);
}

static double as_double(ExactRational r) {
    return (double)r.n / (double)r.d;
}

/* Semitone number of a spelled pitch, C4 = 60, as Ursatz's segmentation reads it. */
static int semitone_of(const PitchSpelling *spelling) {
    static const int LETTER_SEMITONE[7] = {9, 11, 0, 2, 4, 5, 7};
    return 12 * (pitch_spelling_octave(spelling) + 1) + LETTER_SEMITONE[pitch_spelling_letter(spelling)] +
           pitch_spelling_accidental(spelling);
}

/* The strength of an attack at onset, read in the signature of its measure; weak if it cannot be read. */
static BeatStrength attack_strength(const MeterMap *map, const MusicalTime *onset) {
    BeatStrength strength = BEAT_STRENGTH_WEAK;
    MetricalPosition *position = NULL;
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

/* Fills one member from its note, for the segment window [start, end). False when the note does not overlap it. */
static bool build_member(const NoteEvent *note, const MeterMap *map, ExactRational start, ExactRational end,
                         bool held, CommonPracticeMember *member) {
    const MusicalTime *onset_time = temporal_anchor_position(note_event_temporal_anchor(note));
    ExactRational onset = er_of_time(onset_time);
    ExactRational release = {0, 1};
    if (er_add(onset, er_of_duration(note_event_duration(note)), &release) != 0) {
        return false;
    }
    ExactRational from = er_cmp(onset, start) > 0 ? onset : start;
    ExactRational to = er_cmp(release, end) < 0 ? release : end;
    ExactRational overlap = {0, 1};
    if (er_cmp(to, from) <= 0 || er_sub(to, from, &overlap) != 0) {
        return false;
    }

    member->note_id = entity_id_value(note_event_id(note));
    member->pitch = semitone_of(note_event_optional_pitch_spelling(note));
    member->held = held;
    member->duration = as_double(overlap);
    member->strength = held ? BEAT_STRENGTH_STRONG : attack_strength(map, onset_time);
    const VoiceReference *voice = note_event_optional_voice(note);
    member->voice_id = voice != NULL ? entity_id_value(voice_reference_target_id(voice)) : NULL;
    member->onset = as_double(onset);
    return true;
}

/* The beat a segment [start, end) lies inside, as a non-zero index (measure and beat), or 0 when it spans beats or
   the grid cannot be read: segments inside one beat share one value (CommonPracticeSegment.beat). False only on
   allocation failure. */
static bool beat_index(const MeterMap *map, ExactRational start, ExactRational end, long *out) {
    *out = 0;
    MusicalTime *at = er_to_time(start);
    if (!at) {
        return false;
    }
    int measure = 0;
    bool found = meter_map_measure_index_at(map, at, &measure) == METER_MAP_OK;
    musical_time_destroy(at);
    MusicalTime **starts = NULL;
    size_t count = 0;
    if (!found || !standard_meter_segmentation_beat_grid(map, measure, NULL, &starts, &count)) {
        return true;
    }
    TimeSpan *span = NULL;
    if (meter_map_measure_span(map, measure, &span) == METER_MAP_OK) {
        ExactRational measure_end = er_of_time(time_span_end(span));
        for (size_t b = 0; b < count; b++) {
            ExactRational beat_start = er_of_time(starts[b]);
            ExactRational beat_end = b + 1 < count ? er_of_time(starts[b + 1]) : measure_end;
            if (er_cmp(beat_start, start) <= 0 && er_cmp(start, beat_end) < 0) {
                if (er_cmp(end, beat_end) <= 0) {
                    *out = ((long)measure + 1) * 1000 + (long)b + 1;
                }
                break;
            }
        }
        time_span_destroy(span);
    }
    for (size_t b = 0; b < count; b++) {
        musical_time_destroy(starts[b]);
    }
    free(starts);
    return true;
}

static bool fill_segments(const ImportedNotes *notes, const AdaptiveSonorities *sonorities, Segmentation *out) {
    const MeterMap *map = notes->meter.meter_map;

    NoteKey *keys = malloc(sizeof(NoteKey) * notes->count);
    if (!keys) {
        return false;
    }
    for (size_t i = 0; i < notes->count; i++) {
        keys[i].id = entity_id_value(note_event_id(notes->events[i]));
        keys[i].index = i;
    }
    qsort(keys, notes->count, sizeof(NoteKey), compare_note_keys);

    out->segments = calloc(sonorities->count, sizeof(Segment));
    out->library_segments = calloc(sonorities->count, sizeof(CommonPracticeSegment));
    out->segment_ids = calloc(sonorities->count, sizeof(char[32]));
    if (!out->segments || !out->library_segments || !out->segment_ids) {
        free(keys);
        return false;
    }
    out->segment_count = sonorities->count;

    for (size_t s = 0; s < sonorities->count; s++) {
        const AdaptiveSonority *item = &sonorities->items[s];
        const TimeSpan *window = sonority_time_window(item->sonority);
        ExactRational start = er_of_time(time_span_start(window));
        ExactRational end = er_of_time(time_span_end(window));
        size_t count = sonority_event_id_count(item->sonority);

        Segment *segment = &out->segments[s];
        segment->start_numerator = (long)start.n;
        segment->start_denominator = (unsigned long)start.d;
        segment->end_numerator = (long)end.n;
        segment->end_denominator = (unsigned long)end.d;
        segment->note_indices = malloc(sizeof(size_t) * count);
        segment->members = malloc(sizeof(CommonPracticeMember) * count);
        if (!segment->note_indices || !segment->members) {
            free(keys);
            return false;
        }
        segment->member_count = count;

        for (size_t j = 0; j < count; j++) {
            NoteKey probe = {entity_id_value(sonority_event_id_at(item->sonority, j)), 0};
            const NoteKey *found = bsearch(&probe, keys, notes->count, sizeof(NoteKey), compare_note_keys);
            if (!found || !build_member(notes->events[found->index], map, start, end, item->held[j],
                                        &segment->members[j])) {
                free(keys);
                return false;
            }
            segment->note_indices[j] = found->index;
        }

        snprintf(out->segment_ids[s], sizeof(out->segment_ids[s]), "seg-%zu", s);
        out->library_segments[s].members = segment->members;
        out->library_segments[s].member_count = count;
        out->library_segments[s].id = out->segment_ids[s];
        if (!beat_index(map, start, end, &out->library_segments[s].beat)) {
            free(keys);
            return false;
        }
    }
    free(keys);
    return true;
}

bool segment_notes(const ImportedNotes *notes, bool merge_unchanged_harmony, Segmentation *out) {
    *out = (Segmentation){NULL, 0, NULL, NULL, {NULL, 0}};
    if (notes->count == 0) {
        return true;
    }

    AdaptiveSegmentationConfig config = adaptive_segmentation_config_default();
    config.merge_unchanged_harmony = merge_unchanged_harmony;
    AdaptiveSonorities sonorities = {NULL, 0};
    if (sonority_builder_build_adaptive(notes->events, notes->count, notes->meter.meter_map,
                                        standard_meter_segmentation_beat_grid, NULL, &config,
                                        &sonorities) != AD_SEGMENTATION_OK) {
        return false;
    }
    if (!fill_segments(notes, &sonorities, out)) {
        adaptive_sonorities_destroy(&sonorities);
        segmentation_destroy(out);
        return false;
    }
    out->sonorities = sonorities;
    return true;
}

void segmentation_destroy(Segmentation *segmentation) {
    if (!segmentation) {
        return;
    }
    for (size_t i = 0; i < segmentation->segment_count; i++) {
        free(segmentation->segments[i].note_indices);
        free(segmentation->segments[i].members);
    }
    free(segmentation->segments);
    free(segmentation->library_segments);
    free(segmentation->segment_ids);
    adaptive_sonorities_destroy(&segmentation->sonorities);
    *segmentation = (Segmentation){NULL, 0, NULL, NULL, {NULL, 0}};
}

int segment_bass_pitch_class(const Segment *segment) {
    int lowest = segment->members[0].pitch;
    for (size_t i = 1; i < segment->member_count; i++) {
        if (segment->members[i].pitch < lowest) {
            lowest = segment->members[i].pitch;
        }
    }
    return pitch_class_of(lowest);
}
