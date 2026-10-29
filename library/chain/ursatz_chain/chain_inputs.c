#include "ursatz_chain/chain_inputs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/rational.h"
#include "harmony_key_change.h"
#include "semantics/analysis_segment.h"
#include "time/duration.h"
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

/* The length of a beat, from the first measure's beat grid. */
static bool beat_duration(const MeterMap *map, Duration **out) {
    MusicalTime **starts = NULL;
    size_t count = 0;
    TimeSpan *span = NULL;
    if (!standard_meter_segmentation_beat_grid(map, 1, NULL, &starts, &count) || count == 0) {
        return false;
    }
    bool ok = false;
    if (meter_map_measure_span(map, 1, &span) == METER_MAP_OK) {
        Rational first;
        Rational next;
        Rational length;
        ok = rational_of(starts[0], &first) &&
             (count > 1 ? rational_of(starts[1], &next) : rational_of(time_span_end(span), &next)) &&
             rational_sub(next, first, &length) == RATIONAL_OK &&
             duration_create((long)length.num, (unsigned long)length.den, out) == DURATION_OK;
        time_span_destroy(span);
    }
    for (size_t b = 0; b < count; b++) {
        musical_time_destroy(starts[b]);
    }
    free(starts);
    return ok;
}

static const int DEGREE_OF_OFFSET[12] = {1, 0, 2, 0, 3, 4, 0, 5, 0, 6, 0, 7};

/* The degree (1..7) of an outer line's note in the major key on `tonic`, or 0 for a chromatic or an absent note. */
static int degree_of(const OuterVoiceEntry *entry, int tonic) {
    if (entry == NULL || !entry->present) {
        return 0;
    }
    long position = pitch_numeric_value_position_numerator(entry->pitch);
    return DEGREE_OF_OFFSET[(((position - tonic) % 12) + 12) % 12];
}

static void claim_intervals(const StandardTonalityRomanClaim *claim, int out[4], size_t *count) {
    static const int TRIADS[4][3] = {{0, 4, 7}, {0, 3, 7}, {0, 3, 6}, {0, 4, 8}};
    static const int SEVENTHS[5][4] = {{0, 4, 7, 11}, {0, 4, 7, 10}, {0, 3, 7, 10}, {0, 3, 6, 10}, {0, 3, 6, 9}};
    memset(out, 0, 4 * sizeof(int));
    if (claim->seventh) {
        memcpy(out, SEVENTHS[claim->quality], 4 * sizeof(int));
        *count = 4;
    } else {
        memcpy(out, TRIADS[claim->quality], 3 * sizeof(int));
        *count = 3;
    }
}

ChainInputStatus chain_build_harmony_segments(const ChainSegments *segments, size_t count, const MeterMap *map,
                                              const StandardTonalityChordInput *chords, const StandardTonalityRomanResult *romans,
                                              const OuterVoiceLines *lines, ChainHarmonyInputs *out) {
    if (segments == NULL || map == NULL || chords == NULL || romans == NULL || lines == NULL || out == NULL) {
        return CHAIN_INPUT_ERR_NULL_ARGUMENT;
    }
    if (count == 0 || count > segments->count || count > lines->count) {
        return CHAIN_INPUT_ERR_BAD_NOTES;
    }
    ChainHarmonyInputs built;
    memset(&built, 0, sizeof(built));
    built.count = count;
    built.inputs = calloc(count, sizeof(HarmonySegmentInput));
    built.positions = calloc(count, sizeof(MetricalPosition *));
    if (built.inputs == NULL || built.positions == NULL) {
        chain_harmony_inputs_destroy(&built);
        return CHAIN_INPUT_ERR_ALLOC;
    }
    if (!beat_duration(map, &built.beat_length)) {
        chain_harmony_inputs_destroy(&built);
        return CHAIN_INPUT_ERR_KEYS;
    }
    for (size_t s = 0; s < count; s++) {
        const TimeSpan *window = sonority_time_window(segments->sonorities.items[s].sonority);
        if (metrical_position_from_time(map, time_span_start(window), &built.positions[s]) != METRICAL_POSITION_OK) {
            chain_harmony_inputs_destroy(&built);
            return CHAIN_INPUT_ERR_KEYS;
        }
        HarmonySegmentInput *input = &built.inputs[s];
        AnalysisSegment segment = {segments->ids[s], window, built.positions[s], chain_attack_strength(map, time_span_start(window))};
        harmony_segment_input_set_segment(input, &segment);
        input->key_ref = "key";
        input->mode = HARMONY_MODE_MAJOR;
        input->beat_length = built.beat_length;
        if (!romans[s].has_numeral) {
            input->degree = 0;
            input->interval_count = 3;
            continue;
        }
        const StandardTonalityRomanClaim *claim = &romans[s].claim;
        int intervals[4];
        size_t interval_count = 0;
        claim_intervals(claim, intervals, &interval_count);
        memset(input->intervals, 0, sizeof(input->intervals));
        for (size_t k = 0; k < interval_count; k++) {
            input->intervals[k] = intervals[k];
        }
        input->interval_count = interval_count;
        input->degree = claim->degree;
        input->inversion = claim->inversion;
        input->applied_to = claim->applied ? claim->applied_target_degree : 0;
        input->mode = chords[s].mode == STANDARD_TONALITY_ROMAN_KEY_MINOR ? HARMONY_MODE_MINOR : HARMONY_MODE_MAJOR;
        input->uncertainty = romans[s].uncertainty;
        input->bass_degree = degree_of(&lines->bass[s], chords[s].tonic_pitch_class);
        input->soprano_degree = degree_of(&lines->soprano[s], chords[s].tonic_pitch_class);
    }
    *out = built;
    return CHAIN_INPUT_OK;
}

