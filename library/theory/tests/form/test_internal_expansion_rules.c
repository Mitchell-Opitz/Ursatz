#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "theory/form/internal_expansion_rules.h"

static void test_combination(void) {
    unsigned all = 0;
    for (int i = 0; i < INTERNAL_EXPANSION_TECHNIQUE_COUNT; i++) {
        unsigned bit = standard_form_internal_technique_bit((InternalExpansionTechnique)i);
        assert(bit != 0);
        assert((all & bit) == 0);
        all |= bit;
    }
    assert(standard_form_internal_technique_bit(INTERNAL_EXPANSION_TECHNIQUE_COUNT) == 0);
}

/* A presentation's repeated basic idea is predictable, so it is not an expansion. */
static void test_repetition(void) {
    assert(standard_form_repetition_creates_expansion(true, false));
    assert(!standard_form_repetition_creates_expansion(false, false));
    assert(!standard_form_repetition_creates_expansion(true, true));
    assert(!standard_form_repetition_creates_expansion(false, true));
}

int main(void) {
    test_combination();
    test_repetition();
    printf("test_internal_expansion_rules (standard-form): ok\n");
    return 0;
}
