#include "meter_analysis_density.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "meter_analysis_claim.h"
#include "meter_analysis_position.h"
#include "time/meter_arithmetic.h"
#include "time/score_time.h"

typedef struct {
    Rational start;
    Rational duration;
    int line;
} Member;

static int compare_q(const void *a, const void *b) {
    return rational_cmp(*(const Rational *)a, *(const Rational *)b);
}

static int compare_ints(const void *a, const void *b) {
    int left = *(const int *)a;
    int right = *(const int *)b;
    return (left > right) - (left < right);
}

/* True when `time` is exactly on a beat of its measure. */
static bool on_beat(const MeterMap *map, Rational time) {
    int measure = 0;
    StandardMeterMeasureGrid grid;
    if (meter_map_measure_of(map, time, &measure) != METER_MAP_OK || !standard_meter_measure_grid(map, measure, &grid)) {
        return false;
    }
    Rational offset;
    if (rational_sub(time, grid.base, &offset) != RATIONAL_OK) {
        return false;
    }
    return offset.num >= 0 && (offset.num * grid.beat.den) % (offset.den * grid.beat.num) == 0;
}

/* The profile of the members of one stream (`line` -1 takes every member). */
static void profile(const StandardMeterInput *input, const Member *members, size_t count, int line, int phrase_index,
                    Rational phrase_start, Rational phrase_end, int first_measure, int last_measure,
                    StandardMeterDensityClaim *claim) {
    memset(claim, 0, sizeof(*claim));
    claim->phrase = phrase_index;
    claim->line = line;

    Rational *onsets = (Rational *)malloc((count == 0 ? 1 : count) * sizeof(Rational));
    Rational *durations = (Rational *)malloc((count == 0 ? 1 : count) * sizeof(Rational));
    if (onsets == NULL || durations == NULL) {
        free(onsets);
        free(durations);
        return;
    }
    size_t notes = 0;
    for (size_t i = 0; i < count; i++) {
        if (line == -1 || members[i].line == line) {
            onsets[notes] = members[i].start;
            durations[notes] = members[i].duration;
            notes++;
        }
    }
    qsort(onsets, notes, sizeof(Rational), compare_q);
    qsort(durations, notes, sizeof(Rational), compare_q);
    size_t attacks = 0;
    for (size_t i = 0; i < notes; i++) {
        if (i == 0 || rational_cmp(onsets[i], onsets[i - 1]) != 0) {
            onsets[attacks++] = onsets[i];
        }
    }
    claim->notes = (unsigned)notes;
    claim->attacks = (unsigned)attacks;

    if (attacks >= 2) {
        Rational span;
        if (rational_sub(onsets[attacks - 1], onsets[0], &span) == RATIONAL_OK) {
            claim->mean_inter_onset = rational_to_double(span) / (double)(attacks - 1);
        }
    }
    size_t on = 0;
    for (size_t i = 0; i < attacks; i++) {
        if (on_beat(input->map, onsets[i])) {
            on++;
        }
    }
    if (attacks > 0) {
        claim->on_beat_share = (double)on / (double)attacks;
        claim->off_beat_share = 1.0 - claim->on_beat_share;
    }

    double total = 0.0;
    double largest = 0.0;
    int measured = 0;
    for (int measure = first_measure; measure <= last_measure; measure++) {
        StandardMeterMeasureGrid grid;
        if (!standard_meter_measure_grid(input->map, measure, &grid)) {
            continue;
        }
        Rational from = rational_cmp(grid.start, phrase_start) > 0 ? grid.start : phrase_start;
        Rational to = rational_cmp(grid.end, phrase_end) < 0 ? grid.end : phrase_end;
        if (rational_cmp(to, from) <= 0) {
            continue;
        }
        size_t in_part = 0;
        for (size_t i = 0; i < attacks; i++) {
            if (rational_cmp(onsets[i], from) >= 0 && rational_cmp(onsets[i], to) < 0) {
                in_part++;
            }
        }
        Rational length;
        if (rational_sub(to, from, &length) != RATIONAL_OK) {
            continue;
        }
        double beats = rational_to_double(length) / rational_to_double(grid.beat);
        double rate = (double)in_part / beats;
        total += rate;
        if (rate > largest) {
            largest = rate;
        }
        measured++;
    }
    claim->measures = measured;
    if (measured > 0) {
        claim->attacks_per_beat_mean = total / (double)measured;
        claim->attacks_per_beat_max = largest;
    }

    for (size_t i = 0; i < notes;) {
        size_t j = i;
        while (j < notes && rational_cmp(durations[j], durations[i]) == 0) {
            j++;
        }
        if (claim->bin_count == STANDARD_METER_DENSITY_BINS) {
            claim->bins_truncated = 1;
            break;
        }
        StandardMeterDurationBin *bin = &claim->bins[claim->bin_count++];
        bin->numerator = (long)durations[i].num;
        bin->denominator = (unsigned long)durations[i].den;
        bin->count = (unsigned)(j - i);
        i = j;
    }
    free(onsets);
    free(durations);
}

