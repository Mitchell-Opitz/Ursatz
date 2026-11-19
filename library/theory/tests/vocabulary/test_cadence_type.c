#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "theory/vocabulary/cadence_type.h"

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
    assert(strcmp(cadence_type_name(CADENCE_TYPE_PLAGAL), "plagal") == 0);
    assert(strcmp(cadence_type_name(CADENCE_TYPE_DECEPTIVE), "deceptive") == 0);
    assert(strcmp(cadence_type_name(CADENCE_TYPE_AUTHENTIC_UNSPECIFIED), "authentic-unspecified") == 0);
    assert(strcmp(cadence_type_name(CADENCE_TYPE_UNKNOWN), "unknown") == 0);
    assert(cadence_type_name(CADENCE_TYPE_COUNT) == NULL);
}

static void test_from_name(void) {
    for (int i = 0; i < CADENCE_TYPE_COUNT; i++) {
        CadenceType type = CADENCE_TYPE_COUNT;
        assert(cadence_type_from_name(cadence_type_name((CadenceType)i), &type) && type == (CadenceType)i);
    }
    CadenceType untouched = CADENCE_TYPE_HALF;
    assert(!cadence_type_from_name("PAC", &untouched) && !cadence_type_from_name("", &untouched) && !cadence_type_from_name(NULL, &untouched));
    assert(!cadence_type_from_name("half", NULL) && untouched == CADENCE_TYPE_HALF);
}

static void test_is_authentic(void) {
    assert(cadence_type_is_authentic(CADENCE_TYPE_PERFECT_AUTHENTIC));
    assert(cadence_type_is_authentic(CADENCE_TYPE_IMPERFECT_AUTHENTIC));
    assert(!cadence_type_is_authentic(CADENCE_TYPE_HALF));
    assert(!cadence_type_is_authentic(CADENCE_TYPE_SUBVERTED));
    assert(!cadence_type_is_authentic(CADENCE_TYPE_PLAGAL));
    assert(!cadence_type_is_authentic(CADENCE_TYPE_DECEPTIVE));
    assert(!cadence_type_is_authentic(CADENCE_TYPE_AUTHENTIC_UNSPECIFIED));
    assert(!cadence_type_is_authentic(CADENCE_TYPE_UNKNOWN));
    assert(!cadence_type_is_authentic(CADENCE_TYPE_COUNT));
}

int main(void) {
    test_validity();
    test_names();
    test_from_name();
    test_is_authentic();
    printf("test_cadence_type: ok\n");
    return 0;
}
