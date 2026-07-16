#include "export/score_quantize.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "identity/entity_id.h"
#include "time/duration.h"
#include "time/exact_rational.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"
#include "time/time_signature.h"
#include "time/time_span.h"

ScoreQuantizeOptions score_quantize_options_default(void) {
    ScoreQuantizeOptions options;
    options.grid_denominator = 32;
    options.allow_triplets = true;
    options.gap_absorb_numerator = 1;
    options.gap_absorb_denominator = 4;
    return options;
}

void score_quantized_destroy(ScoreQuantized *quantized) {
    if (quantized == NULL) {
        return;
    }
    free(quantized->start_num);
    free(quantized->start_den);
    free(quantized->end_num);
    free(quantized->end_den);
    memset(quantized, 0, sizeof(*quantized));
}

/* --- the beat grid --- */

typedef struct {
    ExactRational start;
    ExactRational end; /* the next beat, or the barline when the measure is short */
    ExactRational len; /* the nominal beat length */
} Span;

typedef struct {
    ExactRational start;
    ExactRational end;
    Span beat;
    ExactRational step; /* the grid step chosen for the beat of the onset */
    ExactRational snapped_start;
    ExactRational snapped_end;
    size_t part;
    int staff;
    const char *voice; /* the VoiceReference's target id, NULL for none */
    size_t group;      /* the voice group: part, staff and voice */
} Item;

typedef struct {
    ExactRational start;
    ExactRational step;
} BeatGrid;

typedef struct {
    ExactRational beat_start;
    size_t item;
} BeatKey;

static int compare_keys(const void *left, const void *right) {
    const BeatKey *a = (const BeatKey *)left;
    const BeatKey *b = (const BeatKey *)right;
    int order = er_cmp(a->beat_start, b->beat_start);
    if (order != 0) {
        return order;
    }
    return a->item < b->item ? -1 : (a->item > b->item ? 1 : 0);
}

static int compare_grids(const void *key, const void *entry) {
    return er_cmp(*(const ExactRational *)key, ((const BeatGrid *)entry)->start);
}

/* |a - b| */
static int er_distance(ExactRational a, ExactRational b, ExactRational *out) {
    return er_cmp(a, b) >= 0 ? er_sub(a, b, out) : er_sub(b, a, out);
}

/* The beat that holds time t: a simple meter's beat is its bottom unit, a compound meter's (6, 9 or 12 over 8 or 16)
   is three of them. A short measure's last beat ends at its barline. */
static ScoreQuantizeResult locate_beat(const MeterMap *meter, ExactRational t, Span *beat) {
    MusicalTime *time = er_to_time(t);
    if (time == NULL) {
        return SCORE_QUANTIZE_ERR_NOT_REPRESENTABLE;
    }
    int index = 0;
    MeterMapResult status = meter_map_measure_index_at(meter, time, &index);
    musical_time_destroy(time);
    TimeSpan *span = NULL;
    if (status != METER_MAP_OK || meter_map_measure_span(meter, index, &span) != METER_MAP_OK) {
        return SCORE_QUANTIZE_ERR_NOT_REPRESENTABLE;
    }
    ExactRational measure_start = er_of_time(time_span_start(span));
    ExactRational measure_end = er_of_time(time_span_end(span));
    time_span_destroy(span);
    const MeterMapRegion *region = NULL;
    if (meter_map_region_of_measure(meter, index, &region) != METER_MAP_OK) {
        return SCORE_QUANTIZE_ERR_NOT_REPRESENTABLE;
    }
    const TimeSignature *signature = meter_map_region_time_signature(region);
    unsigned top = time_signature_top_number(signature);
    unsigned bottom = time_signature_bottom_number(signature);
    ExactRational len = (top > 3 && top % 3 == 0 && bottom >= 8) ? er_make(3, bottom) : er_make(1, bottom);

    ExactRational offset;
    ExactRational beats;
    ExactRational whole;
    ExactRational beat_end;
    if (er_sub(t, measure_start, &offset) != 0 || er_mul(offset, er_make(len.d, len.n), &beats) != 0 ||
        er_mul(len, er_make(beats.n / beats.d, 1), &whole) != 0 || er_add(measure_start, whole, &beat->start) != 0 ||
        er_add(beat->start, len, &beat_end) != 0) {
        return SCORE_QUANTIZE_ERR_NOT_REPRESENTABLE;
    }
    beat->end = er_cmp(beat_end, measure_end) > 0 ? measure_end : beat_end;
    beat->len = len;
    return SCORE_QUANTIZE_OK;
}

/* The point of the grid (beat start plus whole steps, and the beat end) nearest to t, a tie rounding up. t lies in
   [beat.start, beat.end]. */
