#include "ursatz_chain/chain_inputs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis/outer_voice.h"
#include "core/rational.h"
#include "standard-form/analysis/analysis_time.h"
#include "time/time_signature.h"
#include "standard-harmony/harmony_cadence_rules.h"
#include "standard-harmony/harmony_key_change.h"
#include "common-practice-minimal/outer_scale_degrees.h"
#include "semantics/analysis_segment.h"
#include "interpretation/chord.h"
#include "time/duration.h"
#include "time/temporal_anchor.h"
#include "standard-meter/metric_position_rules.h"
#include "standard-meter/segmentation_beat_grid.h"
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

/* --- the notes' pitches --- */

ChainInputStatus chain_build_pitches(const AnalysisNoteSet *notes, ChainPitches *out) {
    if (notes == NULL || out == NULL) {
        return CHAIN_INPUT_ERR_NULL_ARGUMENT;
    }
    if (!analysis_note_set_valid(notes) || notes->count == 0) {
        return CHAIN_INPUT_ERR_BAD_NOTES;
    }
    ChainPitches built;
    memset(&built, 0, sizeof(built));
    built.events = calloc(notes->count, sizeof(NoteEvent *));
    built.values = calloc(notes->count, sizeof(PitchNumericValue *));
    EntityID *id = NULL;
    if (built.events == NULL || built.values == NULL || entity_id_create("chain-midi", &id) != ENTITY_ID_OK) {
        chain_pitches_destroy(&built);
        return CHAIN_INPUT_ERR_ALLOC;
    }
    if (tuning_system_create(id, "MIDI note number", &built.tuning) != TUNING_SYSTEM_OK) {
        entity_id_destroy(id);
        chain_pitches_destroy(&built);
        return CHAIN_INPUT_ERR_ALLOC;
    }
    for (size_t i = 0; i < notes->count; i++) {
        const AnalysisNote *note = &notes->notes[i];
        if (note->event == NULL) {
            chain_pitches_destroy(&built);
            return CHAIN_INPUT_ERR_NO_EVENT;
        }
        if (note->midi == ANALYSIS_PITCH_UNKNOWN) {
            chain_pitches_destroy(&built);
            return CHAIN_INPUT_ERR_BAD_NOTES;
        }
        built.events[i] = (NoteEvent *)note->event; /* read only by the kernel's analyses */
        if (pitch_numeric_value_create(built.tuning, note->midi, 1, &built.values[i]) != PITCH_NUMERIC_VALUE_OK) {
            chain_pitches_destroy(&built);
            return CHAIN_INPUT_ERR_ALLOC;
        }
        built.count = i + 1;
    }
    *out = built;
    return CHAIN_INPUT_OK;
}

void chain_pitches_destroy(ChainPitches *pitches) {
    if (pitches == NULL) {
        return;
    }
    for (size_t i = 0; i < pitches->count && pitches->values != NULL; i++) {
        pitch_numeric_value_destroy(pitches->values[i]);
    }
    free(pitches->events);
    free(pitches->values);
    tuning_system_destroy(pitches->tuning);
    memset(pitches, 0, sizeof(*pitches));
}

/* --- the form input --- */

static const TimeSpan *form_span(ChainFormInput *build, const MusicalTime *start, const MusicalTime *end) {
    if (build->span_count == build->span_capacity) {
        size_t capacity = build->span_capacity == 0 ? 256 : build->span_capacity * 2;
        TimeSpan **grown = realloc(build->spans, capacity * sizeof(TimeSpan *));
        if (grown == NULL) {
            return NULL;
        }
        build->spans = grown;
        build->span_capacity = capacity;
    }
    TimeSpan *span = NULL;
    if (time_span_create(start, end, TIME_SPAN_BOUNDARY_LEFT_CLOSED, &span) != TIME_SPAN_OK) {
        return NULL;
    }
    build->spans[build->span_count++] = span;
    return span;
}

void chain_form_input_destroy(ChainFormInput *input) {
    if (input == NULL) {
        return;
    }
    standard_form_input_destroy(&input->input);
    for (size_t i = 0; i < input->span_count; i++) {
        time_span_destroy(input->spans[i]);
    }
    free(input->spans);
    memset(input, 0, sizeof(*input));
}

static int measure_of(const MeterMap *map, const MusicalTime *time) {
    int measure = 0;
    return meter_map_measure_index_at(map, time, &measure) == METER_MAP_OK ? measure : -1;
}

/* The note with this id, or NULL. `keys` is the set's ids sorted. */
static const AnalysisNote *find_note(const AnalysisNoteSet *notes, const NoteKey *keys, const char *id) {
    NoteKey probe = {id, 0};
    const NoteKey *found = bsearch(&probe, keys, notes->count, sizeof(NoteKey), compare_keys);
    return found != NULL ? &notes->notes[found->index] : NULL;
}

static NoteKey *sorted_keys(const AnalysisNoteSet *notes) {
    NoteKey *keys = malloc((notes->count > 0 ? notes->count : 1) * sizeof(NoteKey));
    if (keys == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < notes->count; i++) {
        keys[i].id = notes->notes[i].note_id;
        keys[i].index = i;
    }
    qsort(keys, notes->count, sizeof(NoteKey), compare_keys);
    return keys;
}

