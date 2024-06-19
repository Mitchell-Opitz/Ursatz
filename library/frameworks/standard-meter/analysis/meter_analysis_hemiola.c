#include "meter_analysis_hemiola.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hemiola_rules.h"
#include "meter_analysis_claim.h"
#include "meter_analysis_time.h"

typedef struct {
    StandardMeterQ start;
    StandardMeterQ end;
    size_t note;
} Item;

static int compare_items(const void *a, const void *b) {
    const Item *left = (const Item *)a;
    const Item *right = (const Item *)b;
    int by_start = standard_meter_q_compare(left->start, right->start);
    return by_start != 0 ? by_start : (left->note > right->note) - (left->note < right->note);
}

static int compare_times(const void *a, const void *b) {
    return standard_meter_q_compare(*(const StandardMeterQ *)a, *(const StandardMeterQ *)b);
}

/* The index of the pulse `time` is on in a window starting at `start`, or -1 when it is not on a pulse of the window. */
static int pulse_index(StandardMeterQ start, StandardMeterQ pulse, StandardMeterQ time) {
    StandardMeterQ offset = standard_meter_q_sub(time, start);
    if (offset.n < 0) {
        return -1;
    }
    StandardMeterQ steps = standard_meter_q(offset.n * pulse.d, offset.d * pulse.n);
    if (steps.d != 1 || steps.n >= STANDARD_METER_HEMIOLA_PULSES) {
        return -1;
    }
    return (int)steps.n;
}

/* The first index of `array` whose time is at or after `time`. */
static size_t lower_bound_items(const Item *items, size_t count, StandardMeterQ time) {
    size_t low = 0;
    size_t high = count;
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        if (standard_meter_q_compare(items[mid].start, time) < 0) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }
    return low;
}

static size_t lower_bound_times(const StandardMeterQ *times, size_t count, StandardMeterQ time) {
    size_t low = 0;
    size_t high = count;
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        if (standard_meter_q_compare(times[mid], time) < 0) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }
    return low;
}

static StandardMeterAnalysisStatus emit_window(const StandardMeterInput *input, const Item *items, size_t count,
                                               StandardMeterQ window_start, StandardMeterQ window_end, StandardMeterQ pulse,
                                               const StandardMeterHemiolaClaim *claim, StandardMeterHemiolas *found,
                                               size_t *capacity) {
    const char **ids = (const char **)malloc((count == 0 ? 1 : count) * sizeof(char *));
    if (ids == NULL) {
        return STANDARD_METER_ANALYSIS_ERR_ALLOC;
    }
    size_t n = 0;
    for (size_t i = lower_bound_items(items, count, window_start); i < count; i++) {
        if (standard_meter_q_compare(items[i].start, window_end) >= 0) {
            break;
        }
        if (pulse_index(window_start, pulse, items[i].start) >= 0) {
            ids[n++] = input->notes[items[i].note].id;
        }
    }
    if (n == 0) {
        free(ids); /* evidence from the chord changes alone, with no note on a pulse: nothing to point at */
        return STANDARD_METER_ANALYSIS_OK;
    }
    if (found->count == *capacity) {
        size_t next = *capacity == 0 ? 8 : *capacity * 2;
        StandardMeterHemiola *grown = (StandardMeterHemiola *)realloc(found->items, next * sizeof(*grown));
        if (grown == NULL) {
            free(ids);
            return STANDARD_METER_ANALYSIS_ERR_ALLOC;
        }
        found->items = grown;
        *capacity = next;
    }
    StandardMeterHemiola *item = &found->items[found->count];
    memset(item, 0, sizeof(*item));
    item->claim = *claim;
    item->uncertainty = 1.0 - (double)claim->agreeing / 3.0;
    char key[64];
    snprintf(key, sizeof(key), "m%d", claim->first_measure);
    bool ok = standard_meter_analysis_emit("hemiola", key, ids, n, STANDARD_METER_HEMIOLA_CLAIM_TYPE, claim,
                                                    sizeof(*claim), item->uncertainty, &item->shell, &item->interpretation);
    free(ids);
    if (!ok) {
        return STANDARD_METER_ANALYSIS_ERR_ALLOC;
    }
    found->count++;
    return STANDARD_METER_ANALYSIS_OK;
}

