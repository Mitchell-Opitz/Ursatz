#include "meter_analysis_syncopation.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "meter_analysis_claim.h"
#include "meter_analysis_time.h"
#include "syncopation_rules.h"
#include "time/meter_arithmetic.h"
#include "time/score_time.h"

typedef struct {
    Rational start;
    Rational end;
    bool snapped;
    size_t note;
} Item;

static int compare_items(const void *a, const void *b) {
    const Item *left = (const Item *)a;
    const Item *right = (const Item *)b;
    int by_start = rational_cmp(left->start, right->start);
    if (by_start != 0) {
        return by_start;
    }
    int by_end = rational_cmp(left->end, right->end);
    if (by_end != 0) {
        return by_end;
    }
    return (left->note > right->note) - (left->note < right->note);
}

static int compare_ints(const void *a, const void *b) {
    int left = *(const int *)a;
    int right = *(const int *)b;
    return (left > right) - (left < right);
}

/* True when some item of the (start-sorted) stream is attacked at `time`. */
static bool attacked_at(const Item *items, size_t count, Rational time) {
    size_t low = 0;
    size_t high = count;
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        int order = rational_cmp(items[mid].start, time);
        if (order == 0) {
            return true;
        }
        if (order < 0) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }
    return false;
}

/* The strongest position the item is held through that nothing in the stream attacks, if it is stronger than the attack. */
static bool through_position(const MeterMap *map, const Item *items, size_t count, const Item *item, StandardMeterLevel attack,
                             StandardMeterLevel *through, Rational *where) {
    int first = 0;
    int last = 0;
    if (meter_map_measure_of(map, item->start, &first) != METER_MAP_OK ||
        meter_map_last_measure_touched(map, item->end, &last) != METER_MAP_OK) {
        return false;
    }
    bool found = false;
    StandardMeterLevel best = attack;
    for (int measure = first; measure <= last; measure++) {
        StandardMeterMeasureGrid grid;
        if (!standard_meter_measure_grid(map, measure, &grid)) {
            continue;
        }
        long long points = (long long)grid.beats * (long long)grid.division;
        for (long long k = 0; k < points; k++) {
            Rational scaled;
            Rational step;
            Rational p;
            if (rational_mul_int(grid.beat, k, &scaled) != RATIONAL_OK ||
                rational_div_int(scaled, (long long)grid.division, &step) != RATIONAL_OK ||
                rational_add(grid.base, step, &p) != RATIONAL_OK) {
                continue;
            }
            if (rational_cmp(p, item->start) <= 0 || rational_cmp(p, item->end) >= 0 || rational_cmp(p, grid.start) < 0 ||
                rational_cmp(p, grid.end) >= 0) {
                continue;
            }
            StandardMeterLevel level;
            if (!standard_meter_level_at(map, p, &level) || level >= best || attacked_at(items, count, p)) {
                continue;
            }
            best = level;
            *where = p;
            found = true;
        }
    }
    if (found) {
        *through = best;
    }
    return found;
}

static StandardMeterAnalysisStatus detect_stream(const StandardMeterInput *input, Item *items, size_t count, int stream_line,
                                                 bool all_notes, StandardMeterSyncopation **results, size_t *result_count,
                                                 size_t *result_capacity) {
    (void)stream_line;
    qsort(items, count, sizeof(Item), compare_items);
    for (size_t i = 0; i < count; i++) {
        const Item *item = &items[i];
        StandardMeterLevel attack;
        if (!standard_meter_level_at(input->map, item->start, &attack) || attack == STANDARD_METER_LEVEL_DOWNBEAT) {
            continue;
        }
        StandardMeterLevel through;
        Rational where = item->start;
        unsigned strength = 0;
        if (!through_position(input->map, items, count, item, attack, &through, &where) ||
            !standard_meter_syncopated_through_stronger(attack, through, &strength)) {
            continue;
        }
        if (*result_count == *result_capacity) {
            size_t next = *result_capacity == 0 ? 64 : *result_capacity * 2;
            StandardMeterSyncopation *grown = (StandardMeterSyncopation *)realloc(*results, next * sizeof(**results));
            if (grown == NULL) {
                return STANDARD_METER_ANALYSIS_ERR_ALLOC;
            }
            *results = grown;
            *result_capacity = next;
        }
        const StandardMeterNote *note = &input->notes[item->note];
        StandardMeterSyncopation *result = &(*results)[*result_count];
        memset(result, 0, sizeof(*result));
        StandardMeterSyncopationClaim *claim = &result->claim;
        memcpy(claim->note_id, note->id, sizeof(claim->note_id));
        claim->line = note->line;
        claim->attack_level = (int)attack;
        claim->through_level = (int)through;
        claim->strength = strength;
        if (!standard_meter_beat_of(input->map, item->start, &claim->measure, &claim->beat) ||
            !standard_meter_beat_of(input->map, where, &claim->through_measure, &claim->through_beat)) {
            continue;
        }
        result->uncertainty = item->snapped ? 0.1 : 0.0;
        char interpretation_id[200];
        snprintf(interpretation_id, sizeof(interpretation_id), "standard-meter-syncopation-%s%s", note->id,
                 all_notes ? "-all" : "");
        result->interpretation = standard_meter_analysis_interpretation(interpretation_id, note->id,
                                                                        STANDARD_METER_SYNCOPATION_CLAIM_TYPE, claim,
                                                                        sizeof(*claim), result->uncertainty, "syncopation");
        if (result->interpretation == NULL) {
            return STANDARD_METER_ANALYSIS_ERR_ALLOC;
        }
        (*result_count)++;
    }
    return STANDARD_METER_ANALYSIS_OK;
}

