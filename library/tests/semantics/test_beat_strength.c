#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "semantics/beat_strength.h"

static void test_validity(void) {
    for (int i = 0; i < BEAT_STRENGTH_COUNT; i++) {
        assert(beat_strength_valid((BeatStrength)i));
    }
    assert(!beat_strength_valid(BEAT_STRENGTH_COUNT));
    assert(!beat_strength_valid((BeatStrength)-1));
}

static void test_names(void) {
    assert(strcmp(beat_strength_name(BEAT_STRENGTH_STRONG), "strong") == 0);
    assert(strcmp(beat_strength_name(BEAT_STRENGTH_WEAK), "weak") == 0);
    assert(beat_strength_name(BEAT_STRENGTH_COUNT) == NULL);
}

static void test_is_stronger(void) {
    assert(beat_strength_is_stronger(BEAT_STRENGTH_STRONG, BEAT_STRENGTH_WEAK));
    assert(!beat_strength_is_stronger(BEAT_STRENGTH_WEAK, BEAT_STRENGTH_STRONG));
    assert(!beat_strength_is_stronger(BEAT_STRENGTH_STRONG, BEAT_STRENGTH_STRONG));
    assert(!beat_strength_is_stronger(BEAT_STRENGTH_WEAK, BEAT_STRENGTH_WEAK));
    assert(!beat_strength_is_stronger(BEAT_STRENGTH_COUNT, BEAT_STRENGTH_WEAK));
    assert(!beat_strength_is_stronger(BEAT_STRENGTH_STRONG, BEAT_STRENGTH_COUNT));
}

int main(void) {
    test_validity();
    test_names();
    test_is_stronger();
    printf("test_beat_strength: ok\n");
    return 0;
}