static StandardMeterAnalysisStatus add_profile(StandardMeterDensities *found, size_t *capacity,
                                               const StandardMeterDensityClaim *claim, const char *phrase_id) {
    if (found->count == *capacity) {
        size_t next = *capacity == 0 ? 16 : *capacity * 2;
        StandardMeterDensity *grown = (StandardMeterDensity *)realloc(found->items, next * sizeof(*grown));
        if (grown == NULL) {
            return STANDARD_METER_ANALYSIS_ERR_ALLOC;
        }
        found->items = grown;
        *capacity = next;
    }
    StandardMeterDensity *item = &found->items[found->count];
    memset(item, 0, sizeof(*item));
    item->claim = *claim;
    char interpretation_id[200];
    if (claim->line == -1) {
        snprintf(interpretation_id, sizeof(interpretation_id), "standard-meter-density-p%d-all", claim->phrase);
    } else {
        snprintf(interpretation_id, sizeof(interpretation_id), "standard-meter-density-p%d-line%d", claim->phrase, claim->line);
    }
    item->interpretation = standard_meter_analysis_interpretation(interpretation_id, phrase_id, STANDARD_METER_DENSITY_CLAIM_TYPE,
                                                                  claim, sizeof(*claim), 0.0, "density-profile");
    if (item->interpretation == NULL) {
        return STANDARD_METER_ANALYSIS_ERR_ALLOC;
    }
    found->count++;
    return STANDARD_METER_ANALYSIS_OK;
}

StandardMeterAnalysisStatus standard_meter_find_densities(const StandardMeterInput *input, const StandardMeterConfig *config,
                                                          StandardMeterDensities *out) {
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
    StandardMeterDensities found = {NULL, 0};
    size_t capacity = 0;
    Member *members = (Member *)malloc((input->note_count == 0 ? 1 : input->note_count) * sizeof(Member));
    int *lines = (int *)malloc((input->note_count == 0 ? 1 : input->note_count) * sizeof(int));
    if (members == NULL || lines == NULL) {
        free(members);
        free(lines);
        return STANDARD_METER_ANALYSIS_ERR_ALLOC;
    }
    StandardMeterAnalysisStatus status = STANDARD_METER_ANALYSIS_OK;
    for (size_t p = 0; status == STANDARD_METER_ANALYSIS_OK && p < input->phrase_count; p++) {
        Rational start;
        Rational end;
        int first = 0;
        int last = 0;
        if (!score_time_span(input->phrases[p].span, &start, &end) || meter_map_measure_of(input->map, start, &first) != METER_MAP_OK ||
            meter_map_last_measure_touched(input->map, end, &last) != METER_MAP_OK) {
            continue;
        }
        size_t count = 0;
        size_t line_count = 0;
        for (size_t i = 0; i < input->note_count; i++) {
            Rational note_start;
            Rational note_end;
            if (!score_time_span(input->notes[i].span, &note_start, &note_end) ||
                !standard_meter_snap_time(config, &note_start, false, NULL) ||
                !standard_meter_snap_time(config, &note_end, true, NULL)) {
                continue;
            }
            if (rational_cmp(note_start, start) < 0 || rational_cmp(note_start, end) >= 0 ||
                rational_cmp(note_end, note_start) <= 0) {
                continue;
            }
            members[count].start = note_start;
            if (rational_sub(note_end, note_start, &members[count].duration) != RATIONAL_OK) {
                continue;
            }
            members[count].line = input->notes[i].line;
            lines[line_count++] = input->notes[i].line;
            count++;
        }
        qsort(lines, line_count, sizeof(int), compare_ints);
        size_t distinct = 0;
        for (size_t i = 0; i < line_count; i++) {
            if (i == 0 || lines[i] != lines[i - 1]) {
                lines[distinct++] = lines[i];
            }
        }
        StandardMeterDensityClaim claim;
        profile(input, members, count, -1, (int)p, start, end, first, last, &claim);
        status = add_profile(&found, &capacity, &claim, input->phrases[p].id);
        for (size_t l = 0; status == STANDARD_METER_ANALYSIS_OK && l < distinct; l++) {
            profile(input, members, count, lines[l], (int)p, start, end, first, last, &claim);
            status = add_profile(&found, &capacity, &claim, input->phrases[p].id);
        }
    }
    free(members);
    free(lines);
    if (status != STANDARD_METER_ANALYSIS_OK) {
        standard_meter_densities_destroy(&found);
        return status;
    }
    *out = found;
    return STANDARD_METER_ANALYSIS_OK;
}

void standard_meter_densities_destroy(StandardMeterDensities *densities) {
    if (densities == NULL) {
        return;
    }
    for (size_t i = 0; i < densities->count; i++) {
        interpretation_destroy(densities->items[i].interpretation);
    }
    free(densities->items);
    densities->items = NULL;
    densities->count = 0;
}
