#include "pipeline/roman_numeral_pipeline.h"

#include <stdlib.h>
#include <string.h>

#include "applied_chords.h"
#include "chord_input.h"
#include "roman_numeral_adapter.h"
#include "weighted_identification.h"

#define REASON_NO_REGION "no key region"
#define REASON_MINOR_REGION "the library's chord identification names chords in major keys only"
#define REASON_NO_CHORD "the library identified no chord"
#define REASON_NO_NUMERAL "the library gives no Roman numeral for this chord in its key"

static double chord_uncertainty(const AnalysisChordReading *reading) {
    if (!reading->confidence.present || reading->confidence.denominator == 0) {
        return 1.0;
    }
    return 1.0 - (double)reading->confidence.numerator / (double)reading->confidence.denominator;
}

/* The adapter's input for one segment. has_numeral stays false unless the library found a chord in a
   region's key. */
static bool fill_input(const Segmentation *segmentation, const ChordEstimate *chord, const AnalysisKeyRegion *regions,
                       const AnalysisReportEntry *entry, const CommonPracticeWeightedConfig *weighting, size_t index,
                       StandardTonalityChordInput *input) {
    const Segment *segment = &segmentation->segments[index];
    input->segment_id = segmentation->segment_ids[index];
    input->bass_pitch_class = segment_bass_pitch_class(segment);

    CommonPracticeWeights weights;
    if (common_practice_weight_segment(segment->members, segment->member_count, weighting, &weights) !=
        COMMON_PRACTICE_WEIGHTED_OK) {
        return false;
    }
    input->has_weights = true;
    memcpy(input->weights, weights.pitch_class, sizeof(input->weights));

    if (!chord->in_region) {
        return true;
    }
    const AnalysisKeyRegion *region = &regions[chord->region];
    input->region_index = chord->region;
    input->tonic_pitch_class = region->tonic_pitch_class;
    input->mode = region->is_major ? STANDARD_TONALITY_ROMAN_KEY_MAJOR : STANDARD_TONALITY_ROMAN_KEY_MINOR;
    input->key_uncertainty = region->uncertainty;
    if (!chord->found) {
        return true;
    }
    input->has_numeral = true;
    input->root_pitch_class = chord->root_pitch_class;
    input->interval_count = chord->interval_count;
    for (size_t k = 0; k < chord->interval_count; k++) {
        input->intervals[k] = chord->intervals[k];
    }
    input->chord_uncertainty = chord_uncertainty(&entry->chord);
    return true;
}

static const char *unlabelled_reason(const ChordEstimate *chord, const AnalysisKeyRegion *regions) {
    if (!chord->in_region) {
        return REASON_NO_REGION;
    }
    if (!chord->found) {
        return regions[chord->region].is_major ? REASON_NO_CHORD : REASON_MINOR_REGION;
    }
    return REASON_NO_NUMERAL;
}

bool assign_roman_numerals(const Segmentation *segmentation, const ChordEstimate *chords,
                           const AnalysisKeyRegion *regions, size_t region_count, AnalysisReportEntry *entries) {
    (void)region_count;
    size_t count = segmentation->segment_count;
    StandardTonalityChordInput *inputs = calloc(count > 0 ? count : 1, sizeof(StandardTonalityChordInput));
    if (!inputs) {
        return false;
    }
    CommonPracticeWeightedConfig weighting = common_practice_weighted_config_default();
    for (size_t i = 0; i < count; i++) {
        inputs[i].bass_pitch_class = -1;
        if (!fill_input(segmentation, &chords[i], regions, &entries[i], &weighting, i, &inputs[i])) {
            free(inputs);
            return false;
        }
    }

    StandardTonalityRomanResult *results = NULL;
    if (standard_tonality_build_roman_numerals(inputs, count, &results) != STANDARD_TONALITY_ROMAN_OK) {
        free(inputs);
        return false;
    }

    bool ok = true;
    for (size_t i = 0; ok && i < count; i++) {
        AnalysisRomanNumeral *roman = &entries[i].roman;
        if (results[i].has_numeral) {
            roman->numeral = strdup(results[i].claim.label);
            roman->labelled = roman->numeral != NULL;
            roman->degree = results[i].claim.degree;
            roman->uncertainty = results[i].uncertainty;
            ok = roman->labelled;
        } else {
            roman->reason = strdup(unlabelled_reason(&chords[i], regions));
            ok = roman->reason != NULL;
        }
    }
    standard_tonality_roman_results_destroy(results, count);
    free(inputs);
    return ok;
}