void chain_harmony_inputs_destroy(ChainHarmonyInputs *inputs) {
    if (inputs == NULL) {
        return;
    }
    for (size_t s = 0; s < inputs->count && inputs->positions != NULL; s++) {
        metrical_position_destroy(inputs->positions[s]);
    }
    duration_destroy(inputs->beat_length);
    free(inputs->positions);
    free(inputs->inputs);
    memset(inputs, 0, sizeof(*inputs));
}

static double time_double(const MusicalTime *time) {
    return (double)musical_time_numerator(time) / (double)musical_time_denominator(time);
}

ChainInputStatus chain_add_boundary_cadences(const HarmonySegment *prepared, size_t count, const CommonPracticeKeyRegions *regions,
                                             int home_key_index, HarmonyCadences *cadences) {
    if (prepared == NULL || regions == NULL || cadences == NULL) {
        return CHAIN_INPUT_ERR_NULL_ARGUMENT;
    }
    double *starts = calloc(regions->region_count + 1, sizeof(double));
    if (starts == NULL) {
        return CHAIN_INPUT_ERR_ALLOC;
    }
    size_t n = 0;
    for (size_t i = 1; i < regions->region_count; i++) {
        if (regions->regions[i].key_index == home_key_index && regions->regions[i - 1].key_index != home_key_index) {
            starts[n++] = time_double(time_span_start(regions->regions[i].span));
        }
    }
    HarmonyCadenceAnalysisStatus status = harmony_analysis_add_boundary_cadences(prepared, count, NULL, starts, n, cadences);
    free(starts);
    return status == HARMONY_CADENCE_ANALYSIS_OK ? CHAIN_INPUT_OK : CHAIN_INPUT_ERR_KEYS;
}

ChainInputStatus chain_confirm_key_changes(CommonPracticeKeyRegions *regions, const double *segment_starts, const HarmonyCadences *cadences) {
    if (regions == NULL || segment_starts == NULL || cadences == NULL) {
        return CHAIN_INPUT_ERR_NULL_ARGUMENT;
    }
    double *arrivals = calloc(cadences->count + 1, sizeof(double));
    if (arrivals == NULL) {
        return CHAIN_INPUT_ERR_ALLOC;
    }
    for (size_t k = 0; k < cadences->count; k++) {
        arrivals[k] = segment_starts[cadences->items[k].arrival_first];
    }
    ChainInputStatus status = CHAIN_INPUT_OK;
    for (size_t i = 0; i < regions->change_count && status == CHAIN_INPUT_OK; i++) {
        CommonPracticeKeyChange *change = &regions->changes[i];
        if (change->is_return) {
            continue;
        }
        const CommonPracticeKeyRegion *into = &regions->regions[change->to_region];
        KeyChangeExtent confirmed = change->extent;
        if (!harmony_key_change_confirm(change->extent, time_double(time_span_start(into->span)),
                                        time_double(time_span_end(into->span)), arrivals, cadences->count, &confirmed)) {
            status = CHAIN_INPUT_ERR_KEYS;
            break;
        }
        change->extent = confirmed;
        change->cadence_confirmed = key_change_extent_is_modulation(confirmed);
    }
    for (size_t i = 1; i < regions->change_count && status == CHAIN_INPUT_OK; i++) {
        if (regions->changes[i].is_return) {
            regions->changes[i].extent = regions->changes[i - 1].extent;
            regions->changes[i].cadence_confirmed = regions->changes[i - 1].cadence_confirmed;
        }
    }
    free(arrivals);
    return status;
}
