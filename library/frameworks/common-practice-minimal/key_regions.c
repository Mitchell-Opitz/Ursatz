#include "key_regions.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static const char *const TONIC_NAMES[12] = {"C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};

CommonPracticeKeyProfiles common_practice_krumhansl_kessler_profiles(void) {
    CommonPracticeKeyProfiles profiles = {
        {6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88},
        {6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17}};
    return profiles;
}

CommonPracticeKeyRegionsConfig common_practice_key_regions_config_default(void) {
    CommonPracticeKeyRegionsConfig config;
    config.window_measures = 4;
    config.profiles = common_practice_krumhansl_kessler_profiles();
    config.change_penalty = 0.2;
    config.tonicization_max_measures = 4;
    config.context_id_prefix = "key-region";
    config.pivot_search_measures = 2;
    return config;
}

int common_practice_key_index_tonic(int key_index) {
    if (key_index < 0 || key_index >= COMMON_PRACTICE_KEY_COUNT) {
        return -1;
    }
    return key_index % 12;
}

CommonPracticeKeyMode common_practice_key_index_mode(int key_index) {
    return key_index >= 12 && key_index < COMMON_PRACTICE_KEY_COUNT ? COMMON_PRACTICE_KEY_MINOR
                                                                    : COMMON_PRACTICE_KEY_MAJOR;
}

int common_practice_key_index_of(int tonic_pitch_class, CommonPracticeKeyMode mode) {
    if (tonic_pitch_class < 0 || tonic_pitch_class > 11 ||
        (mode != COMMON_PRACTICE_KEY_MAJOR && mode != COMMON_PRACTICE_KEY_MINOR)) {
        return -1;
    }
    return tonic_pitch_class + (mode == COMMON_PRACTICE_KEY_MINOR ? 12 : 0);
}

const char *common_practice_key_index_tonic_name(int key_index) {
    int tonic = common_practice_key_index_tonic(key_index);
    return tonic < 0 ? NULL : TONIC_NAMES[tonic];
}

void common_practice_key_windows_destroy(CommonPracticeKeyWindows *windows) {
    if (windows == NULL) {
        return;
    }
    free(windows->items);
    windows->items = NULL;
    windows->count = 0;
}

/* Pearson correlation of two 12-vectors; 0 when either is flat. */
static double correlate(const double x[12], const double y[12]) {
    double mean_x = 0.0;
    double mean_y = 0.0;
    for (int i = 0; i < 12; i++) {
        mean_x += x[i];
        mean_y += y[i];
    }
    mean_x /= 12.0;
    mean_y /= 12.0;
    double covariance = 0.0;
    double variance_x = 0.0;
    double variance_y = 0.0;
    for (int i = 0; i < 12; i++) {
        double dx = x[i] - mean_x;
        double dy = y[i] - mean_y;
        covariance += dx * dy;
        variance_x += dx * dx;
        variance_y += dy * dy;
    }
    if (!(variance_x > 0.0) || !(variance_y > 0.0)) {
        return 0.0;
    }
    return covariance / sqrt(variance_x * variance_y);
}

static bool profiles_valid(const CommonPracticeKeyProfiles *profiles) {
    for (int i = 0; i < 12; i++) {
        if (!isfinite(profiles->major[i]) || !isfinite(profiles->minor[i])) {
            return false;
        }
    }
    return true;
}

/* The key's profile rotated so index pc is the weight of pitch class pc. */
static void rotated_profile(const CommonPracticeKeyProfiles *profiles, int key_index, double out[12]) {
    int tonic = key_index % 12;
    const double *base = key_index >= 12 ? profiles->minor : profiles->major;
    for (int pc = 0; pc < 12; pc++) {
        out[pc] = base[(pc - tonic + 12) % 12];
    }
}

static bool time_ok(const MusicalTime *time) {
    return time != NULL && musical_time_numerator(time) >= 0;
}

