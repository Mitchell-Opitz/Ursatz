#include "model/interpretation/uncertainty.h"

#include <stdlib.h>

struct Uncertainty {
    UncertaintyKind kind;

    unsigned long confidence_numerator;
    unsigned long confidence_denominator;
};

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

    *out = uncertainty;
    return UNCERTAINTY_OK;
}

void uncertainty_destroy(Uncertainty *uncertainty) {
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
