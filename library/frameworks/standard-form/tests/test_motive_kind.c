#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "motive_kind.h"

static void test_validity(void) {
    for (int i = 0; i < MOTIVE_KIND_COUNT; i++) {
        assert(motive_kind_valid((MotiveKind)i));
    }
    assert(!motive_kind_valid(MOTIVE_KIND_COUNT));
    assert(!motive_kind_valid((MotiveKind)-1));
}

static void test_names(void) {
    assert(strcmp(motive_kind_name(MOTIVE_KIND_PITCH), "pitch") == 0);
    assert(strcmp(motive_kind_name(MOTIVE_KIND_RHYTHMIC), "rhythmic") == 0);
    assert(strcmp(motive_kind_name(MOTIVE_KIND_CONTOUR), "contour") == 0);
    assert(strcmp(motive_kind_name(MOTIVE_KIND_TIMBRE), "timbre") == 0);
    assert(motive_kind_name(MOTIVE_KIND_COUNT) == NULL);
}

int main(void) {
    test_validity();
    test_names();
    printf("test_motive_kind: ok\n");
    return 0;
}
