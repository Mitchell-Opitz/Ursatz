#include "semantics/sonority_segmentation.h"

#include <stdlib.h>
#include <string.h>

#include "pitch/pitch_spelling.h"
#include "semantics/sonority_segmentation_internal.h"
#include "time/duration.h"
#include "time/temporal_anchor.h"
#include "time/time_span.h"

AdaptiveSegmentationConfig adaptive_segmentation_config_default(void) {
    AdaptiveSegmentationConfig config;
    config.min_split_numerator = 1;
    config.min_split_denominator = 2;
    config.merge_across_barlines = false;
    return config;
}

void segment_list_destroy(SegmentList *list) {
    if (list == NULL) {
        return;
    }
    for (size_t i = 0; i < list->count; i++) {
        free(list->items[i].members);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
}

/* The semitone of each letter above C, indexed by PitchLetter (A B C D E F G). */
static const int LETTER_SEMITONE[7] = {9, 11, 0, 2, 4, 5, 7};

static int semitone_of(const PitchSpelling *spelling) {
    return 12 * (pitch_spelling_octave(spelling) + 1) + LETTER_SEMITONE[pitch_spelling_letter(spelling)] +
           pitch_spelling_accidental(spelling);
}

AdaptiveSegmentationResult segmentation_prepare_notes(NoteEvent *const *notes, size_t note_count, NoteSpan **out) {
    NoteSpan *spans = (NoteSpan *)calloc(note_count, sizeof(NoteSpan));
    if (spans == NULL) {
        return AD_SEGMENTATION_ERR_ALLOC;
    }
    for (size_t i = 0; i < note_count; i++) {
        const NoteEvent *note = notes[i];
        if (note == NULL) {
            free(spans);
            return AD_SEGMENTATION_ERR_NULL_NOTE;
        }
        const PitchSpelling *spelling = note_event_optional_pitch_spelling(note);
        if (spelling == NULL) {
            free(spans);
            return AD_SEGMENTATION_ERR_NO_PITCH;
        }
        const MusicalTime *position = temporal_anchor_position(note_event_temporal_anchor(note));
        if (musical_time_numerator(position) < 0) {
            free(spans);
            return AD_SEGMENTATION_ERR_METER_MAP;
        }
        spans[i].onset = er_of_time(position);
        if (er_add(spans[i].onset, er_of_duration(note_event_duration(note)), &spans[i].release) != 0) {
            free(spans);
            return AD_SEGMENTATION_ERR_OVERFLOW;
        }
        spans[i].pitch = semitone_of(spelling);
        spans[i].pitch_class = ((spans[i].pitch % 12) + 12) % 12;
    }
    *out = spans;
    return AD_SEGMENTATION_OK;
}

AdaptiveSegmentationResult segmentation_members_of(const NoteSpan *spans, size_t span_count, ExactRational start,
                                                    ExactRational end, SegmentMember **out_members,
                                                    size_t *out_count) {
    SegmentMember *members = NULL;
    size_t count = 0;
    if (span_count > 0) {
        members = (SegmentMember *)malloc(span_count * sizeof(SegmentMember));
        if (members == NULL) {
            return AD_SEGMENTATION_ERR_ALLOC;
        }
    }
    for (size_t i = 0; i < span_count; i++) {
        if (er_cmp(spans[i].onset, end) < 0 && er_cmp(spans[i].release, start) > 0) {
            members[count].note = i;
            members[count].attacked = er_cmp(spans[i].onset, start) >= 0;
            count++;
        }
    }
    if (count == 0) {
        free(members);
        members = NULL;
    }
    *out_members = members;
    *out_count = count;
    return AD_SEGMENTATION_OK;
}

int segment_bass_pitch(const NoteSpan *spans, const Segment *segment) {
    int bass = -1;
    for (size_t i = 0; i < segment->member_count; i++) {
        int pitch = spans[segment->members[i].note].pitch;
        if (bass < 0 || pitch < bass) {
            bass = pitch;
        }
    }
    return bass;
}

unsigned segment_pitch_class_set(const NoteSpan *spans, const Segment *segment) {
    unsigned set = 0;
    for (size_t i = 0; i < segment->member_count; i++) {
        set |= 1u << spans[segment->members[i].note].pitch_class;
    }
    return set;
}

static void free_beat_starts(MusicalTime **starts, size_t count) {
    if (starts == NULL) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        musical_time_destroy(starts[i]);
    }
    free(starts);
}

