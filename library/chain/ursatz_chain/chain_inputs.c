#include "ursatz_chain/chain_inputs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/rational.h"
#include "metric_position_rules.h"
#include "segmentation_beat_grid.h"
#include "time/metrical_position.h"
#include "time/time_rational.h"
#include "time/time_span.h"

typedef struct {
    const char *id;
    size_t index;
} NoteKey;

static int compare_keys(const void *a, const void *b) {
    return strcmp(((const NoteKey *)a)->id, ((const NoteKey *)b)->id);
}

static bool rational_of(const MusicalTime *time, Rational *out) {
    return rational_of_musical_time(time, out) == RATIONAL_OK;
}

ChainInputStatus chain_build_segments(const AnalysisNoteSet *notes, const AdaptiveSegmentationConfig *config, ChainSegments *out) {
    if (notes == NULL || out == NULL) {
        return CHAIN_INPUT_ERR_NULL_ARGUMENT;
    }
    if (!analysis_note_set_valid(notes) || notes->count == 0) {
        return CHAIN_INPUT_ERR_BAD_NOTES;
    }
    NoteEvent **events = calloc(notes->count, sizeof(NoteEvent *));
    if (events == NULL) {
        return CHAIN_INPUT_ERR_ALLOC;
    }
    for (size_t i = 0; i < notes->count; i++) {
        if (notes->notes[i].event == NULL) {
            free(events);
            return CHAIN_INPUT_ERR_NO_EVENT;
        }
        events[i] = (NoteEvent *)notes->notes[i].event; /* read only by segmentation */
    }
    ChainSegments built;
    memset(&built, 0, sizeof(built));
    AdaptiveSegmentationResult segmented = sonority_builder_build_adaptive(events, notes->count, notes->map,
                                                                            standard_meter_segmentation_beat_grid, NULL, config,
                                                                            &built.sonorities);
    free(events);
    if (segmented != AD_SEGMENTATION_OK) {
        return CHAIN_INPUT_ERR_SEGMENTATION;
    }
    built.count = built.sonorities.count;
    built.ids = calloc(built.count > 0 ? built.count : 1, sizeof(char[32]));
    built.starts = calloc(built.count > 0 ? built.count : 1, sizeof(double));
    built.ends = calloc(built.count > 0 ? built.count : 1, sizeof(double));
    if (built.ids == NULL || built.starts == NULL || built.ends == NULL) {
        chain_segments_destroy(&built);
        return CHAIN_INPUT_ERR_ALLOC;
    }
    for (size_t s = 0; s < built.count; s++) {
        const TimeSpan *window = sonority_time_window(built.sonorities.items[s].sonority);
        Rational start;
        Rational end;
        if (!rational_of(time_span_start(window), &start) || !rational_of(time_span_end(window), &end)) {
            chain_segments_destroy(&built);
            return CHAIN_INPUT_ERR_SEGMENTATION;
        }
        built.starts[s] = rational_to_double(start);
        built.ends[s] = rational_to_double(end);
        snprintf(built.ids[s], sizeof(built.ids[s]), "seg-%zu", s);
    }
    *out = built;
    return CHAIN_INPUT_OK;
}

BeatStrength chain_attack_strength(const MeterMap *map, const MusicalTime *onset) {
    MetricalPosition *position = NULL;
    BeatStrength strength = BEAT_STRENGTH_WEAK;
    if (map == NULL || onset == NULL || metrical_position_from_time(map, onset, &position) != METRICAL_POSITION_OK) {
        return strength;
    }
    const TimeSignature *reading = NULL;
    if (metrical_position_reading_signature(map, metrical_position_measure(position), &reading) == METRICAL_POSITION_OK) {
        BeatStrength found;
        if (standard_meter_beat_strength(reading, position, &found)) {
            strength = found;
        }
    }
    metrical_position_destroy(position);
    return strength;
}

/* The beat a segment [start, end) lies inside, as a non-zero index (measure and beat), or 0 when it spans beats or the grid cannot be
   read. It is CommonPracticeSegment.beat, which the context chord reading shares. */
