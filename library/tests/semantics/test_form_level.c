#include <assert.h>
#include <stdio.h>

#include "semantics/form_level.h"

static void test_validity(void) {
    for (int i = 0; i < FORM_LEVEL_COUNT; i++) {
        assert(form_level_valid((FormLevel)i));
    }
    assert(!form_level_valid(FORM_LEVEL_COUNT));
    assert(!form_level_valid((FormLevel)-1));
}

int main(void) {
    test_validity();
    printf("test_form_level: ok\n");
    return 0;
}