StandardMeterAnalysisStatus standard_meter_find_hemiolas(const StandardMeterInput *input, const StandardMeterConfig *config,
                                                         StandardMeterHemiolas *out) {
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
    StandardMeterHemiolas found = {NULL, 0};
    size_t capacity = 0;
    if (input->note_count == 0) {
        *out = found;
        return STANDARD_METER_ANALYSIS_OK;
    }
    Item *items = (Item *)malloc(input->note_count * sizeof(Item));
    StandardMeterQ *changes = (StandardMeterQ *)malloc((input->chord_change_count == 0 ? 1 : input->chord_change_count) * sizeof(*changes));
    if (items == NULL || changes == NULL) {
        free(items);
        free(changes);
        return STANDARD_METER_ANALYSIS_ERR_ALLOC;
    }
    size_t count = 0;
    StandardMeterQ last_end = standard_meter_q(0, 1);
    for (size_t i = 0; i < input->note_count; i++) {
        StandardMeterQ start;
        StandardMeterQ end;
        if (!standard_meter_span_q(input->notes[i].span, &start, &end)) {
            continue;
        }
        start = standard_meter_snap(config, start, NULL);
        end = standard_meter_snap(config, end, NULL);
        if (standard_meter_q_compare(end, start) <= 0) {
            continue;
        }
        items[count].start = start;
        items[count].end = end;
        items[count].note = i;
        count++;
        if (standard_meter_q_compare(end, last_end) > 0) {
            last_end = end;
        }
    }
    size_t change_count = 0;
    for (size_t i = 0; i < input->chord_change_count; i++) {
        changes[change_count++] = standard_meter_snap(config, standard_meter_q_of_time(input->chord_changes[i].time), NULL);
    }
    qsort(items, count, sizeof(Item), compare_items);
    qsort(changes, change_count, sizeof(StandardMeterQ), compare_times);

    StandardMeterAnalysisStatus status = STANDARD_METER_ANALYSIS_OK;
    int first = meter_map_has_pickup(input->map) ? 1 : meter_map_first_measure_number(input->map);
    int last = first;
    if (count == 0 || !standard_meter_last_measure_touched(input->map, last_end, &last)) {
        last = first - 1;
    }
    for (int measure = first; status == STANDARD_METER_ANALYSIS_OK && measure <= last;) {
        const MeterMapRegion *region = NULL;
        if (meter_map_region_of_measure(input->map, measure, &region) != METER_MAP_OK) {
            break;
        }
        Duration *pulse_duration = NULL;
        unsigned span = 0;
        if (!standard_meter_hemiola_window(meter_map_region_time_signature(region), &pulse_duration, &span)) {
            measure++;
            continue;
        }
        StandardMeterQ pulse = standard_meter_q(duration_numerator(pulse_duration), (long long)duration_denominator(pulse_duration));
        duration_destroy(pulse_duration);

        StandardMeterQ window_start = standard_meter_q(0, 1);
        StandardMeterQ window_end = standard_meter_q(0, 1);
        bool full = standard_meter_measure_bounds(input->map, measure, &window_start, NULL);
        for (unsigned k = 0; full && k < span; k++) {
            const MeterMapRegion *other = NULL;
            StandardMeterQ start;
            StandardMeterQ end;
            full = meter_map_region_of_measure(input->map, measure + (int)k, &other) == METER_MAP_OK && other == region &&
                   standard_meter_measure_bounds(input->map, measure + (int)k, &start, &end);
            if (full) {
                window_end = end;
            }
        }
        /* six whole pulses, so no short measure is inside */
        if (!full ||
            standard_meter_q_compare(standard_meter_q_sub(window_end, window_start),
                                     standard_meter_q_mul_int(pulse, STANDARD_METER_HEMIOLA_PULSES)) != 0) {
            measure++;
            continue;
        }

        bool attack[STANDARD_METER_HEMIOLA_PULSES] = {false};
        bool long_onset[STANDARD_METER_HEMIOLA_PULSES] = {false};
        bool change[STANDARD_METER_HEMIOLA_PULSES] = {false};
        StandardMeterQ long_length = standard_meter_q_mul_int(pulse, 2);
        for (size_t i = lower_bound_items(items, count, window_start); i < count; i++) {
            if (standard_meter_q_compare(items[i].start, window_end) >= 0) {
                break;
            }
            int index = pulse_index(window_start, pulse, items[i].start);
            if (index < 0) {
                continue;
            }
            attack[index] = true;
            if (standard_meter_q_compare(standard_meter_q_sub(items[i].end, items[i].start), long_length) >= 0) {
                long_onset[index] = true;
            }
        }
        for (size_t i = lower_bound_times(changes, change_count, window_start); i < change_count; i++) {
            if (standard_meter_q_compare(changes[i], window_end) >= 0) {
                break;
            }
            int index = pulse_index(window_start, pulse, changes[i]);
            if (index >= 0) {
                change[index] = true;
            }
        }

        StandardMeterHemiolaClaim claim;
        memset(&claim, 0, sizeof(claim));
        claim.first_measure = measure;
        claim.last_measure = measure + (int)span - 1;
        claim.duration_evidence = standard_meter_hemiola_duration_evidence(attack) ? 1 : 0;
        claim.accent_evidence = standard_meter_hemiola_accent_evidence(long_onset, attack) ? 1 : 0;
        claim.chord_evidence = change_count == 0 ? -1 : (standard_meter_hemiola_chord_evidence(change) ? 1 : 0);
        claim.available = change_count == 0 ? 2 : 3;
        claim.agreeing = (unsigned)claim.duration_evidence + (unsigned)claim.accent_evidence +
                         (claim.chord_evidence > 0 ? 1u : 0u);
        if (standard_meter_hemiola_evidence_met(claim.agreeing, claim.available, config->hemiola_min_evidence)) {
            status = emit_window(input, items, count, window_start, window_end, pulse, &claim, &found, &capacity);
            measure += (int)span;
        } else {
            measure++;
        }
    }
    free(items);
    free(changes);
    if (status != STANDARD_METER_ANALYSIS_OK) {
        standard_meter_hemiolas_destroy(&found);
        return status;
    }
    *out = found;
    return STANDARD_METER_ANALYSIS_OK;
}

void standard_meter_hemiolas_destroy(StandardMeterHemiolas *hemiolas) {
    if (hemiolas == NULL) {
        return;
    }
    for (size_t i = 0; i < hemiolas->count; i++) {
        interpretation_destroy(hemiolas->items[i].interpretation);
        structure_destroy(hemiolas->items[i].shell);
    }
    free(hemiolas->items);
    hemiolas->items = NULL;
    hemiolas->count = 0;
}