static long beat_index(const MeterMap *map, Rational start, Rational end) {
    MusicalTime *at = musical_time_from_rational_or_null(start);
    int measure = 0;
    bool found = at != NULL && meter_map_measure_index_at(map, at, &measure) == METER_MAP_OK;
    musical_time_destroy(at);
    MusicalTime **starts = NULL;
    size_t count = 0;
    if (!found || !standard_meter_segmentation_beat_grid(map, measure, NULL, &starts, &count)) {
        return 0;
    }
    TimeSpan *span = NULL;
    long index = 0;
    if (meter_map_measure_span(map, measure, &span) == METER_MAP_OK) {
        Rational measure_end;
        if (rational_of(time_span_end(span), &measure_end)) {
            for (size_t b = 0; b < count; b++) {
                Rational beat_start;
                Rational beat_end = measure_end;
                if (!rational_of(starts[b], &beat_start) || (b + 1 < count && !rational_of(starts[b + 1], &beat_end))) {
                    break;
                }
                if (rational_cmp(beat_start, start) <= 0 && rational_cmp(start, beat_end) < 0) {
                    if (rational_cmp(end, beat_end) <= 0) {
                        index = ((long)measure + 1) * 1000 + (long)b + 1;
                    }
                    break;
                }
            }
        }
        time_span_destroy(span);
    }
    for (size_t b = 0; b < count; b++) {
        musical_time_destroy(starts[b]);
    }
    free(starts);
    return index;
}

ChainInputStatus chain_build_members(const AnalysisNoteSet *notes, ChainSegments *segments) {
    if (notes == NULL || segments == NULL) {
        return CHAIN_INPUT_ERR_NULL_ARGUMENT;
    }
    if (!analysis_note_set_valid(notes) || notes->count == 0) {
        return CHAIN_INPUT_ERR_BAD_NOTES;
    }
    NoteKey *keys = malloc(notes->count * sizeof(NoteKey));
    if (keys == NULL) {
        return CHAIN_INPUT_ERR_ALLOC;
    }
    for (size_t i = 0; i < notes->count; i++) {
        keys[i].id = notes->notes[i].note_id;
        keys[i].index = i;
    }
    qsort(keys, notes->count, sizeof(NoteKey), compare_keys);

    size_t total = 0;
    for (size_t s = 0; s < segments->count; s++) {
        total += sonority_event_id_count(segments->sonorities.items[s].sonority);
    }
    CommonPracticeMember *members = calloc(total > 0 ? total : 1, sizeof(CommonPracticeMember));
    CommonPracticeSegment *views = calloc(segments->count > 0 ? segments->count : 1, sizeof(CommonPracticeSegment));
    if (members == NULL || views == NULL) {
        free(keys);
        free(members);
        free(views);
        return CHAIN_INPUT_ERR_ALLOC;
    }
    ChainInputStatus status = CHAIN_INPUT_OK;
    size_t next = 0;
    for (size_t s = 0; s < segments->count && status == CHAIN_INPUT_OK; s++) {
        const AdaptiveSonority *segment = &segments->sonorities.items[s];
        const TimeSpan *window = sonority_time_window(segment->sonority);
        Rational start;
        Rational end;
        if (!rational_of(time_span_start(window), &start) || !rational_of(time_span_end(window), &end)) {
            status = CHAIN_INPUT_ERR_MEMBER;
            break;
        }
        size_t count = sonority_event_id_count(segment->sonority);
        views[s].members = &members[next];
        views[s].member_count = count;
        views[s].id = segments->ids[s];
        views[s].beat = beat_index(notes->map, start, end);
        for (size_t j = 0; j < count; j++) {
            NoteKey probe = {entity_id_value(sonority_event_id_at(segment->sonority, j)), 0};
            const NoteKey *found = bsearch(&probe, keys, notes->count, sizeof(NoteKey), compare_keys);
            if (found == NULL) {
                status = CHAIN_INPUT_ERR_UNKNOWN_NOTE;
                break;
            }
            const AnalysisNote *note = &notes->notes[found->index];
            CommonPracticeMember *member = &members[next];
            if (!common_practice_member_from_analysis_note(note, window, BEAT_STRENGTH_STRONG, member)) {
                status = CHAIN_INPUT_ERR_MEMBER;
                break;
            }
            member->held = segment->held[j];
            member->strength = member->held ? BEAT_STRENGTH_STRONG : chain_attack_strength(notes->map, time_span_start(note->span));
            next++;
        }
    }
    free(keys);
    if (status != CHAIN_INPUT_OK) {
        free(members);
        free(views);
        return status;
    }
    free(segments->members);
    free(segments->segments);
    segments->members = members;
    segments->segments = views;
    return CHAIN_INPUT_OK;
}