static int snap(ExactRational t, const Span *beat, ExactRational step, ExactRational *point) {
    ExactRational offset;
    ExactRational steps;
    ExactRational whole;
    if (er_sub(t, beat->start, &offset) != 0 || er_mul(offset, er_make(step.d, step.n), &steps) != 0) {
        return -1;
    }
    unsigned long long count = steps.n / steps.d;
    if (steps.n % steps.d >= steps.d - steps.n % steps.d) {
        count++;
    }
    if (er_mul(step, er_make(count, 1), &whole) != 0 || er_add(beat->start, whole, point) != 0) {
        return -1;
    }
    if (er_cmp(*point, beat->end) > 0) {
        *point = beat->end;
    }
    return 0;
}

/* The grid step for the onsets of one beat: the candidate with the lowest total snap error, the straight grid on a tie. */
static ScoreQuantizeResult choose_step(const Item *items, const BeatKey *keys, size_t from, size_t to,
                                       const ScoreQuantizeOptions *options, ExactRational *step) {
    const Span *beat = &items[keys[from].item].beat;
    ExactRational candidates[3];
    size_t candidate_count = 0;
    candidates[candidate_count++] = er_make(1, options->grid_denominator);
    if (options->allow_triplets) {
        ExactRational third;
        ExactRational sixth;
        if (er_mul(beat->len, er_make(1, 3), &third) != 0 || er_mul(beat->len, er_make(1, 6), &sixth) != 0) {
            return SCORE_QUANTIZE_ERR_NOT_REPRESENTABLE;
        }
        candidates[candidate_count++] = third;
        candidates[candidate_count++] = sixth;
    }
    ExactRational best_error = {0, 1};
    for (size_t c = 0; c < candidate_count; c++) {
        ExactRational total = {0, 1};
        for (size_t k = from; k < to; k++) {
            const Item *item = &items[keys[k].item];
            ExactRational point;
            ExactRational error;
            if (snap(item->start, beat, candidates[c], &point) != 0 || er_distance(item->start, point, &error) != 0 ||
                er_add(total, error, &total) != 0) {
                return SCORE_QUANTIZE_ERR_NOT_REPRESENTABLE;
            }
        }
        if (c == 0 || er_cmp(total, best_error) < 0) {
            best_error = total;
            *step = candidates[c];
        }
    }
    return SCORE_QUANTIZE_OK;
}

/* --- quantize --- */

static ScoreQuantizeResult fill_items(Item *items, NoteEvent *const *events, size_t count, const size_t *part_index,
                                      const int *staff, const MeterMap *meter) {
    for (size_t i = 0; i < count; i++) {
        items[i].part = part_index != NULL ? part_index[i] : 0;
        items[i].staff = staff != NULL ? staff[i] : 1;
        const VoiceReference *voice = note_event_optional_voice(events[i]);
        items[i].voice = voice != NULL ? entity_id_value(voice_reference_target_id(voice)) : NULL;
        const MusicalTime *position = temporal_anchor_position(note_event_temporal_anchor(events[i]));
        const Duration *duration = note_event_duration(events[i]);
        if (musical_time_numerator(position) < 0 || duration_numerator(duration) <= 0) {
            return SCORE_QUANTIZE_ERR_NOT_REPRESENTABLE;
        }
        items[i].start = er_of_time(position);
        if (er_add(items[i].start, er_of_duration(duration), &items[i].end) != 0) {
            return SCORE_QUANTIZE_ERR_NOT_REPRESENTABLE;
        }
        ScoreQuantizeResult located = locate_beat(meter, items[i].start, &items[i].beat);
        if (located != SCORE_QUANTIZE_OK) {
            return located;
        }
    }
    return SCORE_QUANTIZE_OK;
}

/* Picks the grid of every beat that has an onset: grids[] ascending by beat start, *grid_count of them. */
static ScoreQuantizeResult choose_grids(Item *items, size_t count, const ScoreQuantizeOptions *options, BeatGrid *grids,
                                        size_t *grid_count) {
    BeatKey *keys = (BeatKey *)malloc(count * sizeof(BeatKey));
    if (keys == NULL) {
        return SCORE_QUANTIZE_ERR_ALLOC;
    }
    for (size_t i = 0; i < count; i++) {
        keys[i].beat_start = items[i].beat.start;
        keys[i].item = i;
    }
    qsort(keys, count, sizeof(BeatKey), compare_keys);
    ScoreQuantizeResult result = SCORE_QUANTIZE_OK;
    size_t from = 0;
    while (from < count && result == SCORE_QUANTIZE_OK) {
        size_t to = from + 1;
        while (to < count && er_cmp(keys[to].beat_start, keys[from].beat_start) == 0) {
            to++;
        }
        ExactRational step;
        result = choose_step(items, keys, from, to, options, &step);
        if (result == SCORE_QUANTIZE_OK) {
            grids[*grid_count].start = keys[from].beat_start;
            grids[*grid_count].step = step;
            (*grid_count)++;
            for (size_t k = from; k < to; k++) {
                items[keys[k].item].step = step;
            }
        }
        from = to;
    }
    free(keys);
    return result;
}

