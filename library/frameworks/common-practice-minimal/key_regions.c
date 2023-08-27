#include "key_regions.h"

#include <math.h>
#include <stdlib.h>
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

CommonPracticeKeyRegionsStatus key_window_scores(const CommonPracticeKeySegment *segments, size_t segment_count,
                                                  const MeterMap *map, const CommonPracticeKeyRegionsConfig *config,
                                                  CommonPracticeKeyWindows *out) {
    if (out == NULL || map == NULL || (segment_count > 0 && segments == NULL)) {
        return COMMON_PRACTICE_KEY_REGIONS_ERR_NULL_ARGUMENT;
    }
    if (segment_count == 0) {
        return COMMON_PRACTICE_KEY_REGIONS_ERR_NO_SEGMENTS;
    }
    CommonPracticeKeyRegionsConfig effective =
        config != NULL ? *config : common_practice_key_regions_config_default();
    if (effective.window_measures == 0 || !profiles_valid(&effective.profiles)) {
        return COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_CONFIG;
    }

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
    free(measure_of);

    size_t window_length = effective.window_measures < measure_count ? effective.window_measures : measure_count;
    size_t window_count = measure_count - window_length + 1;
    CommonPracticeKeyWindow *windows = (CommonPracticeKeyWindow *)calloc(window_count, sizeof(CommonPracticeKeyWindow));
    if (windows == NULL) {
        free(per_measure);
        return COMMON_PRACTICE_KEY_REGIONS_ERR_ALLOC;
    }
    double profile[COMMON_PRACTICE_KEY_COUNT][12];
    for (int k = 0; k < COMMON_PRACTICE_KEY_COUNT; k++) {
        rotated_profile(&effective.profiles, k, profile[k]);
    }
    for (size_t w = 0; w < window_count; w++) {
        double sum[12] = {0.0};
        for (size_t m = w; m < w + window_length; m++) {
            for (int pc = 0; pc < 12; pc++) {
                sum[pc] += per_measure[m][pc];
            }
        }
        windows[w].first_measure = first_measure + (int)w;
        windows[w].last_measure = first_measure + (int)(w + window_length) - 1;
        for (int k = 0; k < COMMON_PRACTICE_KEY_COUNT; k++) {
            windows[w].correlation[k] = correlate(sum, profile[k]);
        }
    }
    free(per_measure);

    out->items = windows;
    out->count = window_count;
    out->first_measure = first_measure;
    out->last_measure = last_measure;
    return COMMON_PRACTICE_KEY_REGIONS_OK;
}
