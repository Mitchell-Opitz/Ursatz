#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "semantics/applied_chord_kind.h"

static void test_validity(void) {
    for (int i = 0; i < APPLIED_CHORD_KIND_COUNT; i++) {
        assert(applied_chord_kind_valid((AppliedChordKind)i));
    }
    assert(!applied_chord_kind_valid(APPLIED_CHORD_KIND_COUNT));
    assert(!applied_chord_kind_valid((AppliedChordKind)-1));
}

static void test_names(void) {
    assert(strcmp(applied_chord_kind_name(APPLIED_CHORD_KIND_SECONDARY_DOMINANT), "secondary-dominant") == 0);
    assert(strcmp(applied_chord_kind_name(APPLIED_CHORD_KIND_SECONDARY_LEADING_TONE), "secondary-leading-tone") == 0);
    assert(applied_chord_kind_name(APPLIED_CHORD_KIND_COUNT) == NULL);
}

int main(void) {
    test_validity();
    test_names();
    printf("test_applied_chord_kind: ok\n");
    return 0;
}
