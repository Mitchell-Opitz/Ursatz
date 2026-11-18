#include "chain/chain_result_key_regions.h"

#include <stdlib.h>
#include <string.h>

#include "analysis/harmony/key_regions.h"
#include "model/core/rational.h"
#include "chain/chain_result_spelling.h"
#include "model/time/musical_time.h"
#include "model/time/time_span.h"

static AnalysisKeyChangeExtent change_extent(KeyChangeExtent extent) {
    switch (extent) {
        case KEY_CHANGE_EXTENT_EXTENDED_TONICIZATION:
            return ANALYSIS_KEY_CHANGE_EXTENDED_TONICIZATION;
        case KEY_CHANGE_EXTENT_WEAK_MODULATION:
            return ANALYSIS_KEY_CHANGE_WEAK_MODULATION;
        case KEY_CHANGE_EXTENT_MODULATION:
            return ANALYSIS_KEY_CHANGE_MODULATION;
        default:
            return ANALYSIS_KEY_CHANGE_TONICIZATION;
    }
}

static bool copy_region(const CommonPracticeKeyRegion *source, AnalysisKeyRegion *out) {
    int tonic = common_practice_key_index_tonic(source->key_index);
    KeyStructure structure;
    if (!source->key || !key_structure(source->key, &structure) || !structure.material) {
        return false;
    }
    const PitchMaterial *material = structure.material;
    const MusicalTime start = source->span.start;
    const MusicalTime end = source->span.end;
    out->label = chain_key_label(tonic, material);
    if (!out->label) {
        return false;
    }
    out->tonic_pitch_class = tonic;
    out->material = material;
    out->prefer_flats = chain_tonic_prefers_flats(tonic, material);
    out->start_numerator = (long)start.value.num;
    out->start_denominator = (unsigned long)start.value.den;
    out->end_numerator = (long)end.value.num;
    out->end_denominator = (unsigned long)end.value.den;
    out->first_measure = source->first_measure;
    out->last_measure = source->last_measure;
    out->uncertainty = source->uncertainty;
    out->source = source->source == COMMON_PRACTICE_REGION_SOURCE_KEY_SIGNATURE ? ANALYSIS_REGION_SOURCE_KEY_SIGNATURE
                                                                                : ANALYSIS_REGION_SOURCE_PROFILE;
    return true;
}

static void copy_change(const CommonPracticeKeyChange *source, AnalysisKeyChange *out) {
    out->from_region = source->from_region;
    out->to_region = source->to_region;
    out->extent = change_extent(source->extent);
    out->cadence_confirmed = source->cadence_confirmed;
    out->is_return = source->is_return;
    out->has_pivot = source->has_pivot;
    out->pivot_entry = source->has_pivot ? source->pivot_segment : 0;
    out->pivot = (AnalysisChordReading){NULL, NULL, NULL, NULL, -1, 0.0, {false, 0, 0}};
}

bool key_regions_collect(const CommonPracticeKeyRegions *found, ChainResult *report) {
    if (!found || found->region_count == 0) {
        return true;
    }
    report->key_regions = calloc(found->region_count, sizeof(AnalysisKeyRegion));
    report->key_changes = calloc(found->change_count > 0 ? found->change_count : 1, sizeof(AnalysisKeyChange));
    if (!report->key_regions || !report->key_changes) {
        return false;
    }
    report->key_region_count = found->region_count;
    report->key_change_count = found->change_count;
    for (size_t i = 0; i < found->region_count; i++) {
        if (!copy_region(&found->regions[i], &report->key_regions[i])) {
            return false;
        }
    }
    for (size_t i = 0; i < found->change_count; i++) {
        copy_change(&found->changes[i], &report->key_changes[i]);
    }
    /* The home key: the library's key with the longest total duration over its regions. */
    Rational duration;
    if (!common_practice_home_key_region(found->regions, found->region_count, &report->home_region, &duration)) {
        return false;
    }
    report->home_duration_numerator = (long)duration.num;
    report->home_duration_denominator = (unsigned long)duration.den;
    return true;
}
