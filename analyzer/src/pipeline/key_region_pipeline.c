#include "pipeline/key_region_pipeline.h"

#include <stdlib.h>
#include <string.h>

#include "key_regions.h"
#include "pipeline/pitch_class_util.h"
#include "time/musical_time.h"
#include "time/time_span.h"
#include "weighted_identification.h"

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

/* The library's segments for key scoring: each segment's window and weights (the L2.5 weighting the chord
   identification uses), and its members for the pivot search. times owns the MusicalTimes the segments borrow. */
static bool build_key_segments(const Segmentation *segmentation, CommonPracticeKeySegment *segments,
                               MusicalTime **times, bool *library_ok) {
    CommonPracticeWeightedConfig config = common_practice_weighted_config_default();
    for (size_t i = 0; i < segmentation->segment_count; i++) {
        const Segment *segment = &segmentation->segments[i];
        if (musical_time_create(segment->start_numerator, segment->start_denominator, &times[2 * i]) !=
                MUSICAL_TIME_OK ||
            musical_time_create(segment->end_numerator, segment->end_denominator, &times[2 * i + 1]) !=
                MUSICAL_TIME_OK) {
            return false;
        }
        segments[i].start = times[2 * i];
        segments[i].end = times[2 * i + 1];
        segments[i].source = &segmentation->library_segments[i];
        if (common_practice_weight_segment(segment->members, segment->member_count, &config,
                                           &segments[i].weights) != COMMON_PRACTICE_WEIGHTED_OK) {
            *library_ok = false;
        }
    }
    return true;
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
    out->prefer_flats = tonic_prefers_flats(tonic);
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
    out->is_return = source->is_return;
    out->has_pivot = source->has_pivot;
    out->pivot_entry = source->has_pivot ? source->pivot_segment : 0;
    out->pivot = (AnalysisChordReading){NULL, NULL, NULL, NULL, -1, 0.0, {false, 0, 0}};
}

bool estimate_key_regions(const ImportedNotes *notes, const Segmentation *segmentation, AnalysisKeyRegion **regions,
                          size_t *region_count, AnalysisKeyChange **changes, size_t *change_count) {
    *regions = NULL;
    *region_count = 0;
    *changes = NULL;
    *change_count = 0;
    size_t segment_count = segmentation->segment_count;
    if (segment_count == 0) {
        return true;
    }

    CommonPracticeKeySegment *segments = calloc(segment_count, sizeof(CommonPracticeKeySegment));
    MusicalTime **times = calloc(2 * segment_count, sizeof(MusicalTime *));
    size_t event_count = notes->key_signature_events.count;
    CommonPracticeKeySignatureEvent *events =
        calloc(event_count > 0 ? event_count : 1, sizeof(CommonPracticeKeySignatureEvent));
    if (!segments || !times || !events) {
        free(segments);
        free(times);
        free(events);
        return false;
    }
    for (size_t i = 0; i < event_count; i++) {
        events[i].position = notes->key_signature_events.events[i].position;
        events[i].sharps_flats = notes->key_signature_events.events[i].sharps_flats;
    }

    bool library_ok = true;
    bool ok = build_key_segments(segmentation, segments, times, &library_ok);
    CommonPracticeKeyRegions found = {0};
    if (ok && library_ok &&
        key_regions_estimate(segments, segment_count, notes->meter.meter_map, events, event_count, NULL, &found) !=
            COMMON_PRACTICE_KEY_REGIONS_OK) {
        library_ok = false;
    }

    AnalysisKeyRegion *out_regions = NULL;
    AnalysisKeyChange *out_changes = NULL;
    if (ok && library_ok) {
        out_regions = calloc(found.region_count, sizeof(AnalysisKeyRegion));
        out_changes = calloc(found.change_count > 0 ? found.change_count : 1, sizeof(AnalysisKeyChange));
        ok = out_regions && out_changes;
        for (size_t i = 0; ok && i < found.region_count; i++) {
            ok = copy_region(&found.regions[i], &out_regions[i]);
        }
        if (ok) {
            for (size_t i = 0; i < found.change_count; i++) {
                copy_change(&found.changes[i], &out_changes[i]);
            }
            *regions = out_regions;
            *region_count = found.region_count;
            *changes = out_changes;
            *change_count = found.change_count;
        } else {
            if (out_regions) {
                for (size_t i = 0; i < found.region_count; i++) {
                    free(out_regions[i].label);
                }
            }
            free(out_regions);
            free(out_changes);
        }
        common_practice_key_regions_destroy(&found);
    }

    for (size_t i = 0; i < 2 * segment_count; i++) {
        musical_time_destroy(times[i]);
    }
    free(times);
    free(segments);
    free(events);
    return ok;
}

/* a/b < c/d for non-negative-denominator rationals. */
static bool rational_less(long long a, unsigned long long b, long long c, unsigned long long d) {
    return a * (long long)d < c * (long long)b;
}

size_t home_key_region(const AnalysisKeyRegion *regions, size_t region_count) {
    size_t home = 0;
    long long best_num = 0;
    unsigned long long best_den = 1;
    for (size_t i = 0; i < region_count; i++) {
        const AnalysisKeyRegion *region = &regions[i];
        long long num = (long long)region->end_numerator * (long long)region->start_denominator -
                        (long long)region->start_numerator * (long long)region->end_denominator;
        unsigned long long den = (unsigned long long)region->start_denominator * region->end_denominator;
        if (i == 0 || rational_less(best_num, best_den, num, den)) {
            home = i;
            best_num = num;
            best_den = den;
        }
    }
    return home;
}