static ChainInputStatus add_cadences(ChainFormInput *build, const ChainSegments *segments, const HarmonyCadences *cadences) {
    for (size_t i = 0; cadences != NULL && i < cadences->count; i++) {
        const HarmonyCadenceResult *c = &cadences->items[i];
        if (c->arrival_first >= segments->count || c->arrival_last >= segments->count) {
            return CHAIN_INPUT_ERR_BAD_NOTES;
        }
        const TimeSpan *first = sonority_time_window(segments->sonorities.items[c->arrival_first].sonority);
        const TimeSpan *last = sonority_time_window(segments->sonorities.items[c->arrival_last].sonority);
        const TimeSpan *span = form_span(build, time_span_start(first), time_span_end(last));
        char id[32];
        snprintf(id, sizeof(id), "chain-%zu", i);
        if (span == NULL) {
            return CHAIN_INPUT_ERR_ALLOC;
        }
        if (standard_form_input_add_cadence(&build->input, id, span, c->type, harmony_cadence_strength(c->type), c->post_cadential,
                                            c->uncertainty) != STANDARD_FORM_ANALYSIS_OK) {
            return CHAIN_INPUT_ERR_BAD_NOTES;
        }
    }
    return CHAIN_INPUT_OK;
}

ChainInputStatus chain_build_form_input(const AnalysisNoteSet *notes, const ChainSegments *segments, const OuterVoiceLines *lines,
                                        int first_measure, int last_measure, const HarmonyCadences *cadences, ChainFormInput *out) {
    if (notes == NULL || segments == NULL || lines == NULL || out == NULL) {
        return CHAIN_INPUT_ERR_NULL_ARGUMENT;
    }
    if (!analysis_note_set_valid(notes) || segments->count == 0 || lines->count < segments->count) {
        return CHAIN_INPUT_ERR_BAD_NOTES;
    }
    NoteKey *keys = sorted_keys(notes);
    if (keys == NULL) {
        return CHAIN_INPUT_ERR_ALLOC;
    }
    ChainFormInput build;
    memset(&build, 0, sizeof(build));
    standard_form_input_init(&build.input, notes->map);
    ChainInputStatus status = CHAIN_INPUT_OK;
    const char *current_id = NULL;
    const MusicalTime *current_start = NULL;
    const MusicalTime *current_end = NULL;
    int current_semitone = 0;
    for (size_t s = 0; s <= segments->count && status == CHAIN_INPUT_OK; s++) {
        const TimeSpan *window = s < segments->count ? sonority_time_window(segments->sonorities.items[s].sonority) : NULL;
        int measure = window != NULL ? measure_of(notes->map, time_span_start(window)) : -1;
        bool in_range = window != NULL && measure >= first_measure && measure <= last_measure;
        const char *id = in_range && lines->soprano[s].present ? entity_id_value(lines->soprano[s].note_id) : NULL;
        if (current_id != NULL && (id == NULL || strcmp(id, current_id) != 0)) {
            const AnalysisNote *source = find_note(notes, keys, current_id);
            const TimeSpan *span = source != NULL ? form_span(&build, current_start, current_end) : NULL;
            if (source == NULL || span == NULL) {
                status = source == NULL ? CHAIN_INPUT_ERR_UNKNOWN_NOTE : CHAIN_INPUT_ERR_ALLOC;
                break;
            }
            AnalysisNote line_note = {current_id, source->event, span, current_semitone, NULL, NULL, ANALYSIS_INDEX_NONE,
                                      ANALYSIS_INDEX_NONE, 1.0};
            if (standard_form_input_add_analysis_note(&build.input, &line_note, false) != STANDARD_FORM_ANALYSIS_OK) {
                status = CHAIN_INPUT_ERR_BAD_NOTES;
                break;
            }
            current_id = NULL;
        }
        if (id == NULL) {
            continue;
        }
        if (current_id == NULL) {
            current_id = id;
            current_start = time_span_start(window);
            current_semitone = (int)pitch_numeric_value_position_numerator(lines->soprano[s].pitch);
        }
        current_end = time_span_end(window);
    }
    for (size_t s = 0; s < segments->count && status == CHAIN_INPUT_OK; s++) {
        const TimeSpan *window = sonority_time_window(segments->sonorities.items[s].sonority);
        int measure = measure_of(notes->map, time_span_start(window));
        if (measure >= first_measure && measure <= last_measure) {
            AnalysisSegment segment = {segments->ids[s], window, NULL, BEAT_STRENGTH_WEAK};
            if (standard_form_input_add_analysis_segment(&build.input, &segment, 0, 0) != STANDARD_FORM_ANALYSIS_OK) {
                status = CHAIN_INPUT_ERR_BAD_NOTES;
            }
        }
    }
    free(keys);
    if (status == CHAIN_INPUT_OK) {
        status = add_cadences(&build, segments, cadences);
    }
    if (status == CHAIN_INPUT_OK && standard_form_input_finish(&build.input) != STANDARD_FORM_ANALYSIS_OK) {
        status = CHAIN_INPUT_ERR_BAD_NOTES;
    }
    if (status != CHAIN_INPUT_OK) {
        chain_form_input_destroy(&build);
        return status;
    }
    *out = build;
    return CHAIN_INPUT_OK;
}