static bool before(const MusicalTime *a, const MusicalTime *b) {
    /* a < b for non-negative times: a.n * b.d < b.n * a.d */
    return (long double)musical_time_numerator(a) * (long double)musical_time_denominator(b) <
           (long double)musical_time_numerator(b) * (long double)musical_time_denominator(a);
}

/* The per-measure pitch-class weights of the segments, from the first measure that holds a segment start to
   the last. */
typedef struct {
    int *segment_measure;  /* owned; the measure each segment starts in */
    double (*weights)[12]; /* owned; measure_count rows */
    size_t measure_count;
    int first_measure;
    int last_measure;
} MeasureWeights;

static CommonPracticeKeyRegionsStatus gather_measures(const CommonPracticeKeySegment *segments, size_t segment_count,
                                                       const MeterMap *map, MeasureWeights *out) {
    int *measure_of = (int *)calloc(segment_count, sizeof(int));
    if (measure_of == NULL) {
        return COMMON_PRACTICE_KEY_REGIONS_ERR_ALLOC;
    }
    int first_measure = 0;
    int last_measure = 0;
    for (size_t i = 0; i < segment_count; i++) {
        const CommonPracticeKeySegment *segment = &segments[i];
        bool bad = !time_ok(segment->start) || !time_ok(segment->end) || !before(segment->start, segment->end) ||
                   !isfinite(segment->weights.total) || segment->weights.total < 0.0 ||
                   (i > 0 && before(segment->start, segments[i - 1].start));
        for (int pc = 0; pc < 12 && !bad; pc++) {
            bad = !isfinite(segment->weights.pitch_class[pc]) || segment->weights.pitch_class[pc] < 0.0;
        }
        if (bad) {
            free(measure_of);
            return COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_SEGMENT;
        }
        if (meter_map_measure_index_at(map, segment->start, &measure_of[i]) != METER_MAP_OK) {
            free(measure_of);
            return COMMON_PRACTICE_KEY_REGIONS_ERR_METER_MAP;
        }
        if (i == 0 || measure_of[i] < first_measure) {
            first_measure = measure_of[i];
        }
        if (i == 0 || measure_of[i] > last_measure) {
            last_measure = measure_of[i];
        }
    }

    size_t measure_count = (size_t)(last_measure - first_measure) + 1;
    double(*per_measure)[12] = (double(*)[12])calloc(measure_count, sizeof(double[12]));
    if (per_measure == NULL) {
        free(measure_of);
        return COMMON_PRACTICE_KEY_REGIONS_ERR_ALLOC;
    }
    for (size_t i = 0; i < segment_count; i++) {
        size_t slot = (size_t)(measure_of[i] - first_measure);
        for (int pc = 0; pc < 12; pc++) {
            per_measure[slot][pc] += segments[i].weights.pitch_class[pc];
        }
    }
    out->segment_measure = measure_of;
    out->weights = per_measure;
    out->measure_count = measure_count;
    out->first_measure = first_measure;
    out->last_measure = last_measure;
    return COMMON_PRACTICE_KEY_REGIONS_OK;
}

static void measures_release(MeasureWeights *measures) {
    free(measures->segment_measure);
    free(measures->weights);
}

static CommonPracticeKeyRegionsStatus validate_inputs(const CommonPracticeKeySegment *segments, size_t segment_count,
                                                       const MeterMap *map, const CommonPracticeKeyRegionsConfig *config) {
    if (map == NULL || (segment_count > 0 && segments == NULL)) {
        return COMMON_PRACTICE_KEY_REGIONS_ERR_NULL_ARGUMENT;
    }
    if (segment_count == 0) {
        return COMMON_PRACTICE_KEY_REGIONS_ERR_NO_SEGMENTS;
    }
    if (config->window_measures == 0 || !profiles_valid(&config->profiles)) {
        return COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_CONFIG;
    }
    return COMMON_PRACTICE_KEY_REGIONS_OK;
}

