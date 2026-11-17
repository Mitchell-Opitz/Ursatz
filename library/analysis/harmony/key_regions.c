#include "analysis/harmony/key_regions_internal.h"

#include <stdlib.h>
#include <string.h>

#include "model/pitch/pitch_arithmetic.h"

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
    bool perfect_fifth = (tones & (1u << ((root + 7) % 12))) != 0;
    if (!perfect_fifth && (tones & ((1u << ((root + 6) % 12)) | (1u << ((root + 8) % 12)))) != 0) {
        return 0; /* a diminished or augmented arrival confirms no key */
    }
    if (kind == COMMON_PRACTICE_CADENCE_AUTHENTIC) {
        if (major_third && !minor_third) {
            return 1u << root;
        }
        return minor_third && !major_third ? 1u << (12 + root) : 0;
    }
    if (kind == COMMON_PRACTICE_CADENCE_HALF && major_third && !minor_third && perfect_fifth) {
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

const char *common_practice_key_index_tonic_name(int key_index) {
    int tonic = common_practice_key_index_tonic(key_index);
    return tonic < 0 ? NULL : key_tonic_name(tonic);
}

void common_practice_key_regions_destroy(CommonPracticeKeyRegions *regions) {
    if (regions == NULL) {
        return;
    }
    for (size_t i = 0; i < regions->region_count; i++) {
        key_destroy(regions->regions[i].key);
    }
    free(regions->regions);
    free(regions->changes);
    regions->regions = NULL;
    regions->changes = NULL;
    regions->region_count = 0;
    regions->change_count = 0;
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
    CommonPracticeKeyRegionsStatus status = key_regions_validate_inputs(segments, segment_count, map, &effective);
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
        if (!key_regions_time_ok(effective.phrase_boundaries[b])) {
            return COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_EVENT;
        }
    }

    MeasureWeights measures;
    status = key_regions_gather_measures(segments, segment_count, map, &measures);
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
        status = key_regions_plan_spans(events, event_count, map, &measures, &plan);
    }
    if (phrase_mode && status == COMMON_PRACTICE_KEY_REGIONS_OK) {
        status = key_regions_place_keys_on_phrases(&effective, &measures, &plan, measure_key, forced, arrival_slot);
    }
    for (size_t sp = 0; sp < plan.count && status == COMMON_PRACTICE_KEY_REGIONS_OK && !phrase_mode; sp++) {
        size_t from_slot = plan.start[sp];
        size_t to_slot = sp + 1 < plan.count ? plan.start[sp + 1] : measures.measure_count;
        MeasureWeights view = {NULL, measures.weights + from_slot, to_slot - from_slot,
                               measures.first_measure + (int)from_slot, measures.first_measure + (int)to_slot - 1,
                               measures.evidence + from_slot, measures.chords + from_slot};
        CommonPracticeKeyWindows windows = {0};
        status = key_regions_score_windows(&view, &effective, &windows);
        if (status != COMMON_PRACTICE_KEY_REGIONS_OK) {
            break;
        }
        int *path = (int *)calloc(windows.count, sizeof(int));
        if (path == NULL) {
            status = COMMON_PRACTICE_KEY_REGIONS_ERR_ALLOC;
        } else if (plan.allowed[sp] != 0) {
            status = key_regions_choose_path_restricted(&windows, effective.change_penalty, plan.allowed[sp], path);
        } else {
            status = key_regions_choose_path(&windows, effective.change_penalty, path);
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
        key_regions_hold_home_keys(segments, segment_count, &measures, fixed, effective.hold_tolerance, measure_key);
    }
    if (status == COMMON_PRACTICE_KEY_REGIONS_OK && !phrase_mode && effective.phrase_boundary_count > 0) {
        size_t *slots = (size_t *)calloc(effective.phrase_boundary_count, sizeof(size_t));
        if (slots == NULL) {
            status = COMMON_PRACTICE_KEY_REGIONS_ERR_ALLOC;
        }
        size_t slot_count = 0;
        for (size_t b = 0; b < effective.phrase_boundary_count && status == COMMON_PRACTICE_KEY_REGIONS_OK; b++) {
            int measure = 0;
            status = key_regions_nearest_barline(map, effective.phrase_boundaries[b], &measure);
            long slot = (long)measure - (long)measures.first_measure;
            if (status == COMMON_PRACTICE_KEY_REGIONS_OK && slot > 0 && (size_t)slot < measures.measure_count) {
                slots[slot_count++] = (size_t)slot;
            }
        }
        if (status == COMMON_PRACTICE_KEY_REGIONS_OK) {
            status = key_regions_snap_edges(measure_key, fixed, measures.measure_count, slots, slot_count,
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
                key_regions_region_uncertainty(&measures, from, m - 1, measure_key[from], &effective.profiles);
            status = key_regions_build_region(&effective, result.region_count + 1, measure_key[from], map,
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
            change->at_arrival = phrase_mode && arrival_slot[into->first_measure - measures.first_measure];
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
            status = key_regions_find_pivot(segments, segment_count, &measures, into->first_measure,
                                into->span.start, result.regions[change->from_region].key_index,
                                into->key_index, effective.pivot_search_measures, phrase_mode, &change->has_pivot,
                                &change->pivot_segment);
        }
    }

    free(measure_key);
    free(forced);
    free(arrival_slot);
    free(plan.start);
    free(plan.allowed);
    key_regions_measures_release(&measures);
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
    PitchLetter letter;
    if (!pitch_letter_from_char(name[0], &letter)) {
        return false;
    }
    out->id = NULL;
    out->tonic_letter = letter;
    out->tonic_accidental = name[1] == '#' ? 1 : (name[1] == 'b' ? -1 : 0);
    out->mode = common_practice_key_index_mode(region->key_index) == COMMON_PRACTICE_KEY_MINOR ? KEY_STRUCTURE_MODE_MINOR
                                                                                              : KEY_STRUCTURE_MODE_MAJOR;
    return true;
}

bool common_practice_key_signature_event_from_analysis(const AnalysisKeyEvent *event, CommonPracticeKeySignatureEvent *out) {
    if (event == NULL || out == NULL) {
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
    if (region->key_index < 0 || region->key_index >= COMMON_PRACTICE_KEY_COUNT) {
        return false;
    }
    return rational_sub(region->span.end.value, region->span.start.value, out) == RATIONAL_OK;
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
