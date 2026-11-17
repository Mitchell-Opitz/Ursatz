#include "analysis/harmony/key_regions_internal.h"

#include <stdlib.h>

#include "theory/tonality/tonal_key.h"

/* The pitch classes of a key's natural collection as a 12-bit mask: the major scale, or the relative major's
   for a minor key. */
static unsigned natural_collection(int key_index) {
    StandardTonalityMode mode = common_practice_key_index_mode(key_index) == COMMON_PRACTICE_KEY_MINOR
                                    ? STANDARD_TONALITY_MODE_NATURAL_MINOR
                                    : STANDARD_TONALITY_MODE_MAJOR;
    return standard_tonality_mode_pitch_classes(common_practice_key_index_tonic(key_index), mode);
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
    StandardTonalityMode mode = common_practice_key_index_mode(key_index) == COMMON_PRACTICE_KEY_MINOR
                                    ? STANDARD_TONALITY_MODE_NATURAL_MINOR
                                    : STANDARD_TONALITY_MODE_MAJOR;
    return standard_tonality_mode_pitch_classes_with_leading_tone(common_practice_key_index_tonic(key_index), mode);
}

/* Folds a region back into the key before it when it is a passage of IV, V and ii of that key (see key_regions.h,
   HOME KEY HELD). */
void key_regions_hold_home_keys(const CommonPracticeKeySegment *segments, size_t segment_count,
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
CommonPracticeKeyRegionsStatus key_regions_snap_edges(int *measure_key, const bool *fixed, size_t measure_count,
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

/* The pivot of one change (see key_regions.h): the last segment at or before the boundary, within the search
   distance, whose best chord in the old key lies inside the new key's collection. */
CommonPracticeKeyRegionsStatus key_regions_find_pivot(const CommonPracticeKeySegment *segments, size_t segment_count,
                                                  const MeasureWeights *measures, int boundary_measure,
                                                  const MusicalTime *boundary_start, int old_key, int new_key,
                                                  size_t search_measures, bool strictly_before, bool *found,
                                                  size_t *pivot) {
    *found = false;
    int old_tonic = common_practice_key_index_tonic(old_key);
    CommonPracticeKeyMode old_mode = common_practice_key_index_mode(old_key);
    unsigned new_collection = natural_collection(new_key);
    for (size_t i = segment_count; i-- > 0;) {
        int measure = measures->segment_measure[i];
        if (measure > boundary_measure || key_regions_before(boundary_start, segments[i].start) ||
            (strictly_before && !key_regions_before(segments[i].start, boundary_start))) {
            continue; /* after the boundary, or in phrase mode at it */
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