typedef struct {
    size_t index; /* into the note set */
    StandardFormQ start;
    StandardFormQ end;
    long semitone;
} MotiveNote;

static StandardFormQ q_of_time(const MusicalTime *time) {
    return standard_form_q(musical_time_numerator(time), (long long)musical_time_denominator(time));
}

static bool parse_degree(const char *label, int *degree, int *alteration) {
    *alteration = 0;
    if (label == NULL) {
        return false;
    }
    if (*label == 'b') {
        *alteration = -1;
        label++;
    } else if (*label == '#') {
        *alteration = 1;
        label++;
    }
    if (*label < '1' || *label > '7') {
        return false;
    }
    *degree = *label - '0';
    return true;
}

/* The soprano notes of the measures [first, last], in time order. *out is allocated; its count is returned.
   Only notes starting in the measures compete, so a note held in from an earlier measure does not hide an attack. */
static bool motive_soprano(const AnalysisNoteSet *notes, int first_measure, int last_measure, MotiveNote **out, size_t *kept_out) {
    AnalysisNote *window = calloc(notes->count > 0 ? notes->count : 1, sizeof(AnalysisNote));
    size_t *origin = calloc(notes->count > 0 ? notes->count : 1, sizeof(size_t));
    if (window == NULL || origin == NULL) {
        free(window);
        free(origin);
        return false;
    }
    size_t count = 0;
    for (size_t i = 0; i < notes->count; i++) {
        int measure = measure_of(notes->map, time_span_start(notes->notes[i].span));
        if (measure < first_measure || measure > last_measure) {
            continue;
        }
        window[count] = notes->notes[i];
        origin[count] = i;
        count++;
    }
    AnalysisNoteSet in_window = {window, count, notes->map};
    size_t *picked = NULL;
    size_t kept = 0;
    MotiveNote *line = NULL;
    bool ok = count == 0 || outer_voice_attack_soprano(&in_window, &picked, &kept) == OUTER_VOICE_OK;
    if (ok) {
        line = calloc(kept > 0 ? kept : 1, sizeof(MotiveNote));
        ok = line != NULL;
    }
    for (size_t k = 0; ok && k < kept; k++) {
        const AnalysisNote *source = &window[picked[k]];
        line[k].index = origin[picked[k]];
        line[k].start = q_of_time(time_span_start(source->span));
        line[k].end = q_of_time(time_span_end(source->span));
        line[k].semitone = source->midi;
    }
    free(window);
    free(origin);
    free(picked);
    if (!ok) {
        free(line);
        return false;
    }
    *out = line;
    *kept_out = kept;
    return true;
}

ChainInputStatus chain_build_motive_input(const AnalysisNoteSet *notes, const ChainSegments *segments, int first_measure,
                                          int last_measure, int tonic_pitch_class, CommonPracticeKeyMode mode,
                                          const HarmonyCadences *cadences, ChainFormInput *out) {
    if (notes == NULL || segments == NULL || out == NULL) {
        return CHAIN_INPUT_ERR_NULL_ARGUMENT;
    }
    if (!analysis_note_set_valid(notes) || notes->count == 0) {
        return CHAIN_INPUT_ERR_BAD_NOTES;
    }
    MotiveNote *line = NULL;
    size_t count = 0;
    if (!motive_soprano(notes, first_measure, last_measure, &line, &count)) {
        return CHAIN_INPUT_ERR_ALLOC;
    }
    ChainFormInput build;
    memset(&build, 0, sizeof(build));
    standard_form_input_init(&build.input, notes->map);
    ChainInputStatus status = CHAIN_INPUT_OK;
    for (size_t k = 0; k < count && status == CHAIN_INPUT_OK; k++) {
        const AnalysisNote *source = &notes->notes[line[k].index];
        /* the span is the note's own, borrowed from the note set */
        if (standard_form_input_add_analysis_note(&build.input, source, false) != STANDARD_FORM_ANALYSIS_OK) {
            status = CHAIN_INPUT_ERR_BAD_NOTES;
            break;
        }
        int pitch_class = (int)(((line[k].semitone % 12) + 12) % 12);
        int degree = 0;
        int alteration = 0;
        if (!parse_degree(common_practice_scale_degree_label(pitch_class, tonic_pitch_class, mode), &degree, &alteration) ||
            standard_form_input_set_degree(&build.input, false, k, degree, alteration) != STANDARD_FORM_ANALYSIS_OK) {
            status = CHAIN_INPUT_ERR_BAD_NOTES;
        }
    }
    free(line);
    if (status == CHAIN_INPUT_OK) {
        status = add_cadences(&build, segments, cadences);
    }
    if (status == CHAIN_INPUT_OK && standard_form_input_finish(&build.input) != STANDARD_FORM_ANALYSIS_OK) {
        status = CHAIN_INPUT_ERR_BAD_NOTES;
    }
    if (status != CHAIN_INPUT_OK) {
        chain_form_input_destroy(&build);
        return status;
    }
    *out = build;
    return CHAIN_INPUT_OK;
}

/* --- the meter input --- */

