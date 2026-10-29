#include "analysis/feature_extraction.h"

#include <stdlib.h>
#include <string.h>

#include "core/string_copy.h"

struct FeatureExtraction {
    bool has_interval;
    Interval *interval;
    bool has_contour;
    char *contour;
    bool has_density;
    unsigned long density_numerator;
    unsigned long density_denominator;
};

FeatureExtractionResult feature_extraction_create(bool has_interval, Interval *interval, bool has_contour,
                                                   const char *contour, bool has_density,
                                                   unsigned long density_numerator, unsigned long density_denominator,
                                                   FeatureExtraction **out) {
    if (has_interval && interval == NULL) {
        return FEATURE_EXTRACTION_ERR_NULL_INTERVAL;
    }
    if (has_contour && (contour == NULL || contour[0] == '\0')) {
        return FEATURE_EXTRACTION_ERR_NULL_CONTOUR;
    }
    if (has_density && density_denominator == 0) {
        return FEATURE_EXTRACTION_ERR_NON_POSITIVE_DENSITY_DENOMINATOR;
    }

    FeatureExtraction *feature_extraction = (FeatureExtraction *)malloc(sizeof(FeatureExtraction));
    if (feature_extraction == NULL) {
        return FEATURE_EXTRACTION_ERR_ALLOC;
    }

    char *copied_contour = NULL;
    if (has_contour) {
        copied_contour = ursatz_string_copy(contour);
        if (copied_contour == NULL) {
            free(feature_extraction);
            return FEATURE_EXTRACTION_ERR_ALLOC;
        }
    }

    feature_extraction->has_interval = has_interval;
    feature_extraction->interval = has_interval ? interval : NULL;
    feature_extraction->has_contour = has_contour;
    feature_extraction->contour = copied_contour;
    feature_extraction->has_density = has_density;
    feature_extraction->density_numerator = has_density ? density_numerator : 0;
    feature_extraction->density_denominator = has_density ? density_denominator : 0;

    *out = feature_extraction;
    return FEATURE_EXTRACTION_OK;
}

void feature_extraction_destroy(FeatureExtraction *feature_extraction) {
    if (feature_extraction == NULL) {
        return;
    }
    if (feature_extraction->has_interval) {
        interval_destroy(feature_extraction->interval);
    }
    free(feature_extraction->contour);
    free(feature_extraction);
}

bool feature_extraction_has_interval(const FeatureExtraction *feature_extraction) {
    return feature_extraction->has_interval;
}

const Interval *feature_extraction_interval(const FeatureExtraction *feature_extraction) {
    return feature_extraction->interval;
}

bool feature_extraction_has_contour(const FeatureExtraction *feature_extraction) {
    return feature_extraction->has_contour;
}

const char *feature_extraction_contour(const FeatureExtraction *feature_extraction) {
    return feature_extraction->contour;
}

bool feature_extraction_has_density(const FeatureExtraction *feature_extraction) {
    return feature_extraction->has_density;
}

unsigned long feature_extraction_density_numerator(const FeatureExtraction *feature_extraction) {
    return feature_extraction->density_numerator;
}

unsigned long feature_extraction_density_denominator(const FeatureExtraction *feature_extraction) {
    return feature_extraction->density_denominator;
}