#include "analysis/harmony/key_regions_internal.h"

#include <stdlib.h>


void common_practice_key_windows_destroy(CommonPracticeKeyWindows *windows) {
    if (windows == NULL) {
        return;
    }
    free(windows->items);
    windows->items = NULL;
    windows->count = 0;
}

/* Pearson correlation of two 12-vectors; 0 when either is flat. */
double key_regions_correlate(const double x[12], const double y[12]) {
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
void key_regions_rotated_profile(const CommonPracticeKeyProfiles *profiles, int key_index, double out[12]) {
    int tonic = key_index % 12;
    const double *base = key_index >= 12 ? profiles->minor : profiles->major;
    for (int pc = 0; pc < 12; pc++) {
        out[pc] = base[(pc - tonic + 12) % 12];
    }
}

bool key_regions_time_ok(const MusicalTime *time) {
    return time != NULL && musical_time_numerator(time) >= 0;
}

bool key_regions_before(const MusicalTime *a, const MusicalTime *b) {
    /* a < b for non-negative times: a.n * b.d < b.n * a.d */
    return (long double)musical_time_numerator(a) * (long double)musical_time_denominator(b) <
           (long double)musical_time_numerator(b) * (long double)musical_time_denominator(a);
}

#define DOMINANT_THIRD_BIT(root) (1u << (((root) + 4) % 12))

/* The functional evidence for the key on `tonic` (minor when `minor`) in the move from chord `from` (NULL for
   none) to chord `to`: see key_regions.h, FUNCTIONAL EVIDENCE. A tonic chord counts only when its third is the
   key's mode's third, so a dominant motion into a major tonic is evidence for the major key and not its parallel. */
static double chord_evidence(const CommonPracticeKeyChord *from, const CommonPracticeKeyChord *to, int tonic,
                             bool minor) {
    double evidence = 0.0;
    unsigned leading = PITCH_CLASS_BIT(tonic + 11);
    if (from == NULL) {
        return evidence;
    }
    bool tonic_chord = to->root_pitch_class == tonic &&
                       (to->pitch_classes & PITCH_CLASS_BIT(tonic + (minor ? 3 : 4))) != 0;
    if (tonic_chord) {
        if (from->root_pitch_class == (tonic + 7) % 12 &&
            (from->pitch_classes & DOMINANT_THIRD_BIT(from->root_pitch_class)) != 0) {
            evidence += 1.0;
        } else if ((from->pitch_classes & leading) != 0) {
            evidence += 0.5;
        }
    }
    if (minor && (to->pitch_classes & leading) != 0 && (to->pitch_classes & PITCH_CLASS_BIT(tonic + 10)) == 0 &&
        (from->pitch_classes & (PITCH_CLASS_BIT(tonic + 3) | PITCH_CLASS_BIT(tonic + 8))) != 0) {
        evidence += 0.5;
    }
    return evidence;
}

CommonPracticeKeyRegionsStatus key_regions_gather_measures(const CommonPracticeKeySegment *segments, size_t segment_count,
                                                       const MeterMap *map, MeasureWeights *out) {
    int *measure_of = (int *)calloc(segment_count, sizeof(int));
    if (measure_of == NULL) {
        return COMMON_PRACTICE_KEY_REGIONS_ERR_ALLOC;
    }
    int first_measure = 0;
    int last_measure = 0;
    for (size_t i = 0; i < segment_count; i++) {
        const CommonPracticeKeySegment *segment = &segments[i];
        bool bad = !key_regions_time_ok(segment->start) || !key_regions_time_ok(segment->end) || !key_regions_before(segment->start, segment->end) ||
                   !isfinite(segment->weights.total) || segment->weights.total < 0.0 ||
                   (i > 0 && key_regions_before(segment->start, segments[i - 1].start)) ||
                   (segment->chord.present &&
                    (segment->chord.root_pitch_class < 0 || segment->chord.root_pitch_class > 11));
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
    double(*evidence)[COMMON_PRACTICE_KEY_COUNT] =
        (double(*)[COMMON_PRACTICE_KEY_COUNT])calloc(measure_count, sizeof(double[COMMON_PRACTICE_KEY_COUNT]));
    double *chords = (double *)calloc(measure_count, sizeof(double));
    if (evidence == NULL || chords == NULL) {
        free(evidence);
        free(chords);
        free(per_measure);
        free(measure_of);
        return COMMON_PRACTICE_KEY_REGIONS_ERR_ALLOC;
    }
    const CommonPracticeKeyChord *previous = NULL;
    for (size_t i = 0; i < segment_count; i++) {
        size_t slot = (size_t)(measure_of[i] - first_measure);
        for (int pc = 0; pc < 12; pc++) {
            per_measure[slot][pc] += segments[i].weights.pitch_class[pc];
        }
        const CommonPracticeKeyChord *chord = &segments[i].chord;
        if (!chord->present) {
            continue;
        }
        chords[slot] += 1.0;
        for (int k = 0; k < COMMON_PRACTICE_KEY_COUNT; k++) {
            evidence[slot][k] += chord_evidence(previous, chord, k % 12, k >= 12);
        }
        previous = chord;
    }
    out->segment_measure = measure_of;
    out->weights = per_measure;
    out->measure_count = measure_count;
    out->first_measure = first_measure;
    out->last_measure = last_measure;
    out->evidence = evidence;
    out->chords = chords;
    return COMMON_PRACTICE_KEY_REGIONS_OK;
}

void key_regions_measures_release(MeasureWeights *measures) {
    free(measures->segment_measure);
    free(measures->weights);
    free(measures->evidence);
    free(measures->chords);
}

CommonPracticeKeyRegionsStatus key_regions_validate_inputs(const CommonPracticeKeySegment *segments, size_t segment_count,
                                                       const MeterMap *map, const CommonPracticeKeyRegionsConfig *config) {
    if (map == NULL || (segment_count > 0 && segments == NULL)) {
        return COMMON_PRACTICE_KEY_REGIONS_ERR_NULL_ARGUMENT;
    }
    if (segment_count == 0) {
        return COMMON_PRACTICE_KEY_REGIONS_ERR_NO_SEGMENTS;
    }
    if (config->window_measures == 0 || !profiles_valid(&config->profiles) || !isfinite(config->evidence_weight) ||
        config->evidence_weight < 0.0) {
        return COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_CONFIG;
    }
    return COMMON_PRACTICE_KEY_REGIONS_OK;
}

/* Scores the windows of already-gathered measures. */
CommonPracticeKeyRegionsStatus key_regions_score_windows(const MeasureWeights *measures,
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
        key_regions_rotated_profile(&config->profiles, k, profile[k]);
    }
    for (size_t w = 0; w < window_count; w++) {
        double sum[12] = {0.0};
        double window_evidence[COMMON_PRACTICE_KEY_COUNT] = {0.0};
        double window_chords = 0.0;
        for (size_t m = w; m < w + window_length; m++) {
            for (int pc = 0; pc < 12; pc++) {
                sum[pc] += measures->weights[m][pc];
            }
            if (measures->chords != NULL) {
                window_chords += measures->chords[m];
                for (int k = 0; k < COMMON_PRACTICE_KEY_COUNT; k++) {
                    window_evidence[k] += measures->evidence[m][k];
                }
            }
        }
        windows[w].first_measure = measures->first_measure + (int)w;
        windows[w].last_measure = measures->first_measure + (int)(w + window_length) - 1;
        for (int k = 0; k < COMMON_PRACTICE_KEY_COUNT; k++) {
            windows[w].correlation[k] = key_regions_correlate(sum, profile[k]);
            windows[w].evidence[k] =
                window_chords > 0.0 ? config->evidence_weight * window_evidence[k] / window_chords : 0.0;
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
    CommonPracticeKeyRegionsStatus status = key_regions_validate_inputs(segments, segment_count, map, &effective);
    if (status != COMMON_PRACTICE_KEY_REGIONS_OK) {
        return status;
    }
    MeasureWeights measures;
    status = key_regions_gather_measures(segments, segment_count, map, &measures);
    if (status != COMMON_PRACTICE_KEY_REGIONS_OK) {
        return status;
    }
    status = key_regions_score_windows(&measures, &effective, out);
    key_regions_measures_release(&measures);
    return status;
}