/* Scores the windows of already-gathered measures. */
static CommonPracticeKeyRegionsStatus score_windows(const MeasureWeights *measures,
                                                     const CommonPracticeKeyRegionsConfig *config,
                                                     CommonPracticeKeyWindows *out) {
    size_t window_length = config->window_measures < measures->measure_count ? config->window_measures
                                                                              : measures->measure_count;
    size_t window_count = measures->measure_count - window_length + 1;
    CommonPracticeKeyWindow *windows = (CommonPracticeKeyWindow *)calloc(window_count, sizeof(CommonPracticeKeyWindow));
    if (windows == NULL) {
        return COMMON_PRACTICE_KEY_REGIONS_ERR_ALLOC;
    }
    double profile[COMMON_PRACTICE_KEY_COUNT][12];
    for (int k = 0; k < COMMON_PRACTICE_KEY_COUNT; k++) {
        rotated_profile(&config->profiles, k, profile[k]);
    }
    for (size_t w = 0; w < window_count; w++) {
        double sum[12] = {0.0};
        for (size_t m = w; m < w + window_length; m++) {
            for (int pc = 0; pc < 12; pc++) {
                sum[pc] += measures->weights[m][pc];
            }
        }
        windows[w].first_measure = measures->first_measure + (int)w;
        windows[w].last_measure = measures->first_measure + (int)(w + window_length) - 1;
        for (int k = 0; k < COMMON_PRACTICE_KEY_COUNT; k++) {
            windows[w].correlation[k] = correlate(sum, profile[k]);
        }
    }
    out->items = windows;
    out->count = window_count;
    out->first_measure = measures->first_measure;
    out->last_measure = measures->last_measure;
    return COMMON_PRACTICE_KEY_REGIONS_OK;
}

CommonPracticeKeyRegionsStatus key_window_scores(const CommonPracticeKeySegment *segments, size_t segment_count,
                                                  const MeterMap *map, const CommonPracticeKeyRegionsConfig *config,
                                                  CommonPracticeKeyWindows *out) {
    if (out == NULL) {
        return COMMON_PRACTICE_KEY_REGIONS_ERR_NULL_ARGUMENT;
    }
    CommonPracticeKeyRegionsConfig effective =
        config != NULL ? *config : common_practice_key_regions_config_default();
    CommonPracticeKeyRegionsStatus status = validate_inputs(segments, segment_count, map, &effective);
    if (status != COMMON_PRACTICE_KEY_REGIONS_OK) {
        return status;
    }
    MeasureWeights measures;
    status = gather_measures(segments, segment_count, map, &measures);
    if (status != COMMON_PRACTICE_KEY_REGIONS_OK) {
        return status;
    }
    status = score_windows(&measures, &effective, out);
    measures_release(&measures);
    return status;
}

/* --- the region path --- */

#define TIE 1e-12

void common_practice_key_regions_destroy(CommonPracticeKeyRegions *regions) {
    if (regions == NULL) {
        return;
    }
    for (size_t i = 0; i < regions->region_count; i++) {
        key_destroy(regions->regions[i].key);
        time_span_destroy(regions->regions[i].span);
    }
    free(regions->regions);
    free(regions->changes);
    regions->regions = NULL;
    regions->changes = NULL;
    regions->region_count = 0;
    regions->change_count = 0;
}

