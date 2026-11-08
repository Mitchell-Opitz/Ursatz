#include "export/score_quantize.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "core/rational.h"
#include "identity/entity_id.h"
#include "time/duration.h"
#include "time/time_rational.h"
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

/* --- the beat grid: one cell per beat, or two when a half beat fits a different grid better --- */

typedef struct {
    Rational start;
    Rational end; /* the next beat, or the barline when the measure is short */
    Rational len; /* the nominal beat length */
} Span;

typedef struct {
    Rational start;
    Rational end;
    Span beat;
    Span cell;     /* the cell of the onset: its beat, or the half of it the grid was chosen for */
    Rational step; /* the grid step chosen for that cell */
    Rational snapped_start;
    Rational snapped_end;
    size_t part;
    int staff;
    const char *voice; /* the VoiceReference's target id, NULL for none */
    size_t group;      /* the voice group: part, staff and voice */
} Item;

/* A stretch of one beat with its own grid: the whole beat, or one half of it. The cells are ascending and never overlap. */
typedef struct {
    Rational start;
    Rational length;
    Rational step;
} Cell;

typedef struct {
    Rational beat_start;
    size_t item;
} BeatKey;

static int compare_keys(const void *left, const void *right) {
    const BeatKey *a = (const BeatKey *)left;
    const BeatKey *b = (const BeatKey *)right;
    int order = rational_cmp(a->beat_start, b->beat_start);
    if (order != 0) {
        return order;
    }
    return a->item < b->item ? -1 : (a->item > b->item ? 1 : 0);
}

/* The cell that holds time t (start <= t < start + length), or NULL when no onset made a cell there. */
static const Cell *find_cell(const Cell *cells, size_t cell_count, Rational t) {
    size_t low = 0;
    size_t high = cell_count;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        if (rational_cmp(cells[middle].start, t) <= 0) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }
    if (low == 0) {
        return NULL;
    }
    const Cell *cell = &cells[low - 1];
    Rational end;
    if (rational_add(cell->start, cell->length, &end) != RATIONAL_OK || rational_cmp(t, end) >= 0) {
        return NULL;
    }
    return cell;
}

/* |a - b| */
static int rational_distance(Rational a, Rational b, Rational *out) {
    return (rational_cmp(a, b) >= 0 ? rational_sub(a, b, out) : rational_sub(b, a, out)) != RATIONAL_OK;
}

/* The beat that holds time t: a simple meter's beat is its bottom unit, a compound meter's (6, 9 or 12 over 8 or 16)
   is three of them. A short measure's last beat ends at its barline. */
static ScoreQuantizeResult locate_beat(const MeterMap *meter, Rational t, Span *beat) {
    MusicalTime *time = NULL;
    if (musical_time_from_rational(t, &time) != TIME_RATIONAL_OK) {
        return SCORE_QUANTIZE_ERR_NOT_REPRESENTABLE;
    }
    int index = 0;
    MeterMapResult status = meter_map_measure_index_at(meter, time, &index);
    musical_time_destroy(time);
    TimeSpan *span = NULL;
    if (status != METER_MAP_OK || meter_map_measure_span(meter, index, &span) != METER_MAP_OK) {
        return SCORE_QUANTIZE_ERR_NOT_REPRESENTABLE;
    }
    Rational measure_start;
    Rational measure_end;
    bool converted = rational_of_musical_time(time_span_start(span), &measure_start) == RATIONAL_OK &&
                     rational_of_musical_time(time_span_end(span), &measure_end) == RATIONAL_OK;
    time_span_destroy(span);
    if (!converted) {
        return SCORE_QUANTIZE_ERR_NOT_REPRESENTABLE;
    }
    const MeterMapRegion *region = NULL;
    if (meter_map_region_of_measure(meter, index, &region) != METER_MAP_OK) {
        return SCORE_QUANTIZE_ERR_NOT_REPRESENTABLE;
    }
    const TimeSignature *signature = meter_map_region_time_signature(region);
    unsigned top = time_signature_top_number(signature);
    unsigned bottom = time_signature_bottom_number(signature);
    Rational len = (top > 3 && top % 3 == 0 && bottom >= 8) ? rational_ratio(3, bottom) : rational_ratio(1, bottom);

    Rational offset;
    Rational beats;
    Rational whole;
    Rational beat_end;
    long long whole_beats = 0;
    if (rational_sub(t, measure_start, &offset) != RATIONAL_OK || rational_div(offset, len, &beats) != RATIONAL_OK ||
        rational_floor(beats, &whole_beats) != RATIONAL_OK || rational_mul_int(len, whole_beats, &whole) != RATIONAL_OK ||
        rational_add(measure_start, whole, &beat->start) != RATIONAL_OK ||
        rational_add(beat->start, len, &beat_end) != RATIONAL_OK) {
        return SCORE_QUANTIZE_ERR_NOT_REPRESENTABLE;
    }
    beat->end = rational_cmp(beat_end, measure_end) > 0 ? measure_end : beat_end;
    beat->len = len;
    return SCORE_QUANTIZE_OK;
}

