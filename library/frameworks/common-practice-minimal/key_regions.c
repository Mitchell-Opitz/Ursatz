#include "key_regions.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "core/rational.h"
#include "pitch/pitch_arithmetic.h"
#include "time/time_rational.h"
#include "pitch/midi_pitch.h"

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
    config.evidence_weight = 0.8;
    config.phrase_boundaries = NULL;
    config.phrase_boundary_count = 0;
    config.snap_reach_measures = 1;
    config.hold_tolerance = 0.05;
    config.phrases = NULL;
    config.phrase_count = 0;
    config.repeats = NULL;
    config.repeat_count = 0;
    return config;
}

unsigned common_practice_cadence_keys(CommonPracticeCadenceKind kind, const CommonPracticeKeyChord *arrival) {
    if (arrival == NULL || !arrival->present || arrival->root_pitch_class < 0 || arrival->root_pitch_class > 11) {
        return 0;
    }
    int root = arrival->root_pitch_class;
    unsigned tones = arrival->pitch_classes;
    bool major_third = (tones & (1u << ((root + 4) % 12))) != 0;
    bool minor_third = (tones & (1u << ((root + 3) % 12))) != 0;
    if (kind == COMMON_PRACTICE_CADENCE_AUTHENTIC) {
        if (major_third && !minor_third) {
            return 1u << root;
        }
        return minor_third && !major_third ? 1u << (12 + root) : 0;
    }
    if (kind == COMMON_PRACTICE_CADENCE_HALF && major_third && !minor_third && (tones & (1u << ((root + 7) % 12))) != 0) {
        int tonic = (root + 5) % 12; /* the key a fifth below the arrival */
        return (1u << tonic) | (1u << (12 + tonic));
    }
    return 0;
}

CommonPracticeKeyChord common_practice_key_chord_of(const CommonPracticeWeightedResult *result) {
    CommonPracticeKeyChord chord = {false, 0, 0};
    if (result != NULL && result->matched) {
        chord.pitch_classes = common_practice_ranked_chord_mask(&result->best, 0);
        chord.root_pitch_class = result->best.root_pitch_class;
        chord.present = chord.pitch_classes != 0;
    }
    return chord;
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
    return tonic < 0 ? NULL : key_tonic_name(tonic);
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
    double (*evidence)[COMMON_PRACTICE_KEY_COUNT]; /* owned; per measure, per key: the unweighted evidence */
    double *chords;                                 /* owned; per measure: the chords starting in it */
} MeasureWeights;

#define DOMINANT_THIRD_BIT(root) (1u << (((root) + 4) % 12))
#define PITCH_CLASS_BIT(pc) (1u << pitch_class_of_midi(pc))

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
                   (i > 0 && before(segment->start, segments[i - 1].start)) ||
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

static void measures_release(MeasureWeights *measures) {
    free(measures->segment_measure);
    free(measures->weights);
    free(measures->evidence);
    free(measures->chords);
}

