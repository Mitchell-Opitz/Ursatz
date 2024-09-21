#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "expansion_placement.h"

static void test_validity(void) {
    for (int i = 0; i < EXPANSION_PLACEMENT_COUNT; i++) {
        assert(expansion_placement_valid((ExpansionPlacement)i));
    }
    assert(!expansion_placement_valid(EXPANSION_PLACEMENT_COUNT));
    assert(!expansion_placement_valid((ExpansionPlacement)-1));
}

static void test_names(void) {
    assert(strcmp(expansion_placement_name(EXPANSION_PLACEMENT_INTERNAL), "internal") == 0);
    assert(strcmp(expansion_placement_name(EXPANSION_PLACEMENT_EXTERNAL), "external") == 0);
    assert(expansion_placement_name(EXPANSION_PLACEMENT_COUNT) == NULL);
}

int main(void) {
    test_validity();
    test_names();
    printf("test_expansion_placement: ok\n");
    return 0;
}
