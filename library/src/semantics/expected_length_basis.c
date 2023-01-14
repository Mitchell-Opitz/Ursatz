#include "semantics/expected_length_basis.h"

#include <stddef.h>

static const char *const NAMES[EXPECTED_LENGTH_BASIS_COUNT] = {"anticipated-cadence", "unexpanded-model"};

bool expected_length_basis_valid(ExpectedLengthBasis value) {
    return value >= 0 && value < EXPECTED_LENGTH_BASIS_COUNT;
}

const char *expected_length_basis_name(ExpectedLengthBasis value) {
    return expected_length_basis_valid(value) ? NAMES[value] : NULL;
}
