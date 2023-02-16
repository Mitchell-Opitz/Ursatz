#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "semantics/expected_length_basis.h"

static void test_validity(void) {
    for (int i = 0; i < EXPECTED_LENGTH_BASIS_COUNT; i++) {
        assert(expected_length_basis_valid((ExpectedLengthBasis)i));
    }
    assert(!expected_length_basis_valid(EXPECTED_LENGTH_BASIS_COUNT));
    assert(!expected_length_basis_valid((ExpectedLengthBasis)-1));
}

static void test_names(void) {
    assert(strcmp(expected_length_basis_name(EXPECTED_LENGTH_BASIS_ANTICIPATED_CADENCE), "anticipated-cadence") == 0);
    assert(strcmp(expected_length_basis_name(EXPECTED_LENGTH_BASIS_UNEXPANDED_MODEL), "unexpanded-model") == 0);
    assert(expected_length_basis_name(EXPECTED_LENGTH_BASIS_COUNT) == NULL);
}

int main(void) {
    test_validity();
    test_names();
    printf("test_expected_length_basis: ok\n");
    return 0;
}
