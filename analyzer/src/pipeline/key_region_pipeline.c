#include "pipeline/key_region_pipeline.h"

#include <stdlib.h>
#include <string.h>

#include "common-practice-minimal/key_regions.h"
#include "core/rational.h"
#include "pipeline/pitch_class_util.h"
#include "time/musical_time.h"
#include "time/time_span.h"

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
    bool is_major = common_practice_key_index_mode(source->key_index) == COMMON_PRACTICE_KEY_MAJOR;
    const MusicalTime *start = time_span_start(source->span);
    const MusicalTime *end = time_span_end(source->span);
    out->label = key_label_create(tonic, is_major);
    if (!out->label) {
        return false;
    }
    out->tonic_pitch_class = tonic;
    out->is_major = is_major;
    out->prefer_flats = tonic_prefers_flats(tonic, is_major);
    out->start_numerator = musical_time_numerator(start);
    out->start_denominator = musical_time_denominator(start);
    out->end_numerator = musical_time_numerator(end);
    out->end_denominator = musical_time_denominator(end);
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

bool key_regions_collect(const CommonPracticeKeyRegions *found, AnalysisReport *report) {
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
