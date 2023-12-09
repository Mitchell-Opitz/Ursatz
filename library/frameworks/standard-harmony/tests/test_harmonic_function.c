#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "harmonic_function.h"

static void test_validity(void) {
    for (int i = 0; i < HARMONIC_FUNCTION_COUNT; i++) {
        assert(harmonic_function_valid((HarmonicFunction)i));
    }
    assert(!harmonic_function_valid(HARMONIC_FUNCTION_COUNT));
    assert(!harmonic_function_valid((HarmonicFunction)-1));
}

static void test_names(void) {
    assert(strcmp(harmonic_function_name(HARMONIC_FUNCTION_TONIC), "tonic") == 0);
    assert(strcmp(harmonic_function_name(HARMONIC_FUNCTION_PREDOMINANT), "predominant") == 0);
    assert(strcmp(harmonic_function_name(HARMONIC_FUNCTION_DOMINANT), "dominant") == 0);
    assert(harmonic_function_name(HARMONIC_FUNCTION_COUNT) == NULL);
}

static void test_order(void) {
    assert(HARMONIC_FUNCTION_TONIC < HARMONIC_FUNCTION_PREDOMINANT);
    assert(HARMONIC_FUNCTION_PREDOMINANT < HARMONIC_FUNCTION_DOMINANT);
}

int main(void) {
    test_validity();
    test_names();
    test_order();
    printf("test_harmonic_function: ok\n");
    return 0;
}
