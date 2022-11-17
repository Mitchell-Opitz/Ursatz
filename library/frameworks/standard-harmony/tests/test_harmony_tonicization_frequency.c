#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "harmony_tonicization_frequency.h"

#define COMMON HARMONY_TONICIZATION_COMMON
#define FREQUENT HARMONY_TONICIZATION_FREQUENT
#define UNCOMMON HARMONY_TONICIZATION_UNCOMMON
#define NONE HARMONY_TONICIZATION_FREQUENCY_COUNT

static void test_values_and_names(void) {
    const char *const names[] = {"common", "frequent", "uncommon"};
    for (int i = 0; i < HARMONY_TONICIZATION_FREQUENCY_COUNT; i++) {
        assert(harmony_tonicization_frequency_valid((HarmonyTonicizationFrequency)i));
        assert(strcmp(harmony_tonicization_frequency_name((HarmonyTonicizationFrequency)i), names[i]) == 0);
    }
    assert(!harmony_tonicization_frequency_valid(NONE));
    assert(!harmony_tonicization_frequency_valid((HarmonyTonicizationFrequency)-1));
    assert(harmony_tonicization_frequency_name(NONE) == NULL);
}

static void test_major(void) {
    assert(harmony_tonicization_frequency(HARMONY_MODE_MAJOR, 5) == COMMON);
    assert(harmony_tonicization_frequency(HARMONY_MODE_MAJOR, 4) == COMMON);
    assert(harmony_tonicization_frequency(HARMONY_MODE_MAJOR, 2) == COMMON);
    assert(harmony_tonicization_frequency(HARMONY_MODE_MAJOR, 6) == FREQUENT);
    assert(harmony_tonicization_frequency(HARMONY_MODE_MAJOR, 3) == UNCOMMON);
}

static void test_minor(void) {
    assert(harmony_tonicization_frequency(HARMONY_MODE_MINOR, 5) == COMMON);
    assert(harmony_tonicization_frequency(HARMONY_MODE_MINOR, 4) == COMMON);
    assert(harmony_tonicization_frequency(HARMONY_MODE_MINOR, 6) == FREQUENT);
    assert(harmony_tonicization_frequency(HARMONY_MODE_MINOR, 3) == FREQUENT);
    assert(harmony_tonicization_frequency(HARMONY_MODE_MINOR, 7) == UNCOMMON);
}

static void test_a_chord_that_cannot_be_tonicized(void) {
    assert(harmony_tonicization_frequency(HARMONY_MODE_MAJOR, 1) == NONE);
    assert(harmony_tonicization_frequency(HARMONY_MODE_MAJOR, 7) == NONE);
    assert(harmony_tonicization_frequency(HARMONY_MODE_MINOR, 2) == NONE);
    assert(harmony_tonicization_frequency(HARMONY_MODE_MINOR, 1) == NONE);
    assert(harmony_tonicization_frequency(HARMONY_MODE_COUNT, 5) == NONE);
}

int main(void) {
    test_values_and_names();
    test_major();
    test_minor();
    test_a_chord_that_cannot_be_tonicized();
    printf("test_harmony_tonicization_frequency: all tests passed\n");
    return 0;
}
