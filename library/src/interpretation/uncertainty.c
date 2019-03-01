#include "interpretation/uncertainty.h"

#include <stdlib.h>
#include <string.h>

struct Uncertainty {
    UncertaintyKind kind;

    unsigned long confidence_numerator;
    unsigned long confidence_denominator;

    UncertaintyCandidate *candidates;
    size_t candidate_count;
};

UncertaintyResult uncertainty_create_unknown(Uncertainty **out) {
    Uncertainty *uncertainty = (Uncertainty *)malloc(sizeof(Uncertainty));
    if (uncertainty == NULL) {
        return UNCERTAINTY_ERR_ALLOC;
    }

    uncertainty->kind = UNCERTAINTY_KIND_UNKNOWN;
    uncertainty->confidence_numerator = 0;
    uncertainty->confidence_denominator = 0;
    uncertainty->candidates = NULL;
    uncertainty->candidate_count = 0;

    *out = uncertainty;
    return UNCERTAINTY_OK;
}

UncertaintyResult uncertainty_create_scalar_confidence(unsigned long confidence_numerator,
                                                        unsigned long confidence_denominator,
                                                        Uncertainty **out) {
    if (confidence_denominator == 0) {
        return UNCERTAINTY_ERR_NON_POSITIVE_DENOMINATOR;
    }
    if (confidence_numerator > confidence_denominator) {
        return UNCERTAINTY_ERR_CONFIDENCE_OUT_OF_RANGE;
    }

    Uncertainty *uncertainty = (Uncertainty *)malloc(sizeof(Uncertainty));
    if (uncertainty == NULL) {
        return UNCERTAINTY_ERR_ALLOC;
    }

    uncertainty->kind = UNCERTAINTY_KIND_SCALAR_CONFIDENCE;
    uncertainty->confidence_numerator = confidence_numerator;
    uncertainty->confidence_denominator = confidence_denominator;
    uncertainty->candidates = NULL;
    uncertainty->candidate_count = 0;

    *out = uncertainty;
    return UNCERTAINTY_OK;
}

UncertaintyResult uncertainty_create_candidate_set(const UncertaintyCandidate *candidates,
                                                    size_t candidate_count, Uncertainty **out) {
    if (candidates == NULL || candidate_count == 0) {
        return UNCERTAINTY_ERR_NULL_CANDIDATES;
    }
    for (size_t i = 0; i < candidate_count; i++) {
        if (candidates[i].label == NULL) {
            return UNCERTAINTY_ERR_NULL_CANDIDATE_LABEL;
        }
    }

    Uncertainty *uncertainty = (Uncertainty *)malloc(sizeof(Uncertainty));
    if (uncertainty == NULL) {
        return UNCERTAINTY_ERR_ALLOC;
    }

    UncertaintyCandidate *owned = (UncertaintyCandidate *)malloc(sizeof(UncertaintyCandidate) * candidate_count);
    if (owned == NULL) {
        free(uncertainty);
        return UNCERTAINTY_ERR_ALLOC;
    }

    for (size_t i = 0; i < candidate_count; i++) {
        size_t len = strlen(candidates[i].label);
        owned[i].label = (char *)malloc(len + 1);
        if (owned[i].label == NULL) {
            for (size_t j = 0; j < i; j++) {
                free(owned[j].label);
            }
            free(owned);
            free(uncertainty);
            return UNCERTAINTY_ERR_ALLOC;
        }
        memcpy(owned[i].label, candidates[i].label, len + 1);
        owned[i].weight_numerator = candidates[i].weight_numerator;
        owned[i].weight_denominator = candidates[i].weight_denominator;
    }

    uncertainty->kind = UNCERTAINTY_KIND_CANDIDATE_SET;
    uncertainty->confidence_numerator = 0;
    uncertainty->confidence_denominator = 0;
    uncertainty->candidates = owned;
    uncertainty->candidate_count = candidate_count;

    *out = uncertainty;
    return UNCERTAINTY_OK;
}

void uncertainty_destroy(Uncertainty *uncertainty) {
    if (uncertainty == NULL) {
        return;
    }
    for (size_t i = 0; i < uncertainty->candidate_count; i++) {
        free(uncertainty->candidates[i].label);
    }
    free(uncertainty->candidates);
    free(uncertainty);
}

UncertaintyKind uncertainty_kind(const Uncertainty *uncertainty) {
    return uncertainty->kind;
}

unsigned long uncertainty_scalar_confidence_numerator(const Uncertainty *uncertainty) {
    return uncertainty->confidence_numerator;
}

unsigned long uncertainty_scalar_confidence_denominator(const Uncertainty *uncertainty) {
    return uncertainty->confidence_denominator;
}

size_t uncertainty_candidate_count(const Uncertainty *uncertainty) {
    return uncertainty->candidate_count;
}

const char *uncertainty_candidate_label_at(const Uncertainty *uncertainty, size_t index) {
    return uncertainty->candidates[index].label;
}

unsigned long uncertainty_candidate_weight_numerator_at(const Uncertainty *uncertainty, size_t index) {
    return uncertainty->candidates[index].weight_numerator;
}

unsigned long uncertainty_candidate_weight_denominator_at(const Uncertainty *uncertainty, size_t index) {
    return uncertainty->candidates[index].weight_denominator;
}