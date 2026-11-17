#include "analysis/harmony/key_regions_internal.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>


/* The score a path earns for key k in a window: correlation plus functional evidence. */
static double window_score(const CommonPracticeKeyWindow *window, int key) {
    return window->correlation[key] + window->evidence[key];
}

/* The least-cost key per window (see key_regions.h). */
CommonPracticeKeyRegionsStatus key_regions_choose_path(const CommonPracticeKeyWindows *windows, double penalty,
                                                   int *path) {
    size_t n = windows->count;
    double(*cost)[COMMON_PRACTICE_KEY_COUNT] =
        (double(*)[COMMON_PRACTICE_KEY_COUNT])calloc(n, sizeof(double[COMMON_PRACTICE_KEY_COUNT]));
    int(*from)[COMMON_PRACTICE_KEY_COUNT] =
        (int(*)[COMMON_PRACTICE_KEY_COUNT])calloc(n, sizeof(int[COMMON_PRACTICE_KEY_COUNT]));
    if (cost == NULL || from == NULL) {
        free(cost);
        free(from);
        return COMMON_PRACTICE_KEY_REGIONS_ERR_ALLOC;
    }
    for (int k = 0; k < COMMON_PRACTICE_KEY_COUNT; k++) {
        cost[0][k] = -window_score(&windows->items[0], k);
        from[0][k] = k;
    }
    for (size_t w = 1; w < n; w++) {
        /* the cheapest previous key to change FROM (lowest index on a tie) */
        int cheapest = 0;
        for (int j = 1; j < COMMON_PRACTICE_KEY_COUNT; j++) {
            if (cost[w - 1][j] < cost[w - 1][cheapest] - TIE) {
                cheapest = j;
            }
        }
        for (int k = 0; k < COMMON_PRACTICE_KEY_COUNT; k++) {
            /* the cheapest previous key other than k */
            int other = cheapest != k ? cheapest : (k == 0 ? 1 : 0);
            for (int j = 0; j < COMMON_PRACTICE_KEY_COUNT; j++) {
                if (j != k && cost[w - 1][j] < cost[w - 1][other] - TIE) {
                    other = j;
                }
            }
            double change = cost[w - 1][other] + penalty;
            if (cost[w - 1][k] <= change + TIE) {
                cost[w][k] = cost[w - 1][k] - window_score(&windows->items[w], k);
                from[w][k] = k;
            } else {
                cost[w][k] = change - window_score(&windows->items[w], k);
                from[w][k] = other;
            }
        }
    }
    int last = 0;
    for (int k = 1; k < COMMON_PRACTICE_KEY_COUNT; k++) {
        if (cost[n - 1][k] < cost[n - 1][last] - TIE) {
            last = k;
        }
    }
    for (size_t w = n; w-- > 0;) {
        path[w] = last;
        last = from[w][last];
    }
    free(cost);
    free(from);
    return COMMON_PRACTICE_KEY_REGIONS_OK;
}

CommonPracticeKeyRegionsStatus key_regions_build_region(const CommonPracticeKeyRegionsConfig *config, size_t number,
                                                    int key_index, const MeterMap *map, int first_measure,
                                                    int last_measure, double uncertainty,
                                                    CommonPracticeRegionSource source, CommonPracticeKeyRegion *out) {
    memset(out, 0, sizeof(*out));
    out->key_index = key_index;
    out->first_measure = first_measure;
    out->last_measure = last_measure;
    out->uncertainty = uncertainty;
    out->source = source;

    TimeSpan first;
    TimeSpan last;
    if (meter_map_measure_span(map, first_measure, &first) != METER_MAP_OK ||
        meter_map_measure_span(map, last_measure, &last) != METER_MAP_OK) {
        return COMMON_PRACTICE_KEY_REGIONS_ERR_METER_MAP;
    }
    if (time_span_make(first.start, last.end, &out->span) != TIME_SPAN_OK) {
        return COMMON_PRACTICE_KEY_REGIONS_ERR_ALLOC;
    }

    char context[160];
    snprintf(context, sizeof(context), "%s-%zu", config->context_id_prefix, number);
    EntityID *context_id = NULL;
    if (entity_id_create(context, &context_id) != ENTITY_ID_OK) {
        return COMMON_PRACTICE_KEY_REGIONS_ERR_ALLOC;
    }
    KeyResult built = common_practice_build_key(common_practice_key_index_mode(key_index),
                                                common_practice_key_index_tonic_name(key_index), context_id, &out->key);
    if (built != KEY_OK) {
        entity_id_destroy(context_id);
        return built == KEY_ERR_ALLOC ? COMMON_PRACTICE_KEY_REGIONS_ERR_ALLOC : COMMON_PRACTICE_KEY_REGIONS_ERR_KEY;
    }
    return COMMON_PRACTICE_KEY_REGIONS_OK;
}