static AdaptiveSegmentationResult append_segment(SegmentList *list, size_t *capacity, Segment segment) {
    if (list->count == *capacity) {
        size_t grown_capacity = *capacity == 0 ? 64 : *capacity * 2;
        Segment *grown = (Segment *)realloc(list->items, grown_capacity * sizeof(Segment));
        if (grown == NULL) {
            return AD_SEGMENTATION_ERR_ALLOC;
        }
        list->items = grown;
        *capacity = grown_capacity;
    }
    list->items[list->count++] = segment;
    return AD_SEGMENTATION_OK;
}

AdaptiveSegmentationResult segmentation_cut(const NoteSpan *spans, size_t span_count, const MeterMap *map,
                                             BeatGridProvider provider, void *provider_context, SegmentList *out) {
    if (spans == NULL || span_count == 0 || map == NULL || provider == NULL || out == NULL) {
        return AD_SEGMENTATION_ERR_NULL_ARGUMENT;
    }

    ExactRational sounding_end = spans[0].release;
    for (size_t i = 1; i < span_count; i++) {
        if (er_cmp(spans[i].release, sounding_end) > 0) {
            sounding_end = spans[i].release;
        }
    }

    SegmentList list = {NULL, 0};
    size_t capacity = 0;
    AdaptiveSegmentationResult failure = AD_SEGMENTATION_OK;

    for (int measure = meter_map_first_measure_number(map); failure == AD_SEGMENTATION_OK; measure++) {
        TimeSpan *measure_span = NULL;
        if (meter_map_measure_span(map, measure, &measure_span) != METER_MAP_OK) {
            failure = AD_SEGMENTATION_ERR_METER_MAP;
            break;
        }
        ExactRational measure_start = er_of_time(time_span_start(measure_span));
        ExactRational measure_end = er_of_time(time_span_end(measure_span));
        time_span_destroy(measure_span);
        if (er_cmp(measure_start, sounding_end) >= 0) {
            break;
        }

        MusicalTime **starts = NULL;
        size_t start_count = 0;
        if (!provider(map, measure, provider_context, &starts, &start_count) || start_count == 0) {
            free_beat_starts(starts, start_count);
            failure = AD_SEGMENTATION_ERR_BEAT_GRID;
            break;
        }

        for (size_t beat = 0; beat < start_count && failure == AD_SEGMENTATION_OK; beat++) {
            ExactRational beat_start = er_of_time(starts[beat]);
            ExactRational beat_end = beat + 1 < start_count ? er_of_time(starts[beat + 1]) : measure_end;
            bool starts_ok = beat > 0 || er_cmp(beat_start, measure_start) == 0;
            if (!starts_ok || er_cmp(beat_start, beat_end) >= 0 || er_cmp(beat_end, measure_end) > 0) {
                failure = AD_SEGMENTATION_ERR_BEAT_GRID;
                break;
            }

            Segment segment;
            segment.start = beat_start;
            segment.end = beat_end;
            segment.starts_measure = beat == 0;
            failure = segmentation_members_of(spans, span_count, beat_start, beat_end, &segment.members,
                                              &segment.member_count);
            if (failure != AD_SEGMENTATION_OK) {
                break;
            }
            failure = append_segment(&list, &capacity, segment);
            if (failure != AD_SEGMENTATION_OK) {
                free(segment.members);
            }
        }
        free_beat_starts(starts, start_count);
    }

    if (failure != AD_SEGMENTATION_OK) {
        segment_list_destroy(&list);
        return failure;
    }
    *out = list;
    return AD_SEGMENTATION_OK;
}

/* The pitch class of the lowest sounding pitch of a non-empty segment. */
static int bass_pitch_class(const NoteSpan *spans, const Segment *segment) {
    return ((segment_bass_pitch(spans, segment) % 12) + 12) % 12;
}

/* Appends the members of `next` that `into` does not already hold, and marks a shared note attacked if
   either has it attacked. */