/* The point of the grid (beat start plus whole steps, and the beat end) nearest to t, a tie rounding up. t lies in
   [beat.start, beat.end]. */
static int snap(Rational t, const Span *beat, Rational step, Rational *point) {
    Rational offset;
    Rational steps;
    Rational whole;
    if (rational_sub(t, beat->start, &offset) != RATIONAL_OK || rational_div(offset, step, &steps) != RATIONAL_OK) {
        return -1;
    }
    long long count = steps.num / steps.den;
    if (steps.num % steps.den >= steps.den - steps.num % steps.den) {
        count++;
    }
    if (rational_mul_int(step, count, &whole) != RATIONAL_OK || rational_add(beat->start, whole, point) != RATIONAL_OK) {
        return -1;
    }
    if (rational_cmp(*point, beat->end) > 0) {
        *point = beat->end;
    }
    return 0;
}

/* The total snap error of the onsets of keys[from..to) that start inside the cell, on a grid of the given step. */
static int cell_error(const Item *items, const BeatKey *keys, size_t from, size_t to, const Span *cell, Rational step,
                      Rational *total) {
    *total = (Rational){0, 1};
    for (size_t k = from; k < to; k++) {
        const Item *item = &items[keys[k].item];
        if (rational_cmp(item->start, cell->start) < 0 || rational_cmp(item->start, cell->end) >= 0) {
            continue;
        }
        Rational point;
        Rational error;
        if (snap(item->start, cell, step, &point) != 0 || rational_distance(item->start, point, &error) != 0 ||
            rational_add(*total, error, total) != RATIONAL_OK) {
            return -1;
        }
    }
    return 0;
}

/* The candidate step with the lowest total snap error in the cell, the first candidate (the straight grid) on a tie. */
static ScoreQuantizeResult best_step(const Item *items, const BeatKey *keys, size_t from, size_t to, const Span *cell,
                                     const Rational *candidates, size_t candidate_count, Rational *step, Rational *best_error) {
    for (size_t c = 0; c < candidate_count; c++) {
        Rational total;
        if (cell_error(items, keys, from, to, cell, candidates[c], &total) != 0) {
            return SCORE_QUANTIZE_ERR_NOT_REPRESENTABLE;
        }
        if (c == 0 || rational_cmp(total, *best_error) < 0) {
            *best_error = total;
            *step = candidates[c];
        }
    }
    return SCORE_QUANTIZE_OK;
}

/* The cells for the onsets of one beat: the whole beat on its best grid (straight, a third or a sixth of the beat), or,
   when triplets are allowed, its two halves each on the best of straight or a third of the half, but only when the two
   halves together snap with a strictly lower error than the whole beat. Writes 1 or 2 cells into out. */