/* Snaps every onset and end to its beat's grid. An end in a beat with no onset uses the straight grid. A note is at
   least one step long. */
static ScoreQuantizeResult snap_items(Item *items, size_t count, const MeterMap *meter, const BeatGrid *grids,
                                      size_t grid_count, const ScoreQuantizeOptions *options) {
    for (size_t i = 0; i < count; i++) {
        Item *item = &items[i];
        if (snap(item->start, &item->beat, item->step, &item->snapped_start) != 0) {
            return SCORE_QUANTIZE_ERR_NOT_REPRESENTABLE;
        }
        Span end_beat;
        ScoreQuantizeResult located = locate_beat(meter, item->end, &end_beat);
        if (located != SCORE_QUANTIZE_OK) {
            return located;
        }
        const BeatGrid *grid = (const BeatGrid *)bsearch(&end_beat.start, grids, grid_count, sizeof(BeatGrid),
                                                         compare_grids);
        ExactRational end_step = grid != NULL ? grid->step : er_make(1, options->grid_denominator);
        ExactRational shortest;
        if (snap(item->end, &end_beat, end_step, &item->snapped_end) != 0 ||
            er_add(item->snapped_start, item->step, &shortest) != 0) {
            return SCORE_QUANTIZE_ERR_NOT_REPRESENTABLE;
        }
        if (er_cmp(item->snapped_end, shortest) < 0) {
            item->snapped_end = shortest;
        }
    }
    return SCORE_QUANTIZE_OK;
}

/* --- legato --- */

static bool same_group(const Item *a, const Item *b) {
    return a->part == b->part && a->staff == b->staff &&
           ((a->voice == NULL && b->voice == NULL) ||
            (a->voice != NULL && b->voice != NULL && strcmp(a->voice, b->voice) == 0));
}

/* Numbers the voice groups: items of one part, staff and voice share a number. */
static void assign_groups(Item *items, size_t count) {
    size_t groups = 0;
    for (size_t i = 0; i < count; i++) {
        size_t found = groups;
        for (size_t g = 0; g < i && found == groups; g++) {
            if (items[g].group < groups && same_group(&items[g], &items[i])) {
                found = items[g].group;
            }
        }
        items[i].group = found;
        if (found == groups) {
            groups++;
        }
    }
}

typedef struct {
    size_t group;
    ExactRational start;
    size_t item;
} VoiceKey;

static int compare_voice_keys(const void *left, const void *right) {
    const VoiceKey *a = (const VoiceKey *)left;
    const VoiceKey *b = (const VoiceKey *)right;
    if (a->group != b->group) {
        return a->group < b->group ? -1 : 1;
    }
    int order = er_cmp(a->start, b->start);
    if (order != 0) {
        return order;
    }
    return a->item < b->item ? -1 : (a->item > b->item ? 1 : 0);
}

/* True when the end of a note should move to the next onset of its voice: the gap is positive, less than a beat and at
   most gap_absorb times the note's own notated length. */
static int absorbs(const Item *item, ExactRational next, const ScoreQuantizeOptions *options, bool *absorb) {
    *absorb = false;
    if (er_cmp(next, item->snapped_end) <= 0) {
        return 0;
    }
    ExactRational gap;
    ExactRational length;
    ExactRational limit;
    if (er_sub(next, item->snapped_end, &gap) != 0 || er_sub(item->snapped_end, item->snapped_start, &length) != 0 ||
        er_mul(length, er_make(options->gap_absorb_numerator, options->gap_absorb_denominator), &limit) != 0) {
        return -1;
    }
    *absorb = er_cmp(gap, item->beat.len) < 0 && er_cmp(gap, limit) <= 0;
    return 0;
}

/* Moves the end of each note over a small gap to the next onset in its voice group. The next onset is the earliest
   onset after the note's own, so the notes of a chord all look to the same one. */