/* The least-cost key per window (see key_regions.h). */
static CommonPracticeKeyRegionsStatus choose_path(const CommonPracticeKeyWindows *windows, double penalty,
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
        cost[0][k] = -windows->items[0].correlation[k];
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
                cost[w][k] = cost[w - 1][k] - windows->items[w].correlation[k];
                from[w][k] = k;
            } else {
                cost[w][k] = change - windows->items[w].correlation[k];
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

static CommonPracticeKeyRegionsStatus build_region(const CommonPracticeKeyRegionsConfig *config, size_t number,
                                                    int key_index, const MeterMap *map, int first_measure,
                                                    int last_measure, double uncertainty,
                                                    CommonPracticeKeyRegion *out) {
    memset(out, 0, sizeof(*out));
    out->key_index = key_index;
    out->first_measure = first_measure;
    out->last_measure = last_measure;
    out->uncertainty = uncertainty;
    out->source = COMMON_PRACTICE_REGION_SOURCE_PROFILE;

    TimeSpan *first = NULL;
    TimeSpan *last = NULL;
    if (meter_map_measure_span(map, first_measure, &first) != METER_MAP_OK) {
        return COMMON_PRACTICE_KEY_REGIONS_ERR_METER_MAP;
    }
    if (meter_map_measure_span(map, last_measure, &last) != METER_MAP_OK) {
        time_span_destroy(first);
        return COMMON_PRACTICE_KEY_REGIONS_ERR_METER_MAP;
    }
    TimeSpanResult spanned = time_span_create(time_span_start(first), time_span_end(last),
                                              TIME_SPAN_BOUNDARY_LEFT_CLOSED, &out->span);
    time_span_destroy(first);
    time_span_destroy(last);
    if (spanned != TIME_SPAN_OK) {
        return COMMON_PRACTICE_KEY_REGIONS_ERR_ALLOC;
    }

    char context[160];
    snprintf(context, sizeof(context), "%s-%zu", config->context_id_prefix, number);
    EntityID *context_id = NULL;
    if (entity_id_create(context, &context_id) != ENTITY_ID_OK) {
        time_span_destroy(out->span);
        out->span = NULL;
        return COMMON_PRACTICE_KEY_REGIONS_ERR_ALLOC;
    }
    KeyResult built = common_practice_build_key(common_practice_key_index_mode(key_index),
                                                common_practice_key_index_tonic_name(key_index), context_id, &out->key);
    if (built != KEY_OK) {
        entity_id_destroy(context_id);
        time_span_destroy(out->span);
        out->span = NULL;
        return built == KEY_ERR_ALLOC ? COMMON_PRACTICE_KEY_REGIONS_ERR_ALLOC : COMMON_PRACTICE_KEY_REGIONS_ERR_KEY;
    }
    return COMMON_PRACTICE_KEY_REGIONS_OK;
}

static double clamp01(double value) {
    return value < 0.0 ? 0.0 : (value > 1.0 ? 1.0 : value);
}

/* 1 - (r(chosen) - r(best other)) for the pooled weights of measures [from, to] (slots in `measures`). */
static double region_uncertainty(const MeasureWeights *measures, size_t from, size_t to, int key_index,
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
        rotated_profile(profiles, k, profile);
        double r = correlate(sum, profile);
        if (k == key_index) {
            chosen = r;
        } else if (r > other) {
            other = r;
        }
    }
    return clamp01(1.0 - (chosen - other));
}

/* The pitch classes of a key's natural collection as a 12-bit mask: the major scale, or the relative major's
   for a minor key. */
static unsigned natural_collection(int key_index) {
    static const int MAJOR_SCALE[7] = {0, 2, 4, 5, 7, 9, 11};
    int tonic = common_practice_key_index_tonic(key_index);
    if (common_practice_key_index_mode(key_index) == COMMON_PRACTICE_KEY_MINOR) {
        tonic = (tonic + 3) % 12;
    }
    unsigned mask = 0;
    for (int i = 0; i < 7; i++) {
        mask |= 1u << ((tonic + MAJOR_SCALE[i]) % 12);
    }
    return mask;
}

/* The pivot of one change (see key_regions.h): the last segment at or before the boundary, within the search
   distance, whose best chord in the old key lies inside the new key's collection. */
