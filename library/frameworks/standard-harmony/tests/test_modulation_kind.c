#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "modulation_kind.h"

static void test_validity(void) {
    for (int i = 0; i < MODULATION_KIND_COUNT; i++) {
        assert(modulation_kind_valid((ModulationKind)i));
    }
    assert(!modulation_kind_valid(MODULATION_KIND_COUNT));
    assert(!modulation_kind_valid((ModulationKind)-1));
}

static void test_names(void) {
    assert(strcmp(modulation_kind_name(MODULATION_KIND_DIRECT), "direct") == 0);
    assert(strcmp(modulation_kind_name(MODULATION_KIND_PIVOT), "pivot") == 0);
    assert(modulation_kind_name(MODULATION_KIND_COUNT) == NULL);
}

int main(void) {
    test_validity();
    test_names();
    printf("test_modulation_kind: ok\n");
    return 0;
}
