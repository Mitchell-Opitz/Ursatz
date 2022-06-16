#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "semantics/cadence_type.h"

static void test_validity(void) {
    for (int i = 0; i < CADENCE_TYPE_COUNT; i++) {
        assert(cadence_type_valid((CadenceType)i));
    }
    assert(!cadence_type_valid(CADENCE_TYPE_COUNT));
    assert(!cadence_type_valid((CadenceType)-1));
}

static void test_names(void) {
    assert(strcmp(cadence_type_name(CADENCE_TYPE_PERFECT_AUTHENTIC), "perfect-authentic") == 0);
    assert(strcmp(cadence_type_name(CADENCE_TYPE_IMPERFECT_AUTHENTIC), "imperfect-authentic") == 0);
    assert(strcmp(cadence_type_name(CADENCE_TYPE_HALF), "half") == 0);
    assert(strcmp(cadence_type_name(CADENCE_TYPE_SUBVERTED), "subverted") == 0);
    assert(cadence_type_name(CADENCE_TYPE_COUNT) == NULL);
}

static void test_is_authentic(void) {
    assert(cadence_type_is_authentic(CADENCE_TYPE_PERFECT_AUTHENTIC));
    assert(cadence_type_is_authentic(CADENCE_TYPE_IMPERFECT_AUTHENTIC));
    assert(!cadence_type_is_authentic(CADENCE_TYPE_HALF));
    assert(!cadence_type_is_authentic(CADENCE_TYPE_SUBVERTED));
    assert(!cadence_type_is_authentic(CADENCE_TYPE_COUNT));
}

int main(void) {
    test_validity();
    test_names();
    test_is_authentic();
    printf("test_cadence_type: ok\n");
    return 0;
}
