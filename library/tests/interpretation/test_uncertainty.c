#include <assert.h>
#include <stdio.h>

#include "interpretation/uncertainty.h"

/*
 * Invariant test: a scalar confidence is an exact fraction in [0, 1]. It reads back as given (not reduced, not
 * rounded), reports the scalar-confidence kind, and a value outside [0, 1] or over a zero denominator is rejected.
 */

int main(void) {
    Uncertainty *scalar;
    assert(uncertainty_create_scalar_confidence(3, 4, &scalar) == UNCERTAINTY_OK);
    assert(uncertainty_kind(scalar) == UNCERTAINTY_KIND_SCALAR_CONFIDENCE);
    assert(uncertainty_scalar_confidence_numerator(scalar) == 3);
    assert(uncertainty_scalar_confidence_denominator(scalar) == 4);
    uncertainty_destroy(scalar);

    /* The ends of the range and an unreduced fraction are kept as given. */
    Uncertainty *zero;
    assert(uncertainty_create_scalar_confidence(0, 1, &zero) == UNCERTAINTY_OK);
    assert(uncertainty_scalar_confidence_numerator(zero) == 0 && uncertainty_scalar_confidence_denominator(zero) == 1);
    uncertainty_destroy(zero);
    Uncertainty *one;
    assert(uncertainty_create_scalar_confidence(6, 6, &one) == UNCERTAINTY_OK);
    assert(uncertainty_scalar_confidence_numerator(one) == 6 && uncertainty_scalar_confidence_denominator(one) == 6);
    uncertainty_destroy(one);

    /* A confidence outside [0, 1] or a zero denominator is rejected. */
    Uncertainty *rejected;
    assert(uncertainty_create_scalar_confidence(5, 4, &rejected) == UNCERTAINTY_ERR_CONFIDENCE_OUT_OF_RANGE);
    assert(uncertainty_create_scalar_confidence(1, 0, &rejected) == UNCERTAINTY_ERR_NON_POSITIVE_DENOMINATOR);

    uncertainty_destroy(NULL);

    printf("test_uncertainty: ok\n");
    return 0;
}
