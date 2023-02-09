#include "analysis/cadence_classification.h"

#include <stddef.h>

#include "interpretation/uncertainty.h"
#include "semantics/cadence_type.h"

/* Weight tiers out of 8ths for the top candidate; the complement candidate gets (8 - top)/8. */
#define CADENCE_CLASSIFICATION_WEIGHT_DEFAULT 6u  /* 3/4 */
#define CADENCE_CLASSIFICATION_WEIGHT_STRONG 7u   /* 7/8 */
#define CADENCE_CLASSIFICATION_WEIGHT_WEAK 4u     /* 1/2 */
#define CADENCE_CLASSIFICATION_WEIGHT_DENOM 8u

static unsigned long authentic_top_weight(const CadenceClassificationInput *input) {
    unsigned long weight = CADENCE_CLASSIFICATION_WEIGHT_DEFAULT;

    if (input->from_is_dominant_seventh) {
        weight = CADENCE_CLASSIFICATION_WEIGHT_STRONG;
    }

    if (input->has_bass_motion) {
        if (input->bass_motion == VOICE_LEADING_BASS_MOTION_LEAP) {
            weight = CADENCE_CLASSIFICATION_WEIGHT_STRONG;
        } else if (input->bass_motion == VOICE_LEADING_BASS_MOTION_SAME) {
            weight = CADENCE_CLASSIFICATION_WEIGHT_WEAK;
        }
    }

    return weight;
}

/* The candidate label is the display label of the CadenceType; the label is the Interpretation
   claim (see cadence_type_label). */
static UncertaintyCandidate make_candidate(CadenceType type, unsigned long weight) {
    return (UncertaintyCandidate){(char *)cadence_type_label(type), weight, CADENCE_CLASSIFICATION_WEIGHT_DENOM};
}

/* The default-weight candidate for `type` and its UNKNOWN complement. */
static void set_typed_pair(UncertaintyCandidate candidates[2], CadenceType type) {
    candidates[0] = make_candidate(type, CADENCE_CLASSIFICATION_WEIGHT_DEFAULT);
    candidates[1] = make_candidate(CADENCE_TYPE_UNKNOWN,
                                    CADENCE_CLASSIFICATION_WEIGHT_DENOM - CADENCE_CLASSIFICATION_WEIGHT_DEFAULT);
}

AnalyzerResult cadence_classification_analyze(const void *input, const Context *context, const Value *config,
                                               AnalysisResult **out) {
    if (input == NULL) {
        return ANALYZER_ERR_NULL_INPUT;
    }
    if (out == NULL) {
        return ANALYZER_ERR_NULL_OUT;
    }

    const CadenceClassificationInput *classification_input = (const CadenceClassificationInput *)input;
    if (classification_input->region == NULL || classification_input->from_chord_root_degree < 1 ||
        classification_input->from_chord_root_degree > 7 || classification_input->to_chord_root_degree < 1 ||
        classification_input->to_chord_root_degree > 7) {
        return ANALYZER_ERR_NULL_INPUT;
    }

    int from_degree = classification_input->from_chord_root_degree;
    int to_degree = classification_input->to_chord_root_degree;

    UncertaintyCandidate candidates[2];
    size_t candidate_count;

    if (to_degree == 1 && from_degree == 5) {
        unsigned long top_weight = authentic_top_weight(classification_input);
        unsigned long complement_weight = CADENCE_CLASSIFICATION_WEIGHT_DENOM - top_weight;

        CadenceType top_type;
        CadenceType complement_type;
        if (classification_input->has_soprano_resolves_to_tonic) {
            if (classification_input->soprano_resolves_to_tonic) {
                top_type = CADENCE_TYPE_PERFECT_AUTHENTIC;
                complement_type = CADENCE_TYPE_IMPERFECT_AUTHENTIC;
            } else {
                top_type = CADENCE_TYPE_IMPERFECT_AUTHENTIC;
                complement_type = CADENCE_TYPE_PERFECT_AUTHENTIC;
            }
        } else {
            top_type = CADENCE_TYPE_AUTHENTIC_UNSPECIFIED;
            complement_type = CADENCE_TYPE_UNKNOWN;
        }

        candidates[0] = make_candidate(top_type, top_weight);
        candidates[1] = make_candidate(complement_type, complement_weight);
        candidate_count = 2;
    } else if (to_degree == 1 && from_degree == 4) {
        set_typed_pair(candidates, CADENCE_TYPE_PLAGAL);
        candidate_count = 2;
    } else if (to_degree == 5) {
        set_typed_pair(candidates, CADENCE_TYPE_HALF);
        candidate_count = 2;
    } else if (from_degree == 5 && to_degree == 6) {
        set_typed_pair(candidates, CADENCE_TYPE_DECEPTIVE);
        candidate_count = 2;
    } else {
        candidates[0] = (UncertaintyCandidate){(char *)cadence_type_label(CADENCE_TYPE_UNKNOWN), 1, 1};
        candidate_count = 1;
    }

    CadenceDetectionInput detection_input = {classification_input->region, candidates, candidate_count,
                                              classification_input->framework};
    return cadence_detection_analyze(&detection_input, context, config, out);
}

const Analyzer CADENCE_CLASSIFICATION_ANALYZER = {"CadenceClassification", cadence_classification_analyze};