static CommonPracticeKeyRegionsStatus find_pivot(const CommonPracticeKeySegment *segments, size_t segment_count,
                                                  const MeasureWeights *measures, int boundary_measure,
                                                  const MusicalTime *boundary_start, int old_key, int new_key,
                                                  size_t search_measures, bool *found, size_t *pivot) {
    *found = false;
    int old_tonic = common_practice_key_index_tonic(old_key);
    if (common_practice_key_index_mode(old_key) == COMMON_PRACTICE_KEY_MINOR) {
        old_tonic = (old_tonic + 3) % 12;
    }
    unsigned new_collection = natural_collection(new_key);
    for (size_t i = segment_count; i-- > 0;) {
        int measure = measures->segment_measure[i];
        if (measure > boundary_measure || before(boundary_start, segments[i].start)) {
            continue; /* after the boundary */
        }
        if ((size_t)(boundary_measure - measure) > search_measures) {
            break; /* too far back, and the segments are in time order */
        }
        if (segments[i].source == NULL) {
            continue;
        }
        CommonPracticeWeightedResult result;
        CommonPracticeWeightedStatus identified =
            common_practice_identify_weighted(segments[i].source, old_tonic, COMMON_PRACTICE_KEY_MAJOR, NULL, &result);
        if (identified == COMMON_PRACTICE_WEIGHTED_ERR_ALLOC) {
            return COMMON_PRACTICE_KEY_REGIONS_ERR_ALLOC;
        }
        if (identified != COMMON_PRACTICE_WEIGHTED_OK) {
            continue;
        }
        bool diatonic = false;
        if (result.matched) {
            unsigned mask = common_practice_ranked_chord_mask(&result.best, old_tonic);
            diatonic = mask != 0 && (mask & ~new_collection) == 0;
        }
        common_practice_weighted_result_destroy(&result);
        if (diatonic) {
            *found = true;
            *pivot = i;
            return COMMON_PRACTICE_KEY_REGIONS_OK;
        }
    }
    return COMMON_PRACTICE_KEY_REGIONS_OK;
}