static double clamp01(double value) {
    return value < 0.0 ? 0.0 : (value > 1.0 ? 1.0 : value);
}

/* 1 - (r(chosen) - r(best other)) for the pooled weights of measures [from, to] (slots in `measures`). */
double key_regions_region_uncertainty(const MeasureWeights *measures, size_t from, size_t to, int key_index,
                                 const CommonPracticeKeyProfiles *profiles) {
    double sum[12] = {0.0};
    for (size_t m = from; m <= to; m++) {
        for (int pc = 0; pc < 12; pc++) {
            sum[pc] += measures->weights[m][pc];
        }
    }
    double chosen = 0.0;
    double other = -2.0;
    for (int k = 0; k < COMMON_PRACTICE_KEY_COUNT; k++) {
        double profile[12];
        key_regions_rotated_profile(profiles, k, profile);
        double r = key_regions_correlate(sum, profile);
        if (k == key_index) {
            chosen = r;
        } else if (r > other) {
            other = r;
        }
    }
    return clamp01(1.0 - (chosen - other));
}

CommonPracticeKeyRegionsStatus key_regions_choose_path_restricted(const CommonPracticeKeyWindows *windows, double penalty,
                                                              unsigned allowed, int *path) {
    size_t n = windows->count;
    size_t cells = n * COMMON_PRACTICE_KEY_COUNT;
    double *cost0 = (double *)calloc(cells, sizeof(double));
    double *cost1 = (double *)calloc(cells, sizeof(double));
    int *from_phase = (int *)calloc(cells, sizeof(int));
    int *from_key = (int *)calloc(cells, sizeof(int));
    if (cost0 == NULL || cost1 == NULL || from_phase == NULL || from_key == NULL) {
        free(cost0);
        free(cost1);
        free(from_phase);
        free(from_key);
        return COMMON_PRACTICE_KEY_REGIONS_ERR_ALLOC;
    }
#define AT(w, k) ((w) * COMMON_PRACTICE_KEY_COUNT + (k))
    for (int k = 0; k < COMMON_PRACTICE_KEY_COUNT; k++) {
        cost0[AT(0, k)] = ((allowed >> k) & 1u) ? -window_score(&windows->items[0], k) : BIG;
        cost1[AT(0, k)] = BIG;
    }
    for (size_t w = 1; w < n; w++) {
        for (int k = 0; k < COMMON_PRACTICE_KEY_COUNT; k++) {
            double r = window_score(&windows->items[w], k);
            cost0[AT(w, k)] = cost0[AT(w - 1, k)] >= BIG ? BIG : cost0[AT(w - 1, k)] - r;
            /* into phase 1 at key k: stay in phase 1, or change from any other key in either phase */
            double best = cost1[AT(w - 1, k)];
            int best_phase = 1;
            int best_key = k;
            for (int j = 0; j < COMMON_PRACTICE_KEY_COUNT; j++) {
                if (j == k) {
                    continue;
                }
                for (int phase = 0; phase < 2; phase++) {
                    double previous = (phase == 0 ? cost0 : cost1)[AT(w - 1, j)];
                    if (previous >= BIG) {
                        continue;
                    }
                    double candidate = previous + penalty;
                    if (best >= BIG || candidate < best - TIE) {
                        best = candidate;
                        best_phase = phase;
                        best_key = j;
                    }
                }
            }
            cost1[AT(w, k)] = best >= BIG ? BIG : best - r;
            from_phase[AT(w, k)] = best_phase;
            from_key[AT(w, k)] = best_key;
        }
    }
    int last_phase = 0;
    int last_key = 0;
    double best = BIG;
    for (int phase = 0; phase < 2; phase++) {
        for (int k = 0; k < COMMON_PRACTICE_KEY_COUNT; k++) {
            double c = (phase == 0 ? cost0 : cost1)[AT(n - 1, k)];
            if (c < BIG && (best >= BIG || c < best - TIE)) {
                best = c;
                last_phase = phase;
                last_key = k;
            }
        }
    }
    for (size_t w = n; w-- > 0;) {
        path[w] = last_key;
        if (last_phase == 1) {
            int phase = from_phase[AT(w, last_key)];
            int key = from_key[AT(w, last_key)];
            last_phase = phase;
            last_key = key;
        }
    }
#undef AT
    free(cost0);
    free(cost1);
    free(from_phase);
    free(from_key);
    return COMMON_PRACTICE_KEY_REGIONS_OK;
}