StandardMeterAnalysisStatus standard_meter_find_syncopations(const StandardMeterInput *input, const StandardMeterConfig *config,
                                                             StandardMeterSyncopations *out) {
    if (input == NULL || out == NULL || input->map == NULL) {
        return STANDARD_METER_ANALYSIS_ERR_NULL_ARGUMENT;
    }
    StandardMeterConfig fallback = standard_meter_config_default();
    if (config == NULL) {
        config = &fallback;
    }
    if (!standard_meter_config_valid(config)) {
        return STANDARD_METER_ANALYSIS_ERR_INVALID;
    }
    StandardMeterSyncopations found = {NULL, 0};
    size_t capacity = 0;
    if (input->note_count == 0) {
        *out = found;
        return STANDARD_METER_ANALYSIS_OK;
    }
    Item *items = (Item *)malloc(input->note_count * sizeof(Item));
    int *lines = (int *)malloc(input->note_count * sizeof(int));
    if (items == NULL || lines == NULL) {
        free(items);
        free(lines);
        return STANDARD_METER_ANALYSIS_ERR_ALLOC;
    }
    size_t usable = 0;
    size_t line_count = 0;
    for (size_t i = 0; i < input->note_count; i++) {
        Rational start;
        Rational end;
        bool moved_start = false;
        if (!score_time_span(input->notes[i].span, &start, &end) ||
            !standard_meter_snap_time(config, &start, false, &moved_start) || !standard_meter_snap_time(config, &end, true, NULL)) {
            continue;
        }
        if (rational_cmp(end, start) <= 0) {
            continue;
        }
        items[usable].start = start;
        items[usable].end = end;
        items[usable].snapped = moved_start;
        items[usable].note = i;
        usable++;
        lines[line_count++] = input->notes[i].line;
    }
    qsort(lines, line_count, sizeof(int), compare_ints);
    size_t distinct = 0;
    for (size_t i = 0; i < line_count; i++) {
        if (i == 0 || lines[i] != lines[i - 1]) {
            lines[distinct++] = lines[i];
        }
    }

    StandardMeterAnalysisStatus status = STANDARD_METER_ANALYSIS_OK;
    Item *stream = (Item *)malloc((usable == 0 ? 1 : usable) * sizeof(Item));
    if (stream == NULL) {
        status = STANDARD_METER_ANALYSIS_ERR_ALLOC;
    }
    size_t streams = config->syncopation_all_notes ? 1 : distinct;
    for (size_t s = 0; status == STANDARD_METER_ANALYSIS_OK && s < streams; s++) {
        size_t n = 0;
        for (size_t i = 0; i < usable; i++) {
            if (config->syncopation_all_notes || input->notes[items[i].note].line == lines[s]) {
                stream[n++] = items[i];
            }
        }
        status = detect_stream(input, stream, n, config->syncopation_all_notes ? -1 : lines[s], config->syncopation_all_notes,
                               &found.items, &found.count, &capacity);
    }
    free(stream);
    free(items);
    free(lines);
    if (status != STANDARD_METER_ANALYSIS_OK) {
        standard_meter_syncopations_destroy(&found);
        return status;
    }
    *out = found;
    return STANDARD_METER_ANALYSIS_OK;
}

void standard_meter_syncopations_destroy(StandardMeterSyncopations *syncopations) {
    if (syncopations == NULL) {
        return;
    }
    for (size_t i = 0; i < syncopations->count; i++) {
        interpretation_destroy(syncopations->items[i].interpretation);
    }
    free(syncopations->items);
    syncopations->items = NULL;
    syncopations->count = 0;
}
