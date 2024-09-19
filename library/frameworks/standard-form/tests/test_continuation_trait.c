#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "continuation_trait.h"

static void test_validity(void) {
    for (int i = 0; i < CONTINUATION_TRAIT_COUNT; i++) {
        assert(continuation_trait_valid((ContinuationTrait)i));
    }
    assert(!continuation_trait_valid(CONTINUATION_TRAIT_COUNT));
    assert(!continuation_trait_valid((ContinuationTrait)-1));
}

static void test_names(void) {
    assert(strcmp(continuation_trait_name(CONTINUATION_TRAIT_FRAGMENTATION), "fragmentation") == 0);
    assert(strcmp(continuation_trait_name(CONTINUATION_TRAIT_INCREASED_RHYTHMIC_ACTIVITY), "increased-rhythmic-activity") == 0);
    assert(strcmp(continuation_trait_name(CONTINUATION_TRAIT_SEQUENCE), "sequence") == 0);
    assert(strcmp(continuation_trait_name(CONTINUATION_TRAIT_INCREASED_HARMONIC_RHYTHM), "increased-harmonic-rhythm") == 0);
    assert(continuation_trait_name(CONTINUATION_TRAIT_COUNT) == NULL);
}

int main(void) {
    test_validity();
    test_names();
    printf("test_continuation_trait: ok\n");
    return 0;
}