static AdaptiveSegmentationResult union_members(Segment *into, const Segment *next) {
    size_t added = 0;
    for (size_t i = 0; i < next->member_count; i++) {
        bool found = false;
        for (size_t j = 0; j < into->member_count; j++) {
            if (into->members[j].note == next->members[i].note) {
                into->members[j].attacked = into->members[j].attacked || next->members[i].attacked;
                found = true;
                break;
            }
        }
        if (!found) {
            added++;
        }
    }
    if (added == 0) {
        return AD_SEGMENTATION_OK;
    }
    SegmentMember *grown = (SegmentMember *)realloc(into->members, (into->member_count + added) * sizeof(SegmentMember));
    if (grown == NULL) {
        return AD_SEGMENTATION_ERR_ALLOC;
    }
    into->members = grown;
    size_t base = into->member_count;
    for (size_t i = 0; i < next->member_count; i++) {
        bool found = false;
        for (size_t j = 0; j < base; j++) {
            if (into->members[j].note == next->members[i].note) {
                found = true;
                break;
            }
        }
        if (!found) {
            into->members[into->member_count++] = next->members[i];
        }
    }
    return AD_SEGMENTATION_OK;
}

AdaptiveSegmentationResult segmentation_merge(const NoteSpan *spans, const AdaptiveSegmentationConfig *config,
                                               SegmentList *segments) {
    if (spans == NULL || config == NULL || segments == NULL) {
        return AD_SEGMENTATION_ERR_NULL_ARGUMENT;
    }

    size_t kept = 0; /* segments[0..kept) is the merged result so far */
    for (size_t i = 0; i < segments->count; i++) {
        Segment *segment = &segments->items[i];
        if (kept > 0) {
            Segment *last = &segments->items[kept - 1];
            bool crosses_barline = segment->starts_measure && !config->merge_across_barlines;
            bool same_content = last->member_count > 0 && segment->member_count > 0 &&
                                segment_pitch_class_set(spans, last) == segment_pitch_class_set(spans, segment) &&
                                bass_pitch_class(spans, last) == bass_pitch_class(spans, segment);
            if (same_content && !crosses_barline && er_cmp(last->end, segment->start) == 0) {
                AdaptiveSegmentationResult merged = union_members(last, segment);
                if (merged != AD_SEGMENTATION_OK) {
                    return merged;
                }
                last->end = segment->end;
                free(segment->members);
                segment->members = NULL;
                segment->member_count = 0;
                continue;
            }
        }
        if (kept != i) {
            segments->items[kept] = *segment;
        }
        kept++;
    }
    segments->count = kept;
    return AD_SEGMENTATION_OK;
}

void adaptive_sonorities_destroy(AdaptiveSonorities *sonorities) {
    if (sonorities == NULL) {
        return;
    }
    for (size_t i = 0; i < sonorities->count; i++) {
        sonority_destroy(sonorities->items[i].sonority);
        free(sonorities->items[i].held);
    }
    free(sonorities->items);
    sonorities->items = NULL;
    sonorities->count = 0;
}

