#ifndef URSATZ_ANALYZER_INTERPRETATION_UTIL_H
#define URSATZ_ANALYZER_INTERPRETATION_UTIL_H

#include <ursatz_analyzer/analysis_report.h>

#include "interpretation/interpretation.h"
#include "interpretation/uncertainty.h"

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
