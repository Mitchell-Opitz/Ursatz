#include "pipeline/roman_numeral_pipeline.h"

#include <stdlib.h>
#include <string.h>

#include "standard-tonality/analysis/applied_chords.h"
#include "standard-tonality/analysis/chord_input.h"
#include "standard-tonality/analysis/roman_numeral_adapter.h"
#include "time/time_span.h"
#include "common-practice-minimal/weighted_identification.h"

#define REASON_NO_REGION "no key region"
#define REASON_NO_CHORD "the library identified no chord"
#define REASON_NO_NUMERAL "the library gives no Roman numeral for this chord in its key"

/* The numeral without its inversion figure, formatted by the library from the same reading. */
static char *base_numeral_of(const StandardTonalityRomanResult *result) {
    StandardTonalityRomanReading root_position = result->reading;
    root_position.inversion = 0;
    char text[STANDARD_TONALITY_ROMAN_LABEL_MAX];
    if (!standard_tonality_format_roman_reading(&root_position, text, sizeof(text))) {
        return NULL;
    }
    return strdup(text);
}

/* The applied chord's parts, from the library's reading: the chord as it reads in the tonicized key and the
   chord tonicized, each formatted by the library. */
static bool fill_applied(const StandardTonalityRomanResult *result, AnalysisRomanNumeral *roman) {
    StandardTonalityRomanReading chord_alone = result->reading;
    chord_alone.applied = false;
    char applied_text[STANDARD_TONALITY_ROMAN_LABEL_MAX];
    char target_text[STANDARD_TONALITY_ROMAN_LABEL_MAX];
    if (!standard_tonality_format_roman_reading(&chord_alone, applied_text, sizeof(applied_text)) ||
        !standard_tonality_roman_numeral_format(&result->reading.applied_target, target_text, sizeof(target_text))) {
        return false;
    }
    roman->applied_numeral = strdup(applied_text);
    roman->target_numeral = strdup(target_text);
    roman->applied = true;
    roman->target_degree = result->claim.applied_target_degree;
    roman->applied_kind = applied_chord_kind_name((AppliedChordKind)result->claim.applied_kind);
    roman->resolution_entry = result->resolution_index;
    roman->deceptive = result->deceptive;
    return roman->applied_numeral != NULL && roman->target_numeral != NULL && roman->applied_kind != NULL;
}

static const char *unlabelled_reason(const ChordEstimate *chord) {
    if (!chord->in_region) {
        return REASON_NO_REGION;
    }
    if (!chord->found) {
        return REASON_NO_CHORD;
    }
    return REASON_NO_NUMERAL;
}

void roman_numerals_destroy(RomanNumerals *romans) {
    if (!romans) {
        return;
    }
    standard_tonality_roman_results_destroy(romans->results, romans->count);
    standard_tonality_chord_inputs_destroy(romans->inputs);
    for (size_t i = 0; romans->starts && i < romans->segment_count; i++) {
        musical_time_destroy(romans->starts[i]);
    }
    free(romans->starts);
    free(romans->segments);
    free(romans->regions);
    free(romans->pitches);
    free(romans->weights);
    *romans = (RomanNumerals){0};
}

/* The adapter's segment inputs: each segment's start, the library's chord, its pitch-class weights and its sounding
   pitches (for the chord bass). False on allocation or library failure. */
static bool fill_segments(const Segmentation *segmentation, const ChordEstimate *chords, RomanNumerals *romans) {
    size_t count = segmentation->segment_count;
    size_t pitch_total = 0;
    for (size_t i = 0; i < count; i++) {
        pitch_total += segmentation->segments[i].member_count;
    }
    romans->segments = calloc(count, sizeof(StandardTonalitySegmentInput));
    romans->starts = calloc(count, sizeof(MusicalTime *));
    romans->weights = calloc(count, sizeof(*romans->weights));
    romans->pitches = calloc(pitch_total > 0 ? pitch_total : 1, sizeof(int));
    if (!romans->segments || !romans->starts || !romans->weights || !romans->pitches) {
        return false;
    }
    romans->segment_count = count;
    CommonPracticeWeightedConfig weighting = common_practice_weighted_config_default();
    size_t next_pitch = 0;
    for (size_t i = 0; i < count; i++) {
        const Segment *segment = &segmentation->segments[i];
        StandardTonalitySegmentInput *input = &romans->segments[i];
        if (musical_time_create(segment->start_numerator, segment->start_denominator, &romans->starts[i]) !=
            MUSICAL_TIME_OK) {
            return false;
        }
        CommonPracticeWeights weights;
        if (common_practice_weight_segment(segment->members, segment->member_count, &weighting, &weights) !=
            COMMON_PRACTICE_WEIGHTED_OK) {
            return false;
        }
        memcpy(romans->weights[i], weights.pitch_class, sizeof(romans->weights[i]));
        input->segment_id = segmentation->segment_ids[i];
        input->start = romans->starts[i];
        input->chord = chords[i].found ? chords[i].result.best_chord : NULL;
        input->weights = romans->weights[i];
        input->pitches = &romans->pitches[next_pitch];
        input->pitch_count = segment->member_count;
        for (size_t m = 0; m < segment->member_count; m++) {
            romans->pitches[next_pitch++] = segment->members[m].pitch;
        }
    }
    return true;
}

bool build_roman_numerals(const Segmentation *segmentation, const ChordEstimate *chords,
                          const CommonPracticeKeyRegions *found, RomanNumerals *out) {
    *out = (RomanNumerals){0};
    if (segmentation->segment_count == 0) {
        return true;
    }
    out->regions = calloc(found->region_count > 0 ? found->region_count : 1, sizeof(StandardTonalityKeyRegionInput));
    if (!out->regions) {
        return false;
    }
    for (size_t r = 0; r < found->region_count; r++) {
        out->regions[r].key = found->regions[r].key;
        out->regions[r].start = time_span_start(found->regions[r].span);
        out->regions[r].uncertainty = found->regions[r].uncertainty;
    }
    if (!fill_segments(segmentation, chords, out) ||
        standard_tonality_build_chord_inputs(out->segments, out->segment_count, out->regions, found->region_count,
                                             &out->inputs, &out->count) != STANDARD_TONALITY_CHORD_INPUT_OK) {
        return false;
    }
    return standard_tonality_build_roman_numerals(out->inputs, out->count, &out->results) == STANDARD_TONALITY_ROMAN_OK;
}

bool assign_roman_numerals(const RomanNumerals *romans, const ChordEstimate *chords, AnalysisReportEntry *entries) {
    for (size_t i = 0; i < romans->count; i++) {
        const StandardTonalityRomanResult *result = &romans->results[i];
        AnalysisRomanNumeral *roman = &entries[i].roman;
        if (result->has_numeral) {
            roman->numeral = strdup(result->claim.label);
            roman->base_numeral = base_numeral_of(result);
            roman->labelled = roman->numeral != NULL && roman->base_numeral != NULL;
            roman->degree = result->claim.degree;
            roman->uncertainty = result->uncertainty;
            if (!roman->labelled || (result->reading.applied && !fill_applied(result, roman))) {
                return false;
            }
        } else {
            roman->reason = strdup(unlabelled_reason(&chords[i]));
            if (!roman->reason) {
                return false;
            }
        }
    }
    return true;
}