static ScoreQuantizeResult choose_cells(const Item *items, const BeatKey *keys, size_t from, size_t to,
                                        const ScoreQuantizeOptions *options, Cell *out, size_t *out_count) {
    const Span *beat = &items[keys[from].item].beat;
    Rational candidates[3];
    size_t candidate_count = 0;
    Rational straight = rational_ratio(1, options->grid_denominator);
    candidates[candidate_count++] = straight;
    Rational sixth = {0, 1};
    if (options->allow_triplets) {
        Rational third;
        if (rational_mul(beat->len, rational_ratio(1, 3), &third) != RATIONAL_OK || rational_mul(beat->len, rational_ratio(1, 6), &sixth) != RATIONAL_OK) {
            return SCORE_QUANTIZE_ERR_NOT_REPRESENTABLE;
        }
        candidates[candidate_count++] = third;
        candidates[candidate_count++] = sixth;
    }
    Rational whole_step;
    Rational whole_error;
    ScoreQuantizeResult result = best_step(items, keys, from, to, beat, candidates, candidate_count, &whole_step, &whole_error);
    if (result != SCORE_QUANTIZE_OK) {
        return result;
    }
    *out_count = 1;
    out[0].start = beat->start;
    if (rational_sub(beat->end, beat->start, &out[0].length) != RATIONAL_OK) {
        return SCORE_QUANTIZE_ERR_NOT_REPRESENTABLE;
    }
    out[0].step = whole_step;
    if (!options->allow_triplets || rational_cmp(whole_error, (Rational){0, 1}) == 0) {
        return SCORE_QUANTIZE_OK;
    }

    Rational half;
    Rational middle;
    if (rational_mul(beat->len, rational_ratio(1, 2), &half) != RATIONAL_OK || rational_add(beat->start, half, &middle) != RATIONAL_OK) {
        return SCORE_QUANTIZE_ERR_NOT_REPRESENTABLE;
    }
    if (rational_cmp(middle, beat->end) >= 0) {
        return SCORE_QUANTIZE_OK; /* a short measure ends before the half */
    }
    Span first = {beat->start, middle, half};
    Span second = {middle, beat->end, half};
    Rational half_candidates[2] = {straight, sixth};
    Rational first_step;
    Rational second_step;
    Rational first_error;
    Rational second_error;
    Rational halves_error;
    if (best_step(items, keys, from, to, &first, half_candidates, 2, &first_step, &first_error) != SCORE_QUANTIZE_OK ||
        best_step(items, keys, from, to, &second, half_candidates, 2, &second_step, &second_error) != SCORE_QUANTIZE_OK ||
        rational_add(first_error, second_error, &halves_error) != RATIONAL_OK) {
        return SCORE_QUANTIZE_ERR_NOT_REPRESENTABLE;
    }
    if (rational_cmp(halves_error, whole_error) >= 0) {
        return SCORE_QUANTIZE_OK;
    }
    out[0].length = half;
    out[0].step = first_step;
    out[1].start = middle;
    if (rational_sub(beat->end, middle, &out[1].length) != RATIONAL_OK) {
        return SCORE_QUANTIZE_ERR_NOT_REPRESENTABLE;
    }
    out[1].step = second_step;
    *out_count = 2;
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
        Rational length;
        if (rational_of_musical_time(position, &items[i].start) != RATIONAL_OK ||
            rational_of_duration(duration, &length) != RATIONAL_OK ||
            rational_add(items[i].start, length, &items[i].end) != RATIONAL_OK) {
            return SCORE_QUANTIZE_ERR_NOT_REPRESENTABLE;
        }
        ScoreQuantizeResult located = locate_beat(meter, items[i].start, &items[i].beat);
        if (located != SCORE_QUANTIZE_OK) {
            return located;
        }
    }
    return SCORE_QUANTIZE_OK;
}

/* Picks the cells of every beat that has an onset: cells[] ascending by start, *cell_count of them (at most two per beat). */
static ScoreQuantizeResult choose_grids(Item *items, size_t count, const ScoreQuantizeOptions *options, Cell *cells,
                                        size_t *cell_count) {
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
        while (to < count && rational_cmp(keys[to].beat_start, keys[from].beat_start) == 0) {
            to++;
        }
        Cell chosen[2];
        size_t chosen_count = 0;
        result = choose_cells(items, keys, from, to, options, chosen, &chosen_count);
        if (result == SCORE_QUANTIZE_OK) {
            for (size_t c = 0; c < chosen_count; c++) {
                cells[(*cell_count)++] = chosen[c];
            }
            for (size_t k = from; k < to && result == SCORE_QUANTIZE_OK; k++) {
                Item *item = &items[keys[k].item];
                const Cell *cell = find_cell(chosen, chosen_count, item->start);
                Rational end;
                if (cell == NULL || rational_add(cell->start, cell->length, &end) != RATIONAL_OK) {
                    result = SCORE_QUANTIZE_ERR_NOT_REPRESENTABLE;
                    break;
                }
                item->cell.start = cell->start;
                item->cell.end = end;
                item->cell.len = cell->length;
                item->step = cell->step;
            }
        }
        from = to;
    }
    free(keys);
    return result;
}

