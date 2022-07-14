#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "semantics/seventh_resolution.h"

static void test_validity(void) {
    for (int i = 0; i < SEVENTH_RESOLUTION_COUNT; i++) {
        assert(seventh_resolution_valid((SeventhResolution)i));
    }
    assert(!seventh_resolution_valid(SEVENTH_RESOLUTION_COUNT));
    assert(!seventh_resolution_valid((SeventhResolution)-1));
}

static void test_names(void) {
    assert(strcmp(seventh_resolution_name(SEVENTH_RESOLUTION_DEFAULT), "default") == 0);
    assert(strcmp(seventh_resolution_name(SEVENTH_RESOLUTION_INCOMPLETE), "incomplete") == 0);
    assert(strcmp(seventh_resolution_name(SEVENTH_RESOLUTION_LEADING_TONE_DROP), "leading-tone-drop") == 0);
    assert(seventh_resolution_name(SEVENTH_RESOLUTION_COUNT) == NULL);
}

static void test_gives_complete_tonic(void) {
    assert(!seventh_resolution_gives_complete_tonic(SEVENTH_RESOLUTION_DEFAULT));
    assert(seventh_resolution_gives_complete_tonic(SEVENTH_RESOLUTION_INCOMPLETE));
    assert(seventh_resolution_gives_complete_tonic(SEVENTH_RESOLUTION_LEADING_TONE_DROP));
    assert(!seventh_resolution_gives_complete_tonic(SEVENTH_RESOLUTION_COUNT));
}

int main(void) {
    test_validity();
    test_names();
    test_gives_complete_tonic();
    printf("test_seventh_resolution: ok\n");
    return 0;
}