static const TimeSpan *meter_span(ChainMeterInput *build, const MusicalTime *start, const MusicalTime *end) {
    if (build->span_count == build->span_capacity) {
        size_t capacity = build->span_capacity == 0 ? 512 : build->span_capacity * 2;
        TimeSpan **grown = realloc(build->spans, capacity * sizeof(TimeSpan *));
        if (grown == NULL) {
            return NULL;
        }
        build->spans = grown;
        build->span_capacity = capacity;
    }
    TimeSpan *span = NULL;
    if (time_span_create(start, end, TIME_SPAN_BOUNDARY_LEFT_CLOSED, &span) != TIME_SPAN_OK) {
        return NULL;
    }
    build->spans[build->span_count++] = span;
    return span;
}

static const MusicalTime *meter_time(ChainMeterInput *build, const MusicalTime *time) {
    if (build->time_count == build->time_capacity) {
        size_t capacity = build->time_capacity == 0 ? 128 : build->time_capacity * 2;
        MusicalTime **grown = realloc(build->times, capacity * sizeof(MusicalTime *));
        if (grown == NULL) {
            return NULL;
        }
        build->times = grown;
        build->time_capacity = capacity;
    }
    MusicalTime *copy = NULL;
    if (musical_time_create(musical_time_numerator(time), musical_time_denominator(time), &copy) != MUSICAL_TIME_OK) {
        return NULL;
    }
    build->times[build->time_count++] = copy;
    return copy;
}

void chain_meter_input_destroy(ChainMeterInput *input) {
    if (input == NULL) {
        return;
    }
    standard_meter_input_destroy(&input->input);
    for (size_t i = 0; i < input->span_count; i++) {
        time_span_destroy(input->spans[i]);
    }
    for (size_t i = 0; i < input->time_count; i++) {
        musical_time_destroy(input->times[i]);
    }
    free(input->spans);
    free(input->times);
    memset(input, 0, sizeof(*input));
}