CommonPracticeKeyRegionsStatus key_regions_estimate(const CommonPracticeKeySegment *segments, size_t segment_count,
                                                     const MeterMap *map, const CommonPracticeKeyRegionsConfig *config,
                                                     CommonPracticeKeyRegions *out) {
    if (out == NULL) {
        return COMMON_PRACTICE_KEY_REGIONS_ERR_NULL_ARGUMENT;
    }
    CommonPracticeKeyRegionsConfig effective =
        config != NULL ? *config : common_practice_key_regions_config_default();
    CommonPracticeKeyRegionsStatus status = validate_inputs(segments, segment_count, map, &effective);
    if (status != COMMON_PRACTICE_KEY_REGIONS_OK) {
        return status;
    }
    if (!isfinite(effective.change_penalty) || effective.change_penalty < 0.0 ||
        effective.tonicization_max_measures == 0 || effective.context_id_prefix == NULL ||
        effective.context_id_prefix[0] == '\0' || strlen(effective.context_id_prefix) > 100) {
        return COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_CONFIG;
    }

    MeasureWeights measures;
    status = gather_measures(segments, segment_count, map, &measures);
    if (status != COMMON_PRACTICE_KEY_REGIONS_OK) {
        return status;
    }
    CommonPracticeKeyWindows windows = {0};
    status = score_windows(&measures, &effective, &windows);
    if (status != COMMON_PRACTICE_KEY_REGIONS_OK) {
        measures_release(&measures);
        return status;
    }

    int *path = (int *)calloc(windows.count, sizeof(int));
    int *measure_key = (int *)calloc(measures.measure_count, sizeof(int));
    if (path == NULL || measure_key == NULL) {
        status = COMMON_PRACTICE_KEY_REGIONS_ERR_ALLOC;
    } else {
        status = choose_path(&windows, effective.change_penalty, path);
    }
    CommonPracticeKeyRegions result = {NULL, 0, NULL, 0};
    if (status == COMMON_PRACTICE_KEY_REGIONS_OK) {
        /* window w labels the measure at its centre; the ends take the edge windows' keys */
        size_t window_length = (size_t)(windows.items[0].last_measure - windows.items[0].first_measure) + 1;
        size_t half = window_length / 2;
        for (size_t m = 0; m < measures.measure_count; m++) {
            size_t w = m < half ? 0 : m - half;
            measure_key[m] = path[w < windows.count ? w : windows.count - 1];
        }

        size_t region_count = 1;
        for (size_t m = 1; m < measures.measure_count; m++) {
            region_count += measure_key[m] != measure_key[m - 1] ? 1 : 0;
        }
        result.regions = (CommonPracticeKeyRegion *)calloc(region_count, sizeof(CommonPracticeKeyRegion));
        result.changes = (CommonPracticeKeyChange *)calloc(region_count > 1 ? region_count - 1 : 1,
                                                           sizeof(CommonPracticeKeyChange));
        if (result.regions == NULL || result.changes == NULL) {
            status = COMMON_PRACTICE_KEY_REGIONS_ERR_ALLOC;
        }
        size_t from = 0;
        for (size_t m = 1; m <= measures.measure_count && status == COMMON_PRACTICE_KEY_REGIONS_OK; m++) {
            if (m < measures.measure_count && measure_key[m] == measure_key[from]) {
                continue;
            }
            double uncertainty =
                region_uncertainty(&measures, from, m - 1, measure_key[from], &effective.profiles);
            status = build_region(&effective, result.region_count + 1, measure_key[from], map,
                                  measures.first_measure + (int)from, measures.first_measure + (int)m - 1,
                                  uncertainty, &result.regions[result.region_count]);
            if (status == COMMON_PRACTICE_KEY_REGIONS_OK) {
                result.region_count++;
            }
            from = m;
        }
    }
    if (status == COMMON_PRACTICE_KEY_REGIONS_OK) {
        /* the extent of each change, by the length and return of the region it goes into */
        size_t n = result.region_count;
        for (size_t i = 1; i < n; i++) {
            CommonPracticeKeyChange *change = &result.changes[i - 1];
            change->from_region = i - 1;
            change->to_region = i;
            change->cadence_confirmed = false;
            const CommonPracticeKeyRegion *into = &result.regions[i];
            bool goes_back = i + 1 < n && result.regions[i + 1].key_index == result.regions[i - 1].key_index;
            size_t length = (size_t)(into->last_measure - into->first_measure) + 1;
            if (goes_back && length < effective.tonicization_max_measures) {
                change->extent = KEY_CHANGE_EXTENT_TONICIZATION;
            } else if (goes_back) {
                change->extent = KEY_CHANGE_EXTENT_WEAK_MODULATION;
            } else {
                change->extent = KEY_CHANGE_EXTENT_MODULATION;
            }
            change->is_return = false;
        }
        for (size_t i = 2; i < n; i++) {
            /* the change out of an excursion back to the key before it */
            if (result.regions[i].key_index == result.regions[i - 2].key_index) {
                CommonPracticeKeyChange *back = &result.changes[i - 1];
                back->is_return = true;
                back->extent = result.changes[i - 2].extent;
            }
        }
        result.change_count = n - 1;

        for (size_t i = 0; i < result.change_count && status == COMMON_PRACTICE_KEY_REGIONS_OK; i++) {
            CommonPracticeKeyChange *change = &result.changes[i];
            change->has_pivot = false;
            change->pivot_segment = 0;
            if (!key_change_extent_is_modulation(change->extent)) {
                continue;
            }
            const CommonPracticeKeyRegion *into = &result.regions[change->to_region];
            status = find_pivot(segments, segment_count, &measures, into->first_measure,
                                time_span_start(into->span), result.regions[change->from_region].key_index,
                                into->key_index, effective.pivot_search_measures, &change->has_pivot,
                                &change->pivot_segment);
        }
    }

    free(path);
    free(measure_key);
    common_practice_key_windows_destroy(&windows);
    measures_release(&measures);
    if (status != COMMON_PRACTICE_KEY_REGIONS_OK) {
        common_practice_key_regions_destroy(&result);
        return status;
    }
    *out = result;
    return COMMON_PRACTICE_KEY_REGIONS_OK;
}
