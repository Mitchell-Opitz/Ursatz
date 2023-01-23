#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "semantics/internal_expansion_technique.h"

static void test_validity(void) {
    for (int i = 0; i < INTERNAL_EXPANSION_TECHNIQUE_COUNT; i++) {
        assert(internal_expansion_technique_valid((InternalExpansionTechnique)i));
    }
    assert(!internal_expansion_technique_valid(INTERNAL_EXPANSION_TECHNIQUE_COUNT));
    assert(!internal_expansion_technique_valid((InternalExpansionTechnique)-1));
}

static void test_names(void) {
    assert(strcmp(internal_expansion_technique_name(INTERNAL_EXPANSION_TECHNIQUE_REPETITION), "repetition") == 0);
    assert(strcmp(internal_expansion_technique_name(INTERNAL_EXPANSION_TECHNIQUE_STRETCHING), "stretching") == 0);
    assert(strcmp(internal_expansion_technique_name(INTERNAL_EXPANSION_TECHNIQUE_ONE_MORE_TIME), "one-more-time") == 0);
    assert(strcmp(internal_expansion_technique_name(INTERNAL_EXPANSION_TECHNIQUE_ALTERNATIVE_PATH), "alternative-path") == 0);
    assert(internal_expansion_technique_name(INTERNAL_EXPANSION_TECHNIQUE_COUNT) == NULL);
}

int main(void) {
    test_validity();
    test_names();
    printf("test_internal_expansion_technique: ok\n");
    return 0;
}