static ScoreQuantizeResult absorb_gaps(Item *items, size_t count, const ScoreQuantizeOptions *options) {
    if (count == 0) {
        return SCORE_QUANTIZE_OK;
    }
    assign_groups(items, count);
    VoiceKey *keys = (VoiceKey *)malloc(count * sizeof(VoiceKey));
    ExactRational *ends = (ExactRational *)malloc(count * sizeof(ExactRational));
    if (keys == NULL || ends == NULL) {
        free(keys);
        free(ends);
        return SCORE_QUANTIZE_ERR_ALLOC;
    }
    for (size_t i = 0; i < count; i++) {
        keys[i].group = items[i].group;
        keys[i].start = items[i].snapped_start;
        keys[i].item = i;
        ends[i] = items[i].snapped_end;
    }
    qsort(keys, count, sizeof(VoiceKey), compare_voice_keys);
    ScoreQuantizeResult result = SCORE_QUANTIZE_OK;
    for (size_t k = count; k-- > 0 && result == SCORE_QUANTIZE_OK;) {
        size_t next = k + 1;
        while (next < count && keys[next].group == keys[k].group && er_cmp(keys[next].start, keys[k].start) == 0) {
            next++;
        }
        if (next >= count || keys[next].group != keys[k].group) {
            continue;
        }
        Item *item = &items[keys[k].item];
        bool absorb = false;
        if (absorbs(item, keys[next].start, options, &absorb) != 0) {
            result = SCORE_QUANTIZE_ERR_NOT_REPRESENTABLE;
        } else if (absorb) {
            ends[keys[k].item] = keys[next].start;
        }
    }
    for (size_t i = 0; i < count && result == SCORE_QUANTIZE_OK; i++) {
        items[i].snapped_end = ends[i];
    }
    free(keys);
    free(ends);
    return result;
}

static ScoreQuantizeResult store_items(const Item *items, size_t count, ScoreQuantized *out) {
    out->start_num = (unsigned long long *)malloc((count > 0 ? count : 1) * sizeof(unsigned long long));
    out->start_den = (unsigned long long *)malloc((count > 0 ? count : 1) * sizeof(unsigned long long));
    out->end_num = (unsigned long long *)malloc((count > 0 ? count : 1) * sizeof(unsigned long long));
    out->end_den = (unsigned long long *)malloc((count > 0 ? count : 1) * sizeof(unsigned long long));
    if (out->start_num == NULL || out->start_den == NULL || out->end_num == NULL || out->end_den == NULL) {
        return SCORE_QUANTIZE_ERR_ALLOC;
    }
    for (size_t i = 0; i < count; i++) {
        out->start_num[i] = items[i].snapped_start.n;
        out->start_den[i] = items[i].snapped_start.d;
        out->end_num[i] = items[i].snapped_end.n;
        out->end_den[i] = items[i].snapped_end.d;
    }
    out->count = count;
    return SCORE_QUANTIZE_OK;
}

ScoreQuantizeResult score_quantize(NoteEvent *const *events, size_t count, const size_t *part_index, const int *staff,
                                   const MeterMap *meter_map, const ScoreQuantizeOptions *options,
                                   ScoreQuantized *out) {
    if (out == NULL || (events == NULL && count > 0)) {
        return SCORE_QUANTIZE_ERR_NULL_ARG;
    }
    memset(out, 0, sizeof(*out));
    ScoreQuantizeOptions defaults = score_quantize_options_default();
    if (options == NULL) {
        options = &defaults;
    }
    if (options->grid_denominator == 0 || options->gap_absorb_denominator == 0) {
        return SCORE_QUANTIZE_ERR_NOT_REPRESENTABLE;
    }

    MeterMap *own_meter = NULL;
    if (meter_map == NULL) {
        EntityID *id = NULL;
        if (entity_id_create("score-quantize-meter-map", &id) != ENTITY_ID_OK) {
            return SCORE_QUANTIZE_ERR_ALLOC;
        }
        if (meter_map_create_defaulted(id, &own_meter) != METER_MAP_OK) {
            entity_id_destroy(id);
            return SCORE_QUANTIZE_ERR_ALLOC;
        }
        meter_map = own_meter;
    }

    Item *items = (Item *)calloc(count > 0 ? count : 1, sizeof(Item));
    BeatGrid *grids = (BeatGrid *)calloc(count > 0 ? count : 1, sizeof(BeatGrid));
    size_t grid_count = 0;
    ScoreQuantizeResult result = items != NULL && grids != NULL ? SCORE_QUANTIZE_OK : SCORE_QUANTIZE_ERR_ALLOC;
    if (result == SCORE_QUANTIZE_OK) {
        result = fill_items(items, events, count, part_index, staff, meter_map);
    }
    if (result == SCORE_QUANTIZE_OK && count > 0) {
        result = choose_grids(items, count, options, grids, &grid_count);
    }
    if (result == SCORE_QUANTIZE_OK) {
        result = snap_items(items, count, meter_map, grids, grid_count, options);
    }
    if (result == SCORE_QUANTIZE_OK) {
        result = absorb_gaps(items, count, options);
    }
    if (result == SCORE_QUANTIZE_OK) {
        result = store_items(items, count, out);
    }
    if (result != SCORE_QUANTIZE_OK) {
        score_quantized_destroy(out);
    }
    free(items);
    free(grids);
    meter_map_destroy(own_meter);
    return result;
}
