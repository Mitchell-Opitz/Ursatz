#ifndef MUSIC_ANALYSIS_FEATURE_EXTRACTION_H
#define MUSIC_ANALYSIS_FEATURE_EXTRACTION_H

#include <stdbool.h>

#include "semantics/interval.h"

/*
 * FeatureExtraction (Concept, Registry #13): explicit derived features
 * (interval, contour, density, etc.) an Analyzer computes from musical
 * material. Registry requires these be explicit named features, never
 * collapsed into one opaque feature blob (the same rule Evidence
 * follows for strength/relevance/reliability, interpretation/
 * evidence.h) -- each feature below is its own field with its own
 * accessor, independently present or absent.
 *
 * A Concept, not a Category: FeatureExtraction carries no EntityID of
 * its own (the same treatment Uncertainty gets, interpretation/
 * uncertainty.h) -- it is a value describing what was measured, not an
 * identified object in the canonical IR.
 *
 * Every feature is independently optional (has_* flag): an extraction
 * that only computed contour, say, leaves interval and density both
 * absent rather than a caller having to fabricate placeholder values
 * for what was never measured.
 *
 * interval reuses semantics/interval.h's Interval (a Value, TDD Sec67)
 * rather than reinventing pitch-distance representation -- when
 * has_interval is true, FeatureExtraction takes ownership of the
 * supplied Interval (destroyed by feature_extraction_destroy).
 *
 * contour is a required-when-present descriptor string (e.g.
 * "ascending", "arch", "descending-then-ascending") -- no dedicated
 * Contour type exists yet, the same placeholder treatment
 * Transformation gives its own dimension fields (transform/
 * transformation.h).
 *
 * density is a required-when-present exact rational fraction
 * (numerator/denominator, denominator > 0), never float/double (Law
 * 19), consistent with Evidence and Uncertainty's confidence
 * representation -- e.g. note-onsets per reference duration. What
 * exactly the fraction is measured against is left to the analyzer
 * that produced it; no reference-duration type is threaded through
 * here.
 */

typedef struct FeatureExtraction FeatureExtraction;

typedef enum {
    FEATURE_EXTRACTION_OK = 0,
    FEATURE_EXTRACTION_ERR_NULL_INTERVAL,
    FEATURE_EXTRACTION_ERR_NULL_CONTOUR,
    FEATURE_EXTRACTION_ERR_NON_POSITIVE_DENSITY_DENOMINATOR,
    FEATURE_EXTRACTION_ERR_ALLOC
} FeatureExtractionResult;

/*
 * Takes ownership of interval on success when has_interval is true
 * (destroyed by feature_extraction_destroy); interval is ignored when
 * has_interval is false. has_interval true with a NULL interval is
 * rejected. contour is copied, required (non-empty) when has_contour
 * is true, ignored when false. density_numerator/density_denominator
 * are read only when has_density is true; density_denominator must
 * then be > 0.
 */
FeatureExtractionResult feature_extraction_create(bool has_interval, Interval *interval, bool has_contour,
                                                   const char *contour, bool has_density,
                                                   unsigned long density_numerator, unsigned long density_denominator,
                                                   FeatureExtraction **out);
void feature_extraction_destroy(FeatureExtraction *feature_extraction);

bool feature_extraction_has_interval(const FeatureExtraction *feature_extraction);
/* NULL when has_interval is false. */
const Interval *feature_extraction_interval(const FeatureExtraction *feature_extraction);

bool feature_extraction_has_contour(const FeatureExtraction *feature_extraction);
/* NULL when has_contour is false. */
const char *feature_extraction_contour(const FeatureExtraction *feature_extraction);

bool feature_extraction_has_density(const FeatureExtraction *feature_extraction);
unsigned long feature_extraction_density_numerator(const FeatureExtraction *feature_extraction);
unsigned long feature_extraction_density_denominator(const FeatureExtraction *feature_extraction);

#endif