/* Snaps every onset and end to the grid of its cell. An end in a cell with no onset uses the straight grid. A note is at
   least one step long. */
static ScoreQuantizeResult snap_items(Item *items, size_t count, const MeterMap *meter, const Cell *cells,
                                      size_t cell_count, const ScoreQuantizeOptions *options) {
    for (size_t i = 0; i < count; i++) {
        Item *item = &items[i];
        if (snap(item->start, &item->cell, item->step, &item->snapped_start) != 0) {
            return SCORE_QUANTIZE_ERR_NOT_REPRESENTABLE;
        }
        Span end_beat;
        ScoreQuantizeResult located = locate_beat(meter, item->end, &end_beat);
        if (located != SCORE_QUANTIZE_OK) {
            return located;
        }
        const Cell *cell = find_cell(cells, cell_count, item->end);
        Span end_cell = end_beat;
        Rational end_step = rational_ratio(1, options->grid_denominator);
        if (cell != NULL) {
            end_cell.start = cell->start;
            end_cell.len = cell->length;
            end_step = cell->step;
            if (rational_add(cell->start, cell->length, &end_cell.end) != RATIONAL_OK) {
                return SCORE_QUANTIZE_ERR_NOT_REPRESENTABLE;
            }
        }
        Rational shortest;
        if (snap(item->end, &end_cell, end_step, &item->snapped_end) != 0 ||
            rational_add(item->snapped_start, item->step, &shortest) != RATIONAL_OK) {
            return SCORE_QUANTIZE_ERR_NOT_REPRESENTABLE;
        }
        if (rational_cmp(item->snapped_end, shortest) < 0) {
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
    Rational start;
    size_t item;
} VoiceKey;

static int compare_voice_keys(const void *left, const void *right) {
    const VoiceKey *a = (const VoiceKey *)left;
    const VoiceKey *b = (const VoiceKey *)right;
    if (a->group != b->group) {
        return a->group < b->group ? -1 : 1;
    }
    int order = rational_cmp(a->start, b->start);
    if (order != 0) {
        return order;
    }
    return a->item < b->item ? -1 : (a->item > b->item ? 1 : 0);
}

/* True when the end of a note should move to the next onset of its voice: the gap is positive, less than a beat and at
   most gap_absorb times the note's own notated length. */
static int absorbs(const Item *item, Rational next, const ScoreQuantizeOptions *options, bool *absorb) {
    *absorb = false;
    if (rational_cmp(next, item->snapped_end) <= 0) {
        return 0;
    }
    Rational gap;
    Rational length;
    Rational limit;
    if (rational_sub(next, item->snapped_end, &gap) != RATIONAL_OK || rational_sub(item->snapped_end, item->snapped_start, &length) != RATIONAL_OK ||
        rational_mul(length, rational_ratio(options->gap_absorb_numerator, options->gap_absorb_denominator), &limit) != RATIONAL_OK) {
        return -1;
    }
    *absorb = rational_cmp(gap, item->beat.len) < 0 && rational_cmp(gap, limit) <= 0;
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
    Rational *ends = (Rational *)malloc(count * sizeof(Rational));
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
        while (next < count && keys[next].group == keys[k].group && rational_cmp(keys[next].start, keys[k].start) == 0) {
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
        out->start_num[i] = items[i].snapped_start.num;
        out->start_den[i] = items[i].snapped_start.den;
        out->end_num[i] = items[i].snapped_end.num;
        out->end_den[i] = items[i].snapped_end.den;
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
    Cell *cells = (Cell *)calloc(count > 0 ? 2 * count : 1, sizeof(Cell));
    size_t cell_count = 0;
    ScoreQuantizeResult result = items != NULL && cells != NULL ? SCORE_QUANTIZE_OK : SCORE_QUANTIZE_ERR_ALLOC;
    if (result == SCORE_QUANTIZE_OK) {
        result = fill_items(items, events, count, part_index, staff, meter_map);
    }
    if (result == SCORE_QUANTIZE_OK && count > 0) {
        result = choose_grids(items, count, options, cells, &cell_count);
    }
    if (result == SCORE_QUANTIZE_OK) {
        result = snap_items(items, count, meter_map, cells, cell_count, options);
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
    free(cells);
    meter_map_destroy(own_meter);
    return result;
}