static unsigned signature_keys(int sharps_flats) {
    int tonic = pitch_class_of_midi(7 * sharps_flats); /* the major key: sharps climb by fifths from C */
    int relative = (tonic + 9) % 12;
    return (1u << tonic) | (1u << (12 + relative));
}

/* The measure whose start is nearest `position`. */
CommonPracticeKeyRegionsStatus key_regions_nearest_barline(const MeterMap *map, MusicalTime position,
                                                       int *out_measure) {
    int measure = 0;
    TimeSpan span;
    if (meter_map_measure_index_at(map, &position, &measure) != METER_MAP_OK ||
        meter_map_measure_span(map, measure, &span) != METER_MAP_OK) {
        return COMMON_PRACTICE_KEY_REGIONS_ERR_METER_MAP;
    }
    Rational at = position.value;
    Rational start = span.start.value;
    Rational end = span.end.value;
    Rational since = {0, 1};
    Rational until = {0, 1};
    if (rational_sub(at, start, &since) != RATIONAL_OK || rational_sub(end, at, &until) != RATIONAL_OK) {
        return COMMON_PRACTICE_KEY_REGIONS_ERR_METER_MAP;
    }
    *out_measure = rational_cmp(since, until) > 0 ? measure + 1 : measure;
    return COMMON_PRACTICE_KEY_REGIONS_OK;
}

CommonPracticeKeyRegionsStatus key_regions_plan_spans(const CommonPracticeKeySignatureEvent *events, size_t event_count,
                                                  const MeterMap *map, const MeasureWeights *measures,
                                                  SpanPlan *plan) {
    if (event_count > 0 && events == NULL) {
        return COMMON_PRACTICE_KEY_REGIONS_ERR_NULL_ARGUMENT;
    }
    plan->start = (size_t *)calloc(event_count + 1, sizeof(size_t));
    plan->allowed = (unsigned *)calloc(event_count + 1, sizeof(unsigned));
    if (plan->start == NULL || plan->allowed == NULL) {
        return COMMON_PRACTICE_KEY_REGIONS_ERR_ALLOC;
    }
    plan->count = 1;
    bool have_previous = false;
    int previous_sharps = 0;
    for (size_t i = 0; i < event_count; i++) {
        const CommonPracticeKeySignatureEvent *event = &events[i];
        if (!key_regions_time_ok(event->position) || event->sharps_flats < -7 || event->sharps_flats > 7 ||
            (i > 0 && key_regions_before(event->position, events[i - 1].position))) {
            return COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_EVENT;
        }
        if (have_previous && event->sharps_flats == previous_sharps) {
            continue; /* the same signature again: not a change */
        }
        have_previous = true;
        previous_sharps = event->sharps_flats;
        int measure = 0;
        CommonPracticeKeyRegionsStatus snapped = key_regions_nearest_barline(map, event->position, &measure);
        if (snapped != COMMON_PRACTICE_KEY_REGIONS_OK) {
            return snapped;
        }
        long slot = (long)measure - (long)measures->first_measure;
        unsigned keys = signature_keys(event->sharps_flats);
        if (slot <= 0) {
            plan->allowed[0] = keys;
        } else if ((size_t)slot < measures->measure_count) {
            if (plan->start[plan->count - 1] == (size_t)slot) {
                plan->allowed[plan->count - 1] = keys;
            } else {
                plan->start[plan->count] = (size_t)slot;
                plan->allowed[plan->count] = keys;
                plan->count++;
            }
        }
    }
    return COMMON_PRACTICE_KEY_REGIONS_OK;
}