void chain_segments_destroy(ChainSegments *segments) {
    if (segments == NULL) {
        return;
    }
    adaptive_sonorities_destroy(&segments->sonorities);
    free(segments->ids);
    free(segments->starts);
    free(segments->ends);
    free(segments->members);
    free(segments->segments);
    memset(segments, 0, sizeof(*segments));
}

ChainInputStatus chain_build_key_segments(const ChainSegments *segments, size_t count, const CommonPracticeWeightedConfig *weighting,
                                          CommonPracticeKeySegment **out) {
    if (segments == NULL || out == NULL) {
        return CHAIN_INPUT_ERR_NULL_ARGUMENT;
    }
    if (segments->segments == NULL || count == 0 || count > segments->count) {
        return CHAIN_INPUT_ERR_BAD_NOTES;
    }
    CommonPracticeWeightedConfig defaults = common_practice_weighted_config_default();
    if (weighting == NULL) {
        weighting = &defaults;
    }
    CommonPracticeKeySegment *built = calloc(count, sizeof(CommonPracticeKeySegment));
    if (built == NULL) {
        return CHAIN_INPUT_ERR_ALLOC;
    }
    for (size_t s = 0; s < count; s++) {
        const TimeSpan *window = sonority_time_window(segments->sonorities.items[s].sonority);
        built[s].start = time_span_start(window);
        built[s].end = time_span_end(window);
        built[s].source = &segments->segments[s];
        if (common_practice_weight_segment(segments->segments[s].members, segments->segments[s].member_count, weighting,
                                           &built[s].weights) != COMMON_PRACTICE_WEIGHTED_OK) {
            free(built);
            return CHAIN_INPUT_ERR_WEIGHTS;
        }
    }
    *out = built;
    return CHAIN_INPUT_OK;
}

void chain_key_segments_destroy(CommonPracticeKeySegment *key_segments) {
    free(key_segments);
}

ChainInputStatus chain_estimate_key_regions(const CommonPracticeKeySegment *key_segments, size_t count, const MeterMap *map,
                                            const AnalysisKeyEvent *events, size_t event_count,
                                            const CommonPracticeKeyRegionsConfig *config, CommonPracticeKeyRegions *out) {
    if (key_segments == NULL || map == NULL || out == NULL || (event_count > 0 && events == NULL)) {
        return CHAIN_INPUT_ERR_NULL_ARGUMENT;
    }
    CommonPracticeKeySignatureEvent *signatures = NULL;
    if (event_count > 0) {
        signatures = calloc(event_count, sizeof(*signatures));
        if (signatures == NULL) {
            return CHAIN_INPUT_ERR_ALLOC;
        }
        for (size_t i = 0; i < event_count; i++) {
            if (!common_practice_key_signature_event_from_analysis(&events[i], &signatures[i])) {
                free(signatures);
                return CHAIN_INPUT_ERR_KEYS;
            }
        }
    }
    CommonPracticeKeyRegionsStatus status = key_regions_estimate(key_segments, count, map, signatures, event_count, config, out);
    free(signatures);
    return status == COMMON_PRACTICE_KEY_REGIONS_OK ? CHAIN_INPUT_OK : CHAIN_INPUT_ERR_KEYS;
}