/* One AdaptiveSonority for a non-empty segment. */
static AdaptiveSegmentationResult assemble_one(NoteEvent *const *notes, const Segment *segment,
                                                AdaptiveSonority *out) {
    MusicalTime *start = er_to_time(segment->start);
    MusicalTime *end = er_to_time(segment->end);
    if (start == NULL || end == NULL) {
        musical_time_destroy(start);
        musical_time_destroy(end);
        return AD_SEGMENTATION_ERR_OVERFLOW;
    }
    TimeSpan *window = NULL;
    TimeSpanResult span_result = time_span_create(start, end, TIME_SPAN_BOUNDARY_LEFT_CLOSED, &window);
    musical_time_destroy(start);
    musical_time_destroy(end);
    if (span_result != TIME_SPAN_OK) {
        return AD_SEGMENTATION_ERR_ALLOC;
    }

    EntityID **ids = (EntityID **)calloc(segment->member_count, sizeof(EntityID *));
    bool *held = (bool *)calloc(segment->member_count, sizeof(bool));
    AdaptiveSegmentationResult failure = AD_SEGMENTATION_OK;
    if (ids == NULL || held == NULL) {
        failure = AD_SEGMENTATION_ERR_ALLOC;
    } else {
        for (size_t i = 0; i < segment->member_count; i++) {
            const EntityID *note_id = note_event_id(notes[segment->members[i].note]);
            if (entity_id_create(entity_id_value(note_id), &ids[i]) != ENTITY_ID_OK) {
                failure = AD_SEGMENTATION_ERR_ALLOC;
                break;
            }
            held[i] = !segment->members[i].attacked;
        }
    }

    Sonority *sonority = NULL;
    if (failure == AD_SEGMENTATION_OK &&
        sonority_create(window, ids, segment->member_count, NULL, 0, SONORITY_EVENT_DOMAIN_SOUNDING, &sonority) !=
            SONORITY_OK) {
        failure = AD_SEGMENTATION_ERR_ALLOC;
    }
    if (failure != AD_SEGMENTATION_OK) {
        if (ids != NULL) {
            for (size_t i = 0; i < segment->member_count; i++) {
                entity_id_destroy(ids[i]);
            }
        }
        free(ids);
        free(held);
        time_span_destroy(window);
        return failure;
    }
    /* sonority_create took the window and the ids; the array itself is ours. */
    free(ids);
    out->sonority = sonority;
    out->held = held;
    return AD_SEGMENTATION_OK;
}

static AdaptiveSegmentationResult validate_config(const AdaptiveSegmentationConfig *config) {
    if (config->min_split_denominator == 0 || config->min_split_numerator > config->min_split_denominator) {
        return AD_SEGMENTATION_ERR_BAD_CONFIG;
    }
    return AD_SEGMENTATION_OK;
}

AdaptiveSegmentationResult sonority_builder_build_adaptive(NoteEvent *const *notes, size_t note_count,
                                                             const MeterMap *map, BeatGridProvider provider,
                                                             void *provider_context,
                                                             const AdaptiveSegmentationConfig *config,
                                                             AdaptiveSonorities *out) {
    if (notes == NULL || note_count == 0 || map == NULL || provider == NULL || out == NULL) {
        return AD_SEGMENTATION_ERR_NULL_ARGUMENT;
    }
    AdaptiveSegmentationConfig effective = config != NULL ? *config : adaptive_segmentation_config_default();
    AdaptiveSegmentationResult checked = validate_config(&effective);
    if (checked != AD_SEGMENTATION_OK) {
        return checked;
    }

    NoteSpan *spans = NULL;
    AdaptiveSegmentationResult prepared = segmentation_prepare_notes(notes, note_count, &spans);
    if (prepared != AD_SEGMENTATION_OK) {
        return prepared;
    }

    SegmentList segments = {NULL, 0};
    AdaptiveSegmentationResult result =
        segmentation_cut(spans, note_count, map, provider, provider_context, &segments);
    if (result == AD_SEGMENTATION_OK) {
        result = segmentation_merge(spans, &effective, &segments);
    }

    AdaptiveSonorities assembled = {NULL, 0};
    if (result == AD_SEGMENTATION_OK) {
        size_t non_empty = 0;
        for (size_t i = 0; i < segments.count; i++) {
            if (segments.items[i].member_count > 0) {
                non_empty++;
            }
        }
        if (non_empty > 0) {
            assembled.items = (AdaptiveSonority *)calloc(non_empty, sizeof(AdaptiveSonority));
            if (assembled.items == NULL) {
                result = AD_SEGMENTATION_ERR_ALLOC;
            }
        }
        for (size_t i = 0; i < segments.count && result == AD_SEGMENTATION_OK; i++) {
            if (segments.items[i].member_count == 0) {
                continue;
            }
            result = assemble_one(notes, &segments.items[i], &assembled.items[assembled.count]);
            if (result == AD_SEGMENTATION_OK) {
                assembled.count++;
            }
        }
    }

    segment_list_destroy(&segments);
    free(spans);
    if (result != AD_SEGMENTATION_OK) {
        adaptive_sonorities_destroy(&assembled);
        return result;
    }
    *out = assembled;
    return AD_SEGMENTATION_OK;
}