static CommonPracticeKeyRegionsStatus validate_inputs(const CommonPracticeKeySegment *segments, size_t segment_count,
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
            windows[w].correlation[k] = correlate(sum, profile[k]);
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

/* The score a path earns for key k in a window: correlation plus functional evidence. */
static double window_score(const CommonPracticeKeyWindow *window, int key) {
    return window->correlation[key] + window->evidence[key];
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

static CommonPracticeKeyRegionsStatus build_region(const CommonPracticeKeyRegionsConfig *config, size_t number,
                                                    int key_index, const MeterMap *map, int first_measure,
                                                    int last_measure, double uncertainty,
                                                    CommonPracticeRegionSource source, CommonPracticeKeyRegion *out) {
    memset(out, 0, sizeof(*out));
    out->key_index = key_index;
    out->first_measure = first_measure;
    out->last_measure = last_measure;
    out->uncertainty = uncertainty;
    out->source = source;

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

/* The least-cost path when the FIRST region (the windows before the first change of key) must be one of the
   `allowed` keys (a bit mask over the 24): a two-phase search, phase 0 = still in that first region. */
#define BIG 1e300

static CommonPracticeKeyRegionsStatus choose_path_restricted(const CommonPracticeKeyWindows *windows, double penalty,
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

/* The spans of the piece the events cut it into: span i starts at measure slot start[i] and its first region
   is restricted to the keys in allowed[i] (0 = unrestricted). */
typedef struct {
    size_t *start;
    unsigned *allowed;
    size_t count;
} SpanPlan;

static unsigned signature_keys(int sharps_flats) {
    int tonic = pitch_class_of_midi(7 * sharps_flats); /* the major key: sharps climb by fifths from C */
    int relative = (tonic + 9) % 12;
    return (1u << tonic) | (1u << (12 + relative));
}

/* The measure whose start is nearest `position`. */
static CommonPracticeKeyRegionsStatus nearest_barline(const MeterMap *map, const MusicalTime *position,
                                                       int *out_measure) {
    int measure = 0;
    TimeSpan *span = NULL;
    if (meter_map_measure_index_at(map, position, &measure) != METER_MAP_OK ||
        meter_map_measure_span(map, measure, &span) != METER_MAP_OK) {
        return COMMON_PRACTICE_KEY_REGIONS_ERR_METER_MAP;
    }
    Rational at;
    Rational start;
    Rational end;
    bool converted = rational_of_musical_time(position, &at) == RATIONAL_OK &&
                     rational_of_musical_time(time_span_start(span), &start) == RATIONAL_OK &&
                     rational_of_musical_time(time_span_end(span), &end) == RATIONAL_OK;
    time_span_destroy(span);
    Rational since = {0, 1};
    Rational until = {0, 1};
    if (!converted || rational_sub(at, start, &since) != RATIONAL_OK || rational_sub(end, at, &until) != RATIONAL_OK) {
        return COMMON_PRACTICE_KEY_REGIONS_ERR_METER_MAP;
    }
    *out_measure = rational_cmp(since, until) > 0 ? measure + 1 : measure;
    return COMMON_PRACTICE_KEY_REGIONS_OK;
}

static CommonPracticeKeyRegionsStatus plan_spans(const CommonPracticeKeySignatureEvent *events, size_t event_count,
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
        if (!time_ok(event->position) || event->sharps_flats < -7 || event->sharps_flats > 7 ||
            (i > 0 && before(event->position, events[i - 1].position))) {
            return COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_EVENT;
        }
        if (have_previous && event->sharps_flats == previous_sharps) {
            continue; /* the same signature again: not a change */
        }
        have_previous = true;
        previous_sharps = event->sharps_flats;
        int measure = 0;
        CommonPracticeKeyRegionsStatus snapped = nearest_barline(map, event->position, &measure);
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

/* True when the key `to` is closely related to `from`: the dominant or subdominant in the same mode, or the
   relative major or minor. */
static bool closely_related(int from, int to) {
    int from_tonic = common_practice_key_index_tonic(from);
    int to_tonic = common_practice_key_index_tonic(to);
    CommonPracticeKeyMode from_mode = common_practice_key_index_mode(from);
    CommonPracticeKeyMode to_mode = common_practice_key_index_mode(to);
    int step = pitch_class_of_midi(to_tonic - from_tonic);
    if (from_mode == to_mode) {
        return step == 5 || step == 7;
    }
    return from_mode == COMMON_PRACTICE_KEY_MAJOR ? step == 9 : step == 3;
}

/* The tones a key's diatonic chords are made of: its natural collection, plus the raised seventh in minor. */
static unsigned held_collection(int key_index) {
    unsigned mask = natural_collection(key_index);
    if (common_practice_key_index_mode(key_index) == COMMON_PRACTICE_KEY_MINOR) {
        mask |= PITCH_CLASS_BIT(common_practice_key_index_tonic(key_index) + 11);
    }
    return mask;
}

/* Folds a region back into the key before it when it is a passage of IV, V and ii of that key (see key_regions.h,
   HOME KEY HELD). */
static void hold_home_keys(const CommonPracticeKeySegment *segments, size_t segment_count,
                           const MeasureWeights *measures, const bool *fixed, double tolerance, int *measure_key) {
    size_t from = 1;
    while (from < measures->measure_count) {
        if (measure_key[from] == measure_key[from - 1]) {
            from++;
            continue;
        }
        size_t to = from + 1;
        while (to < measures->measure_count && measure_key[to] == measure_key[from]) {
            to++;
        }
        int old_key = measure_key[from - 1];
        int tonic = common_practice_key_index_tonic(old_key);
        unsigned collection = held_collection(old_key);
        bool hold = !fixed[from] && closely_related(old_key, measure_key[from]);
        size_t chords = 0;
        for (size_t i = 0; i < segment_count && hold; i++) {
            size_t slot = (size_t)(measures->segment_measure[i] - measures->first_measure);
            if (slot < from || slot >= to || !segments[i].chord.present) {
                continue;
            }
            int root = segments[i].chord.root_pitch_class;
            int degree = pitch_class_of_midi(root - tonic);
            hold = (degree == 2 || degree == 5 || degree == 7) && (segments[i].chord.pitch_classes & ~collection) == 0;
            chords++;
        }
        double total = 0.0;
        double outside = 0.0;
        for (size_t m = from; m < to && hold; m++) {
            for (int pc = 0; pc < 12; pc++) {
                total += measures->weights[m][pc];
                outside += ((collection >> pc) & 1u) ? 0.0 : measures->weights[m][pc];
            }
        }
        hold = hold && chords > 0 && outside <= tolerance * total;
        if (hold) {
            for (size_t m = from; m < to; m++) {
                measure_key[m] = old_key;
            }
        }
        from = to;
    }
}

/* Moves each edge of the measure keys (a measure whose key differs from the one before it) to the nearest boundary
   slot within `reach` measures, never onto or past a neighbouring edge, and never an edge marked fixed. */
static CommonPracticeKeyRegionsStatus snap_edges(int *measure_key, const bool *fixed, size_t measure_count,
                                                  const size_t *boundaries, size_t boundary_count, size_t reach) {
    if (boundary_count == 0 || measure_count < 2) {
        return COMMON_PRACTICE_KEY_REGIONS_OK;
    }
    size_t edge_count = 0;
    for (size_t m = 1; m < measure_count; m++) {
        edge_count += measure_key[m] != measure_key[m - 1] ? 1 : 0;
    }
    if (edge_count == 0) {
        return COMMON_PRACTICE_KEY_REGIONS_OK;
    }
    size_t *edge = (size_t *)calloc(edge_count, sizeof(size_t));
    int *before_key = (int *)calloc(edge_count, sizeof(int));
    int *after_key = (int *)calloc(edge_count, sizeof(int));
    if (edge == NULL || before_key == NULL || after_key == NULL) {
        free(edge);
        free(before_key);
        free(after_key);
        return COMMON_PRACTICE_KEY_REGIONS_ERR_ALLOC;
    }
    size_t next = 0;
    for (size_t m = 1; m < measure_count; m++) {
        if (measure_key[m] != measure_key[m - 1]) {
            edge[next] = m;
            before_key[next] = measure_key[m - 1];
            after_key[next] = measure_key[m];
            next++;
        }
    }
    size_t previous = 0; /* the position of the edge before this one, after its own move; 0 = the start */
    for (size_t i = 0; i < edge_count; i++) {
        size_t at = edge[i];
        size_t next_edge = i + 1 < edge_count ? edge[i + 1] : measure_count;
        size_t target = at;
        if (!fixed[at]) {
            size_t best_distance = reach + 1;
            for (size_t b = 0; b < boundary_count; b++) {
                size_t slot = boundaries[b];
                if (slot <= previous || slot >= next_edge) {
                    continue;
                }
                size_t distance = slot > at ? slot - at : at - slot;
                if (distance < best_distance || (distance == best_distance && slot < target)) {
                    best_distance = distance;
                    target = slot;
                }
            }
        }
        if (target < at) {
            for (size_t m = target; m < at; m++) {
                measure_key[m] = after_key[i];
            }
        } else if (target > at) {
            for (size_t m = at; m < target; m++) {
                measure_key[m] = before_key[i];
            }
        }
        previous = target;
    }
    free(edge);
    free(before_key);
    free(after_key);
    return COMMON_PRACTICE_KEY_REGIONS_OK;
}

/* --- keys on phrases (see key_regions.h, KEYS ON PHRASES) --- */

/* A phrase as measure slots (0 = the first measure of the piece), with what the path needs of it. */
typedef struct {
    size_t first;
    size_t last;
    unsigned cadence_keys;
    unsigned arrival_keys;
    bool closes;
    bool span_start;   /* starts a key signature span after the first */
    unsigned allowed;  /* the keys it may take (0 = all): its span's two keys when it starts a restricted span */
} PhraseSlot;

/* A path state is (key, satisfied, changed here); `satisfied`: the region holds a phrase that confirms its key. */
#define PHRASE_STATES (COMMON_PRACTICE_KEY_COUNT * 4)
#define STATE(key, satisfied, changed) ((size_t)(((key) * 2 + (satisfied)) * 2 + (changed)))

/* The phrases clipped to the measures, closed up and split at the key signature span starts. Returns the count in *out_count. */
static CommonPracticeKeyRegionsStatus build_phrase_slots(const CommonPracticeKeyRegionsConfig *config,
                                                         const MeasureWeights *measures, const SpanPlan *plan,
                                                         PhraseSlot **out, size_t *out_count) {
    long measure_count = (long)measures->measure_count;
    PhraseSlot *clipped = (PhraseSlot *)calloc(config->phrase_count + 1, sizeof(PhraseSlot));
    PhraseSlot *split = (PhraseSlot *)calloc(config->phrase_count + plan->count + 1, sizeof(PhraseSlot));
    if (clipped == NULL || split == NULL) {
        free(clipped);
        free(split);
        return COMMON_PRACTICE_KEY_REGIONS_ERR_ALLOC;
    }
    size_t count = 0;
    for (size_t i = 0; i < config->phrase_count; i++) {
        const CommonPracticeKeyPhrase *phrase = &config->phrases[i];
        long first = (long)phrase->first_measure - (long)measures->first_measure;
        long last = (long)phrase->last_measure - (long)measures->first_measure;
        if (last < 0 || first >= measure_count) {
            continue; /* outside the piece */
        }
        clipped[count].first = first < 0 ? 0 : (size_t)first;
        clipped[count].last = last >= measure_count ? (size_t)measure_count - 1 : (size_t)last;
        clipped[count].cadence_keys = phrase->cadence_keys;
        clipped[count].arrival_keys = phrase->arrival_keys;
        clipped[count].closes = phrase->closes;
        count++;
    }
    if (count == 0) { /* no phrase in the piece: the whole piece is one */
        clipped[0].last = (size_t)measure_count - 1;
        count = 1;
    }
    clipped[0].first = 0;
    for (size_t i = 1; i < count; i++) {
        clipped[i].first = clipped[i - 1].last + 1;
    }
    clipped[count - 1].last = (size_t)measure_count - 1;

    size_t total = 0;
    for (size_t i = 0; i < count; i++) {
        PhraseSlot part = clipped[i];
        for (size_t sp = 1; sp < plan->count; sp++) {
            size_t start = plan->start[sp];
            if (start > part.first && start <= part.last) {
                PhraseSlot before_part = part;
                before_part.last = start - 1;
                before_part.cadence_keys = 0;
                before_part.arrival_keys = 0;
                before_part.closes = false;
                split[total++] = before_part;
                part.first = start;
            }
        }
        split[total++] = part;
    }
    for (size_t i = 0; i < total; i++) {
        split[i].span_start = false;
        split[i].allowed = i == 0 ? plan->allowed[0] : 0;
        for (size_t sp = 1; sp < plan->count; sp++) {
            if (split[i].first == plan->start[sp]) {
                split[i].span_start = true;
                split[i].allowed = plan->allowed[sp];
            }
        }
    }
    free(clipped);
    *out = split;
    *out_count = total;
    return COMMON_PRACTICE_KEY_REGIONS_OK;
}

/* The score of every key for every phrase: the correlation of the phrase's pooled weights with the key's profile plus evidence_weight
   times the phrase's functional evidence over its chord count. */
static void phrase_scores(const PhraseSlot *phrases, size_t phrase_count, const MeasureWeights *measures,
                          const CommonPracticeKeyRegionsConfig *config, double (*score)[COMMON_PRACTICE_KEY_COUNT]) {
    double profile[COMMON_PRACTICE_KEY_COUNT][12];
    for (int k = 0; k < COMMON_PRACTICE_KEY_COUNT; k++) {
        rotated_profile(&config->profiles, k, profile[k]);
    }
    for (size_t p = 0; p < phrase_count; p++) {
        double sum[12] = {0.0};
        double evidence[COMMON_PRACTICE_KEY_COUNT] = {0.0};
        double chords = 0.0;
        for (size_t m = phrases[p].first; m <= phrases[p].last; m++) {
            for (int pc = 0; pc < 12; pc++) {
                sum[pc] += measures->weights[m][pc];
            }
            chords += measures->chords[m];
            for (int k = 0; k < COMMON_PRACTICE_KEY_COUNT; k++) {
                evidence[k] += measures->evidence[m][k];
            }
        }
        for (int k = 0; k < COMMON_PRACTICE_KEY_COUNT; k++) {
            score[p][k] = correlate(sum, profile[k]) + (chords > 0.0 ? config->evidence_weight * evidence[k] / chords : 0.0);
        }
    }
}

static bool has_key(unsigned keys, int key) {
    return ((keys >> key) & 1u) != 0;
}

/* The least-cost key per phrase (see key_regions.h, KEYS ON PHRASES). */
static CommonPracticeKeyRegionsStatus choose_phrase_path(const PhraseSlot *phrases, size_t phrase_count,
                                                         const double (*score)[COMMON_PRACTICE_KEY_COUNT], double penalty,
                                                         int *path) {
    size_t cells = phrase_count * PHRASE_STATES;
    double *cost = (double *)malloc(cells * sizeof(double));
    int *from = (int *)malloc(cells * sizeof(int));
    if (cost == NULL || from == NULL) {
        free(cost);
        free(from);
        return COMMON_PRACTICE_KEY_REGIONS_ERR_ALLOC;
    }
    for (size_t c = 0; c < cells; c++) {
        cost[c] = BIG;
        from[c] = -1;
    }
    for (int k = 0; k < COMMON_PRACTICE_KEY_COUNT; k++) {
        if (phrases[0].allowed == 0 || has_key(phrases[0].allowed, k)) {
            cost[STATE(k, 1, 0)] = -score[0][k]; /* the first region of the piece is satisfied */
        }
    }
    for (size_t p = 1; p < phrase_count; p++) {
        const PhraseSlot *phrase = &phrases[p];
        const double *previous = &cost[(p - 1) * PHRASE_STATES];
        double *current = &cost[p * PHRASE_STATES];
        int *back = &from[p * PHRASE_STATES];
        bool free_change = phrase->span_start || phrases[p - 1].closes;
        for (int pass = 0; pass < 2; pass++) { /* staying first, so a tie keeps the earlier key */
            for (int j = 0; j < COMMON_PRACTICE_KEY_COUNT; j++) {
                for (int satisfied = 0; satisfied < 2; satisfied++) {
                    for (int changed = 0; changed < 2; changed++) {
                        double before_cost = previous[STATE(j, satisfied, changed)];
                        if (before_cost >= BIG) {
                            continue;
                        }
                        for (int k = 0; k < COMMON_PRACTICE_KEY_COUNT; k++) {
                            if ((pass == 0) != (j == k) || (phrase->allowed != 0 && !has_key(phrase->allowed, k))) {
                                continue;
                            }
                            size_t into;
                            double total;
                            if (j == k) {
                                /* a change into k at phrase p - 1 belongs at the arrival when that phrase's own cadence arrives in k
                                   and the phrase before it did not */
                                if (changed && !phrases[p - 1].span_start && has_key(phrases[p - 1].arrival_keys, k) &&
                                    !(p >= 2 && has_key(phrases[p - 2].arrival_keys, k))) {
                                    continue;
                                }
                                into = STATE(k, (satisfied || has_key(phrase->cadence_keys, k)) ? 1 : 0, 0);
                                total = before_cost - score[p][k];
                            } else {
                                if (!satisfied && !phrase->span_start) {
                                    continue; /* a change needs the region before satisfied */
                                }
                                into = STATE(k, (free_change || has_key(phrase->cadence_keys, k)) ? 1 : 0, 1);
                                total = before_cost + (free_change ? 0.0 : penalty) - score[p][k];
                            }
                            if (current[into] >= BIG || total < current[into] - TIE) {
                                current[into] = total;
                                back[into] = (int)STATE(j, satisfied, changed);
                            }
                        }
                    }
                }
            }
        }
    }
    const double *last = &cost[(phrase_count - 1) * PHRASE_STATES];
    int end = -1;
    for (int want_satisfied = 1; want_satisfied >= 0 && end < 0; want_satisfied--) {
        for (int k = 0; k < COMMON_PRACTICE_KEY_COUNT; k++) {
            for (int satisfied = 0; satisfied < 2; satisfied++) {
                for (int changed = 0; changed < 2; changed++) {
                    size_t state = STATE(k, satisfied, changed);
                    if (last[state] >= BIG || (want_satisfied && !satisfied)) {
                        continue;
                    }
                    if (end < 0 || last[state] < last[end] - TIE) {
                        end = (int)state;
                    }
                }
            }
        }
    }
    CommonPracticeKeyRegionsStatus status = COMMON_PRACTICE_KEY_REGIONS_OK;
    if (end < 0) {
        status = COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_CONFIG;
    }
    for (size_t p = phrase_count; p-- > 0 && end >= 0;) {
        path[p] = end / 4;
        end = from[p * PHRASE_STATES + (size_t)end];
    }
    free(cost);
    free(from);
    return status;
}

/* Places the keys on the phrases: fills measure_key per measure slot, marks arrival slots and the first region of each restricted span
   (forced). */
static CommonPracticeKeyRegionsStatus place_keys_on_phrases(const CommonPracticeKeyRegionsConfig *config,
                                                            const MeasureWeights *measures, const SpanPlan *plan,
                                                            int *measure_key, bool *forced, bool *arrival_slot) {
    PhraseSlot *phrases = NULL;
    size_t phrase_count = 0;
    CommonPracticeKeyRegionsStatus status = build_phrase_slots(config, measures, plan, &phrases, &phrase_count);
    if (status != COMMON_PRACTICE_KEY_REGIONS_OK) {
        return status;
    }
    double(*score)[COMMON_PRACTICE_KEY_COUNT] =
        (double(*)[COMMON_PRACTICE_KEY_COUNT])calloc(phrase_count, sizeof(double[COMMON_PRACTICE_KEY_COUNT]));
    int *path = (int *)calloc(phrase_count, sizeof(int));
    bool *first_region = (bool *)calloc(measures->measure_count, sizeof(bool));
    if (score == NULL || path == NULL || first_region == NULL) {
        status = COMMON_PRACTICE_KEY_REGIONS_ERR_ALLOC;
    } else {
        phrase_scores(phrases, phrase_count, measures, config, score);
        status = choose_phrase_path(phrases, phrase_count, (const double(*)[COMMON_PRACTICE_KEY_COUNT])score,
                                    config->change_penalty, path);
    }
    if (status == COMMON_PRACTICE_KEY_REGIONS_OK) {
        for (size_t p = 0; p < phrase_count; p++) {
            for (size_t m = phrases[p].first; m <= phrases[p].last; m++) {
                measure_key[m] = path[p];
            }
        }
        /* the last measure of a phrase whose cadence arrives in the next phrase's key takes that key */
        for (size_t p = 0; p + 1 < phrase_count; p++) {
            if (path[p + 1] != path[p] && has_key(phrases[p].arrival_keys, path[p + 1]) && !phrases[p + 1].span_start) {
                measure_key[phrases[p].last] = path[p + 1];
                arrival_slot[phrases[p].last] = true;
            }
        }
        for (size_t sp = 0; sp < plan->count; sp++) {
            size_t end = sp + 1 < plan->count ? plan->start[sp + 1] : measures->measure_count;
            forced[plan->start[sp]] = plan->allowed[sp] != 0;
            for (size_t m = plan->start[sp]; plan->allowed[sp] != 0 && m < end && measure_key[m] == measure_key[plan->start[sp]]; m++) {
                first_region[m] = true; /* the first region of a restricted span */
            }
        }
        for (size_t r = 0; r < config->repeat_count; r++) {
            const CommonPracticeKeyRepeat *repeat = &config->repeats[r];
            for (int t = 0; t < repeat->measure_count; t++) {
                long earlier = (long)repeat->earlier_first + t - (long)measures->first_measure;
                long later = (long)repeat->later_first + t - (long)measures->first_measure;
                if (earlier < 0 || later < 0 || (size_t)earlier >= measures->measure_count ||
                    (size_t)later >= measures->measure_count || first_region[earlier]) {
                    continue;
                }
                measure_key[later] = measure_key[earlier];
                arrival_slot[later] = arrival_slot[earlier];
            }
        }
    }
    free(first_region);
    free(path);
    free(score);
    free(phrases);
    return status;
}

/* The pivot of one change (see key_regions.h): the last segment at or before the boundary, within the search
   distance, whose best chord in the old key lies inside the new key's collection. */
static CommonPracticeKeyRegionsStatus find_pivot(const CommonPracticeKeySegment *segments, size_t segment_count,
                                                  const MeasureWeights *measures, int boundary_measure,
                                                  const MusicalTime *boundary_start, int old_key, int new_key,
                                                  size_t search_measures, bool *found, size_t *pivot) {
    *found = false;
    int old_tonic = common_practice_key_index_tonic(old_key);
    CommonPracticeKeyMode old_mode = common_practice_key_index_mode(old_key);
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
            common_practice_identify_weighted(segments[i].source, old_tonic, old_mode, NULL, &result);
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
                                                     const MeterMap *map,
                                                     const CommonPracticeKeySignatureEvent *events, size_t event_count,
                                                     const CommonPracticeKeyRegionsConfig *config,
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
    if (!isfinite(effective.change_penalty) || effective.change_penalty < 0.0 || !isfinite(effective.hold_tolerance) ||
        effective.hold_tolerance < 0.0 ||
        effective.tonicization_max_measures == 0 || effective.context_id_prefix == NULL ||
        effective.context_id_prefix[0] == '\0' || strlen(effective.context_id_prefix) > 100 ||
        (effective.phrase_boundary_count > 0 && effective.phrase_boundaries == NULL) ||
        (effective.phrase_count > 0 && effective.phrases == NULL) ||
        (effective.repeat_count > 0 && effective.repeats == NULL)) {
        return COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_CONFIG;
    }
    for (size_t p = 0; p < effective.phrase_count; p++) {
        if (effective.phrases[p].last_measure < effective.phrases[p].first_measure ||
            (p > 0 && effective.phrases[p].first_measure <= effective.phrases[p - 1].last_measure)) {
            return COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_CONFIG;
        }
    }
    for (size_t b = 0; b < effective.phrase_boundary_count; b++) {
        if (!time_ok(effective.phrase_boundaries[b])) {
            return COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_EVENT;
        }
    }

    MeasureWeights measures;
    status = gather_measures(segments, segment_count, map, &measures);
    if (status != COMMON_PRACTICE_KEY_REGIONS_OK) {
        return status;
    }
    int *measure_key = (int *)calloc(measures.measure_count, sizeof(int));
    bool *forced = (bool *)calloc(measures.measure_count, sizeof(bool));
    bool *arrival_slot = (bool *)calloc(measures.measure_count, sizeof(bool));
    bool phrase_mode = effective.phrase_count > 0;
    SpanPlan plan = {NULL, NULL, 0};
    if (measure_key == NULL || forced == NULL || arrival_slot == NULL) {
        status = COMMON_PRACTICE_KEY_REGIONS_ERR_ALLOC;
    } else {
        status = plan_spans(events, event_count, map, &measures, &plan);
    }
    if (phrase_mode && status == COMMON_PRACTICE_KEY_REGIONS_OK) {
        status = place_keys_on_phrases(&effective, &measures, &plan, measure_key, forced, arrival_slot);
    }
    for (size_t sp = 0; sp < plan.count && status == COMMON_PRACTICE_KEY_REGIONS_OK && !phrase_mode; sp++) {
        size_t from_slot = plan.start[sp];
        size_t to_slot = sp + 1 < plan.count ? plan.start[sp + 1] : measures.measure_count;
        MeasureWeights view = {NULL, measures.weights + from_slot, to_slot - from_slot,
                               measures.first_measure + (int)from_slot, measures.first_measure + (int)to_slot - 1,
                               measures.evidence + from_slot, measures.chords + from_slot};
        CommonPracticeKeyWindows windows = {0};
        status = score_windows(&view, &effective, &windows);
        if (status != COMMON_PRACTICE_KEY_REGIONS_OK) {
            break;
        }
        int *path = (int *)calloc(windows.count, sizeof(int));
        if (path == NULL) {
            status = COMMON_PRACTICE_KEY_REGIONS_ERR_ALLOC;
        } else if (plan.allowed[sp] != 0) {
            status = choose_path_restricted(&windows, effective.change_penalty, plan.allowed[sp], path);
        } else {
            status = choose_path(&windows, effective.change_penalty, path);
        }
        if (status == COMMON_PRACTICE_KEY_REGIONS_OK) {
            /* window w labels the measure at its centre; the ends take the edge windows' keys */
            size_t window_length = (size_t)(windows.items[0].last_measure - windows.items[0].first_measure) + 1;
            size_t half = window_length / 2;
            for (size_t m = 0; m < view.measure_count; m++) {
                size_t w = m < half ? 0 : m - half;
                measure_key[from_slot + m] = path[w < windows.count ? w : windows.count - 1];
            }
            forced[from_slot] = plan.allowed[sp] != 0;
        }
        free(path);
        common_practice_key_windows_destroy(&windows);
    }
    bool *fixed = (bool *)calloc(measures.measure_count, sizeof(bool));
    if (fixed == NULL && status == COMMON_PRACTICE_KEY_REGIONS_OK) {
        status = COMMON_PRACTICE_KEY_REGIONS_ERR_ALLOC;
    }
    if (status == COMMON_PRACTICE_KEY_REGIONS_OK && !phrase_mode) {
        for (size_t m = 0; m < measures.measure_count; m++) {
            fixed[m] = forced[m];
        }
        for (size_t sp = 1; sp < plan.count; sp++) {
            fixed[plan.start[sp]] = true;
        }
        hold_home_keys(segments, segment_count, &measures, fixed, effective.hold_tolerance, measure_key);
    }
    if (status == COMMON_PRACTICE_KEY_REGIONS_OK && !phrase_mode && effective.phrase_boundary_count > 0) {
        size_t *slots = (size_t *)calloc(effective.phrase_boundary_count, sizeof(size_t));
        if (slots == NULL) {
            status = COMMON_PRACTICE_KEY_REGIONS_ERR_ALLOC;
        }
        size_t slot_count = 0;
        for (size_t b = 0; b < effective.phrase_boundary_count && status == COMMON_PRACTICE_KEY_REGIONS_OK; b++) {
            int measure = 0;
            status = nearest_barline(map, effective.phrase_boundaries[b], &measure);
            long slot = (long)measure - (long)measures.first_measure;
            if (status == COMMON_PRACTICE_KEY_REGIONS_OK && slot > 0 && (size_t)slot < measures.measure_count) {
                slots[slot_count++] = (size_t)slot;
            }
        }
        if (status == COMMON_PRACTICE_KEY_REGIONS_OK) {
            status = snap_edges(measure_key, fixed, measures.measure_count, slots, slot_count,
                                effective.snap_reach_measures);
        }
        free(slots);
    }
    free(fixed);
    CommonPracticeKeyRegions result = {NULL, 0, NULL, 0};
    if (status == COMMON_PRACTICE_KEY_REGIONS_OK) {
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
                                  uncertainty,
                                  forced[from] ? COMMON_PRACTICE_REGION_SOURCE_KEY_SIGNATURE
                                               : COMMON_PRACTICE_REGION_SOURCE_PROFILE,
                                  &result.regions[result.region_count]);
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

    free(measure_key);
    free(forced);
    free(arrival_slot);
    free(plan.start);
    free(plan.allowed);
    measures_release(&measures);
    if (status != COMMON_PRACTICE_KEY_REGIONS_OK) {
        common_practice_key_regions_destroy(&result);
        return status;
    }
    *out = result;
    return COMMON_PRACTICE_KEY_REGIONS_OK;
}

bool common_practice_key_region_analysis_key(const CommonPracticeKeyRegion *region, AnalysisKey *out) {
    if (region == NULL || out == NULL) {
        return false;
    }
    const char *name = common_practice_key_index_tonic_name(region->key_index);
    if (name == NULL) {
        return false;
    }
    static const PitchLetter LETTERS[7] = {PITCH_LETTER_A, PITCH_LETTER_B, PITCH_LETTER_C, PITCH_LETTER_D,
                                           PITCH_LETTER_E, PITCH_LETTER_F, PITCH_LETTER_G};
    out->id = NULL;
    out->tonic_letter = LETTERS[name[0] - 'A'];
    out->tonic_accidental = name[1] == '#' ? 1 : (name[1] == 'b' ? -1 : 0);
    out->mode = common_practice_key_index_mode(region->key_index) == COMMON_PRACTICE_KEY_MINOR ? KEY_STRUCTURE_MODE_MINOR
                                                                                              : KEY_STRUCTURE_MODE_MAJOR;
    return true;
}

bool common_practice_key_signature_event_from_analysis(const AnalysisKeyEvent *event, CommonPracticeKeySignatureEvent *out) {
    if (event == NULL || out == NULL || event->time == NULL) {
        return false;
    }
    int fifths = pitch_spelling_fifths(event->key.tonic_letter, event->key.tonic_accidental);
    if (event->key.mode == KEY_STRUCTURE_MODE_MINOR) {
        fifths -= 3;
    }
    if (fifths < -7 || fifths > 7) {
        return false;
    }
    out->position = event->time;
    out->sharps_flats = fifths;
    return true;
}

static bool region_duration(const CommonPracticeKeyRegion *region, Rational *out) {
    Rational start;
    Rational end;
    if (region->span == NULL || region->key_index < 0 || region->key_index >= COMMON_PRACTICE_KEY_COUNT ||
        rational_of_musical_time(time_span_start(region->span), &start) != RATIONAL_OK ||
        rational_of_musical_time(time_span_end(region->span), &end) != RATIONAL_OK) {
        return false;
    }
    return rational_sub(end, start, out) == RATIONAL_OK;
}

bool common_practice_home_key_region(const CommonPracticeKeyRegion *regions, size_t count, size_t *home,
                                     Rational *duration) {
    if (regions == NULL || home == NULL || count == 0) {
        return false;
    }
    size_t best_region = 0;
    Rational best = {0, 1};
    for (size_t i = 0; i < count; i++) {
        bool first_of_key = true;
        for (size_t earlier = 0; earlier < i && first_of_key; earlier++) {
            first_of_key = regions[earlier].key_index != regions[i].key_index;
        }
        if (!first_of_key) {
            continue;
        }
        Rational total = {0, 1};
        for (size_t j = i; j < count; j++) {
            Rational length;
            if (regions[j].key_index == regions[i].key_index) {
                if (!region_duration(&regions[j], &length) || rational_add(total, length, &total) != RATIONAL_OK) {
                    return false;
                }
            }
        }
        if (i == 0 || rational_cmp(best, total) < 0) {
            best_region = i;
            best = total;
        }
    }
    *home = best_region;
    if (duration != NULL) {
        *duration = best;
    }
    return true;
}