static int compare_ids(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* The ids of the notes the line takes, sorted. */
static const char **line_ids(const OuterVoiceEntry *entries, size_t count, size_t *out_count) {
    const char **ids = malloc((count == 0 ? 1 : count) * sizeof(char *));
    if (ids == NULL) {
        return NULL;
    }
    size_t n = 0;
    for (size_t s = 0; s < count; s++) {
        if (entries[s].present && entries[s].note_id != NULL) {
            ids[n++] = entity_id_value(entries[s].note_id);
        }
    }
    qsort(ids, n, sizeof(char *), compare_ids);
    *out_count = n;
    return ids;
}

static bool in_ids(const char **ids, size_t count, const char *id) {
    return bsearch(&id, ids, count, sizeof(char *), compare_ids) != NULL;
}

static StandardMeterExpansionKind meter_expansion_of(int kind) {
    switch (kind) {
    case STANDARD_FORM_EXPANSION_INTERNAL:
        return STANDARD_METER_EXPANSION_INTERNAL;
    case STANDARD_FORM_EXPANSION_EXTERNAL:
        return STANDARD_METER_EXPANSION_EXTERNAL;
    default:
        return STANDARD_METER_EXPANSION_CONTRACTION;
    }
}

ChainInputStatus chain_build_meter_input(const AnalysisNoteSet *notes, const OuterVoiceLines *lines, const ChainSegments *segments,
                                         const char *const *chord_labels, int first_measure, int last_measure,
                                         const StandardFormInput *form, const StandardFormPhrases *phrases,
                                         const StandardFormExpansions *expansions, ChainMeterInput *out) {
    if (notes == NULL || lines == NULL || segments == NULL || out == NULL) {
        return CHAIN_INPUT_ERR_NULL_ARGUMENT;
    }
    if (!analysis_note_set_valid(notes) || notes->count == 0) {
        return CHAIN_INPUT_ERR_BAD_NOTES;
    }
    ChainMeterInput build;
    memset(&build, 0, sizeof(build));
    if (standard_meter_input_init(&build.input, notes->map) != STANDARD_METER_ANALYSIS_OK) {
        return CHAIN_INPUT_ERR_BAD_NOTES;
    }
    size_t soprano_count = 0;
    size_t bass_count = 0;
    const char **soprano = line_ids(lines->soprano, lines->count, &soprano_count);
    const char **bass = line_ids(lines->bass, lines->count, &bass_count);
    ChainInputStatus status = soprano != NULL && bass != NULL ? CHAIN_INPUT_OK : CHAIN_INPUT_ERR_ALLOC;
    for (size_t i = 0; i < notes->count && status == CHAIN_INPUT_OK; i++) {
        AnalysisNote note = notes->notes[i];
        int measure = measure_of(notes->map, time_span_start(note.span));
        if (measure < first_measure || measure > last_measure) {
            continue;
        }
        note.voice = in_ids(soprano, soprano_count, note.note_id) ? 0 : (in_ids(bass, bass_count, note.note_id) ? 1 : 2);
        if (standard_meter_input_add_analysis_note(&build.input, &note) != STANDARD_METER_ANALYSIS_OK) {
            status = CHAIN_INPUT_ERR_BAD_NOTES;
        }
    }
    free(soprano);
    free(bass);
    for (size_t s = 1; chord_labels != NULL && s < segments->count && status == CHAIN_INPUT_OK; s++) {
        if (chord_labels[s] == NULL || chord_labels[s - 1] == NULL || strcmp(chord_labels[s], chord_labels[s - 1]) == 0) {
            continue;
        }
        const MusicalTime *start = time_span_start(sonority_time_window(segments->sonorities.items[s].sonority));
        int measure = measure_of(notes->map, start);
        if (measure < first_measure || measure > last_measure) {
            continue;
        }
        char id[48];
        snprintf(id, sizeof(id), "chord-change-%zu", s);
        const MusicalTime *kept = meter_time(&build, start);
        if (kept == NULL || standard_meter_input_add_chord_change(&build.input, id, kept) != STANDARD_METER_ANALYSIS_OK) {
            status = kept == NULL ? CHAIN_INPUT_ERR_ALLOC : CHAIN_INPUT_ERR_BAD_NOTES;
        }
    }
    for (size_t i = 0; phrases != NULL && form != NULL && i < phrases->count && status == CHAIN_INPUT_OK; i++) {
        const StandardFormPhrase *phrase = &phrases->items[i];
        const TimeSpan *span = meter_span(&build, time_span_start(phrase->span), time_span_end(phrase->span));
        const TimeSpan *arrival = NULL;
        if (span == NULL) {
            status = CHAIN_INPUT_ERR_ALLOC;
            break;
        }
        if (phrase->claim.cadence_index >= 0) {
            const TimeSpan *chord = form->cadences[phrase->claim.cadence_index].arrival;
            arrival = meter_span(&build, time_span_start(chord), time_span_end(chord));
            if (arrival == NULL) {
                status = CHAIN_INPUT_ERR_ALLOC;
                break;
            }
        }
        StandardMeterExpansionKind expansion = STANDARD_METER_EXPANSION_NONE;
        int measures = 0;
        for (size_t e = 0; expansions != NULL && e < expansions->count; e++) {
            if (expansions->items[e].claim.phrase != phrase->claim.index) {
                continue;
            }
            expansion = meter_expansion_of(expansions->items[e].claim.kind);
            measures = expansions->items[e].claim.measures_changed;
            break;
        }
        char id[32];
        snprintf(id, sizeof(id), "phrase-%zu", i);
        if (standard_meter_input_add_phrase(&build.input, id, span, arrival, expansion, measures, false) !=
            STANDARD_METER_ANALYSIS_OK) {
            status = CHAIN_INPUT_ERR_BAD_NOTES;
        }
    }
    if (status != CHAIN_INPUT_OK) {
        chain_meter_input_destroy(&build);
        return status;
    }
    *out = build;
    return CHAIN_INPUT_OK;
}

/* --- the texture input --- */

void chain_texture_input_destroy(ChainTextureInput *input) {
    if (input == NULL) {
        return;
    }
    standard_texture_input_destroy(&input->input);
    voice_assignment_destroy(&input->assignment);
    memset(input, 0, sizeof(*input));
}

ChainInputStatus chain_build_texture_input(const AnalysisNoteSet *notes, const ChainPitches *pitches, ChainTextureInput *out) {
    if (notes == NULL || pitches == NULL || out == NULL) {
        return CHAIN_INPUT_ERR_NULL_ARGUMENT;
    }
    if (!analysis_note_set_valid(notes) || notes->count == 0 || pitches->count != notes->count) {
        return CHAIN_INPUT_ERR_BAD_NOTES;
    }
    size_t *stream = calloc(notes->count, sizeof(size_t));
    if (stream == NULL) {
        return CHAIN_INPUT_ERR_ALLOC;
    }
    size_t highest = 0;
    for (size_t i = 0; i < notes->count; i++) {
        stream[i] = notes->notes[i].track == ANALYSIS_INDEX_NONE ? 0 : notes->notes[i].track;
        highest = stream[i] > highest ? stream[i] : highest;
    }
    ChainTextureInput build;
    memset(&build, 0, sizeof(build));
    build.stream_count = highest + 1;
    if (voice_separate(pitches->events, pitches->values, notes->count, stream, notes->map, standard_meter_segmentation_beat_grid,
                       NULL, NULL, &build.assignment) != VOICE_SEPARATION_OK) {
        free(stream);
        return CHAIN_INPUT_ERR_SEGMENTATION;
    }
    ChainInputStatus status = standard_texture_input_init(&build.input, notes->map) == STANDARD_TEXTURE_ANALYSIS_OK
                                  ? CHAIN_INPUT_OK
                                  : CHAIN_INPUT_ERR_ALLOC;
    for (size_t i = 0; i < notes->count && status == CHAIN_INPUT_OK; i++) {
        AnalysisNote note = notes->notes[i];
        const VoiceAssignmentEntry *entry = &build.assignment.entries[i];
        note.track = stream[i];
        note.voice = entry->voice;
        note.voice_confidence = entry->confidence;
        if (standard_texture_input_add_analysis_note(&build.input, &note) != STANDARD_TEXTURE_ANALYSIS_OK) {
            status = CHAIN_INPUT_ERR_BAD_NOTES;
        }
    }
    free(stream);
    if (status != CHAIN_INPUT_OK) {
        chain_texture_input_destroy(&build);
        return status;
    }
    *out = build;
    return CHAIN_INPUT_OK;
}

/* --- the voice grid --- */

void chain_voice_grid_destroy(ChainVoiceGrid *grid) {
    if (grid == NULL) {
        return;
    }
    voice_assignment_destroy(&grid->assignment);
    free(grid->cells);
    free(grid->segment_ids);
    memset(grid, 0, sizeof(*grid));
}

ChainInputStatus chain_build_voice_grid(const AnalysisNoteSet *notes, const ChainPitches *pitches, const ChainSegments *segments,
                                        size_t count, ChainVoiceGrid *out) {
    if (notes == NULL || pitches == NULL || segments == NULL || out == NULL) {
        return CHAIN_INPUT_ERR_NULL_ARGUMENT;
    }
    if (!analysis_note_set_valid(notes) || notes->count == 0 || pitches->count != notes->count || count == 0 ||
        count > segments->count) {
        return CHAIN_INPUT_ERR_BAD_NOTES;
    }
    ChainVoiceGrid built;
    memset(&built, 0, sizeof(built));
    if (voice_separate(pitches->events, pitches->values, notes->count, NULL, notes->map, standard_meter_segmentation_beat_grid, NULL,
                       NULL, &built.assignment) != VOICE_SEPARATION_OK) {
        return CHAIN_INPUT_ERR_SEGMENTATION;
    }
    size_t voices = 0;
    for (size_t s = 0; s < built.assignment.stream_count; s++) {
        voices = built.assignment.streams[s].voice_count > voices ? built.assignment.streams[s].voice_count : voices;
    }
    if (voices == 0) {
        chain_voice_grid_destroy(&built);
        return CHAIN_INPUT_ERR_BAD_NOTES;
    }
    built.cells = calloc(count * voices, sizeof(AnalysisVoiceCell));
    built.segment_ids = calloc(count, sizeof(char *));
    if (built.cells == NULL || built.segment_ids == NULL) {
        chain_voice_grid_destroy(&built);
        return CHAIN_INPUT_ERR_ALLOC;
    }
    for (size_t s = 0; s < count; s++) {
        built.segment_ids[s] = segments->ids[s];
        const TimeSpan *window = sonority_time_window(segments->sonorities.items[s].sonority);
        Rational start;
        if (!rational_of(time_span_start(window), &start)) {
            chain_voice_grid_destroy(&built);
            return CHAIN_INPUT_ERR_SEGMENTATION;
        }
        for (size_t i = 0; i < notes->count; i++) {
            const AnalysisNote *note = &notes->notes[i];
            Rational on;
            Rational off;
            if (!rational_of(time_span_start(note->span), &on) || !rational_of(time_span_end(note->span), &off)) {
                chain_voice_grid_destroy(&built);
                return CHAIN_INPUT_ERR_SEGMENTATION;
            }
            if (!(rational_cmp(on, start) <= 0 && rational_cmp(start, off) < 0)) {
                continue;
            }
            size_t voice = built.assignment.entries[i].voice;
            if (voice >= voices || built.cells[s * voices + voice].present) {
                continue; /* an extra note in the same voice: the first stands */
            }
            AnalysisVoiceCell *cell = &built.cells[s * voices + voice];
            cell->present = true;
            cell->pitch = pitches->values[i];
            cell->spelling = note->spelling;
            cell->note_id = note->note_id;
            cell->attacked = rational_cmp(on, start) == 0;
            cell->confidence = built.assignment.entries[i].confidence;
        }
    }
    built.grid.segment_count = count;
    built.grid.voice_count = voices;
    built.grid.segment_ids = built.segment_ids;
    built.grid.cells = built.cells;
    *out = built;
    return CHAIN_INPUT_OK;
}

/* --- the melody line --- */

void chain_melody_line_destroy(ChainMelodyLine *melody) {
    if (melody == NULL) {
        return;
    }
    standard_counterpoint_melody_line_destroy(&melody->line);
    free(melody->segment);
    memset(melody, 0, sizeof(*melody));
}

static BeatStrength member_attack_strength(const CommonPracticeSegment *segment, const char *note_id, bool *found) {
    for (size_t j = 0; j < segment->member_count; j++) {
        const CommonPracticeMember *member = &segment->members[j];
        if (member->note_id != NULL && strcmp(member->note_id, note_id) == 0 && !member->held) {
            *found = true;
            return member->strength;
        }
    }
    *found = false;
    return BEAT_STRENGTH_WEAK;
}

ChainInputStatus chain_build_melody_line(const ChainSegments *segments, size_t count, const OuterVoiceEntry *entries,
                                         const CommonPracticeWeightedResult *results, int tonic_pitch_class,
                                         CommonPracticeKeyMode mode, ChainMelodyLine *out) {
    if (segments == NULL || entries == NULL || results == NULL || out == NULL) {
        return CHAIN_INPUT_ERR_NULL_ARGUMENT;
    }
    if (segments->segments == NULL || count == 0 || count > segments->count) {
        return CHAIN_INPUT_ERR_BAD_NOTES;
    }
    ChainMelodyLine built;
    memset(&built, 0, sizeof(built));
    standard_counterpoint_melody_line_init(&built.line);
    built.segment = calloc(count + 1, sizeof(size_t));
    if (built.segment == NULL) {
        return CHAIN_INPUT_ERR_ALLOC;
    }
    const char *previous_id = NULL;
    for (size_t s = 0; s < count; s++) {
        const OuterVoiceEntry *entry = &entries[s];
        if (!entry->present) {
            if (previous_id != NULL || built.line.count == 0) {
                built.segment[built.line.count] = s;
                if (standard_counterpoint_melody_line_add_rest(&built.line) != STANDARD_COUNTERPOINT_MELODY_OK) {
                    chain_melody_line_destroy(&built);
                    return CHAIN_INPUT_ERR_BAD_NOTES;
                }
            }
            previous_id = NULL;
            continue;
        }
        const char *id = entity_id_value(entry->note_id);
        if (previous_id != NULL && strcmp(previous_id, id) == 0) {
            continue; /* the same note, held into this segment */
        }
        previous_id = id;
        StandardCounterpointMelodyNote note;
        memset(&note, 0, sizeof(note));
        note.present = true;
        note.note_id = id;
        note.pitch = entry->pitch;
        note.confidence = 1.0;
        long midi = pitch_numeric_value_position_numerator(entry->pitch);
        if (!parse_degree(common_practice_scale_degree_label((int)(((midi % 12) + 12) % 12), tonic_pitch_class, mode), &note.degree,
                          &note.alteration)) {
            chain_melody_line_destroy(&built);
            return CHAIN_INPUT_ERR_BAD_NOTES;
        }
        bool has_strength = false;
        note.strength = member_attack_strength(&segments->segments[s], id, &has_strength);
        note.has_strength = has_strength;

        note.nct = STANDARD_COUNTERPOINT_NCT_UNCLASSIFIED;
        const CommonPracticeWeightedResult *result = &results[s];
        if (result->matched) {
            note.nct = STANDARD_COUNTERPOINT_NCT_CHORD_TONE;
            for (size_t k = 0; k < result->leftover_count; k++) {
                if (strcmp(result->leftover[k].note_id, id) == 0) {
                    if (result->leftover[k].classified) {
                        note.nct = STANDARD_COUNTERPOINT_NCT_NON_CHORD_TONE;
                        note.nct_type = result->leftover[k].tone;
                    } else {
                        note.nct = STANDARD_COUNTERPOINT_NCT_UNCLASSIFIED;
                    }
                }
            }
        }
        char ref[160];
        snprintf(ref, sizeof(ref), "%s/%s", segments->ids[s], id);
        note.nct_ref = ref;
        built.segment[built.line.count] = s;
        if (standard_counterpoint_melody_line_add_note(&built.line, &note) != STANDARD_COUNTERPOINT_MELODY_OK) {
            chain_melody_line_destroy(&built);
            return CHAIN_INPUT_ERR_BAD_NOTES;
        }
    }
    *out = built;
    return CHAIN_INPUT_OK;
}

/* --- the galant segments --- */

void chain_galant_segments_destroy(ChainGalantSegments *segments) {
    if (segments == NULL) {
        return;
    }
    galant_segments_destroy(&segments->list);
    for (size_t i = 0; i < segments->span_count; i++) {
        time_span_destroy(segments->spans[i]);
    }
    free(segments->spans);
    memset(segments, 0, sizeof(*segments));
}

static const TimeSpan *galant_span(ChainGalantSegments *build, const MusicalTime *start, const MusicalTime *end) {
    if (build->span_count == build->span_capacity) {
        size_t capacity = build->span_capacity == 0 ? 256 : build->span_capacity * 2;
        TimeSpan **grown = realloc(build->spans, capacity * sizeof(TimeSpan *));
        if (grown == NULL) {
            return NULL;
        }
        build->spans = grown;
        build->span_capacity = capacity;
    }
    TimeSpan *span = NULL;
    if (time_span_create(start, end, TIME_SPAN_BOUNDARY_LEFT_CLOSED, &span) != TIME_SPAN_OK) {
        return NULL;
    }
    build->spans[build->span_count++] = span;
    return span;
}

static GalantPitchDegree degree_of_offset(int semitones) {
    static const GalantPitchDegree TABLE[12] = {{1, 0}, {2, -1}, {2, 0}, {3, -1}, {3, 0}, {4, 0},
                                                {4, 1}, {5, 0},  {6, -1}, {6, 0}, {7, -1}, {7, 0}};
    return TABLE[((semitones % 12) + 12) % 12];
}

/* The length of a beat at `time`, in whole notes (a compound meter's beat is its dotted unit); a quarter when it cannot be read. */
static void beat_length_at(const MeterMap *map, const MusicalTime *time, long *numerator, unsigned long *denominator) {
    const MeterMapRegion *region = NULL;
    *numerator = 1;
    *denominator = 4;
    if (meter_map_region_at(map, time, &region) != METER_MAP_OK) {
        return;
    }
    const TimeSignature *signature = meter_map_region_time_signature(region);
    Duration *unit = NULL;
    if (signature == NULL || time_signature_unit_duration(signature, &unit) != DURATION_OK) {
        return;
    }
    long n = duration_numerator(unit);
    unsigned long d = duration_denominator(unit);
    duration_destroy(unit);
    unsigned top = time_signature_top_number(signature);
    if ((top == 6 || top == 9 || top == 12) && time_signature_bottom_number(signature) >= 8) {
        n *= 3;
    }
    *numerator = n;
    *denominator = d;
}

static int pitch_class_of(const OuterVoiceEntry *entry) {
    long position = pitch_numeric_value_position_numerator(entry->pitch);
    return (int)(((position % 12) + 12) % 12);
}

static double clamp01(double x) {
    return x < 0.0 ? 0.0 : (x > 1.0 ? 1.0 : x);
}

static bool same_harmony(const GalantSegment *a, const GalantSegment *b) {
    return strcmp(a->key_id, b->key_id) == 0 && a->has_bass == b->has_bass && a->has_soprano == b->has_soprano &&
           (!a->has_bass || galant_degree_equal(a->bass, b->bass)) &&
           (!a->has_soprano || galant_degree_equal(a->soprano, b->soprano)) && a->has_numeral == b->has_numeral &&
           (!a->has_numeral || (galant_degree_equal(a->root, b->root) && a->inversion == b->inversion));
}

ChainInputStatus chain_build_galant_segments(const ChainSegments *segments, size_t count, const MeterMap *map,
                                             const OuterVoiceLines *lines, int home_key_index,
                                             const StandardTonalityChordInput *chords, const StandardTonalityRomanResult *romans,
                                             ChainGalantSegments *out) {
    if (segments == NULL || map == NULL || lines == NULL || chords == NULL || romans == NULL || out == NULL) {
        return CHAIN_INPUT_ERR_NULL_ARGUMENT;
    }
    int home_pc = common_practice_key_index_tonic(home_key_index);
    if (count == 0 || count > segments->count || count > lines->count || home_pc < 0) {
        return CHAIN_INPUT_ERR_BAD_NOTES;
    }
    ChainGalantSegments build;
    memset(&build, 0, sizeof(build));
    galant_segments_init(&build.list);
    ChainInputStatus status = CHAIN_INPUT_OK;
    for (size_t s = 0; s < count && status == CHAIN_INPUT_OK; s++) {
        const TimeSpan *window = sonority_time_window(segments->sonorities.items[s].sonority);
        int tonic_pc = chords[s].tonic_pitch_class;
        bool minor = chords[s].mode == STANDARD_TONALITY_ROMAN_KEY_MINOR;
        GalantSegment segment;
        memset(&segment, 0, sizeof(segment));
        AnalysisSegment analysis = {segments->ids[s], window, NULL, chain_attack_strength(map, time_span_start(window))};
        if (!galant_segment_set_analysis_segment(&segment, &analysis)) {
            status = CHAIN_INPUT_ERR_BAD_NOTES;
            break;
        }
        beat_length_at(map, time_span_start(window), &segment.beat_numerator, &segment.beat_denominator);
        snprintf(segment.key_id, sizeof(segment.key_id), "key-%d%s", tonic_pc, minor ? "m" : "");
        segment.key = degree_of_offset(tonic_pc - home_pc);
        if (lines->bass[s].present) {
            segment.has_bass = true;
            segment.bass = degree_of_offset(pitch_class_of(&lines->bass[s]) - tonic_pc);
        }
        if (lines->soprano[s].present) {
            segment.has_soprano = true;
            segment.soprano = degree_of_offset(pitch_class_of(&lines->soprano[s]) - tonic_pc);
        }
        segment.uncertainty = 0.5;
        if (romans[s].has_numeral) {
            const StandardTonalityRomanClaim *claim = &romans[s].claim;
            int lowered = minor && (claim->degree == 3 || claim->degree == 6 || claim->degree == 7) &&
                                  !romans[s].reading.raised_leading_tone
                              ? -1
                              : 0;
            segment.has_numeral = true;
            segment.root = (GalantPitchDegree){claim->degree, lowered};
            if (claim->applied) {
                int target_lowered = minor && (claim->applied_target_degree == 3 || claim->applied_target_degree == 6 ||
                                               claim->applied_target_degree == 7)
                                         ? -1
                                         : 0;
                segment.root = galant_degree_plus((GalantPitchDegree){claim->applied_target_degree, target_lowered},
                                                  (GalantPitchDegree){claim->degree, 0});
                segment.applied_to = claim->applied_target_degree;
            }
            segment.quality = claim->quality;
            segment.seventh = claim->seventh;
            segment.inversion = claim->inversion;
            segment.uncertainty = clamp01(romans[s].uncertainty);
        }
        /* a held chord continues the segment before it */
        if (build.list.count > 0) {
            GalantSegment *last = &build.list.items[build.list.count - 1];
            if (same_harmony(last, &segment)) {
                const TimeSpan *merged = galant_span(&build, time_span_start(last->span), time_span_end(window));
                if (merged == NULL) {
                    status = CHAIN_INPUT_ERR_ALLOC;
                    break;
                }
                last->span = merged;
                last->uncertainty = last->uncertainty > segment.uncertainty ? last->uncertainty : segment.uncertainty;
                continue;
            }
        }
        if (galant_segments_add(&build.list, &segment) != GALANT_ANALYSIS_OK) {
            status = CHAIN_INPUT_ERR_BAD_NOTES;
        }
    }
    if (status != CHAIN_INPUT_OK) {
        chain_galant_segments_destroy(&build);
        return status;
    }
    *out = build;
    return CHAIN_INPUT_OK;
}
