#ifndef URSATZ_CHAIN_CHAIN_RESULT_INTERPRETATION_H
#define URSATZ_CHAIN_CHAIN_RESULT_INTERPRETATION_H

#include <chain/chain_result.h>

#include "model/interpretation/interpretation.h"
#include "model/interpretation/uncertainty.h"

/* The Interpretation's scalar confidence, or present = false for an
   unknown or candidate-set uncertainty. */
static inline AnalysisConfidence interpretation_confidence(const Interpretation *interpretation) {
    AnalysisConfidence confidence = {false, 0, 0};
    const Uncertainty *uncertainty = interpretation_uncertainty(interpretation);
    if (uncertainty && uncertainty_kind(uncertainty) == UNCERTAINTY_KIND_SCALAR_CONFIDENCE) {
        confidence.present = true;
        confidence.numerator = uncertainty_scalar_confidence_numerator(uncertainty);
        confidence.denominator = uncertainty_scalar_confidence_denominator(uncertainty);
    }
    return confidence;
}

#endif
