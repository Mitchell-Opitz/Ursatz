#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "semantics/voice_part.h"

static void test_validity(void) {
    for (int i = 0; i < VOICE_PART_COUNT; i++) {
        assert(voice_part_valid((VoicePart)i));
    }
    assert(!voice_part_valid(VOICE_PART_COUNT));
    assert(!voice_part_valid((VoicePart)-1));
}

static void test_names(void) {
    assert(strcmp(voice_part_name(VOICE_PART_SOPRANO), "soprano") == 0);
    assert(strcmp(voice_part_name(VOICE_PART_ALTO), "alto") == 0);
    assert(strcmp(voice_part_name(VOICE_PART_TENOR), "tenor") == 0);
    assert(strcmp(voice_part_name(VOICE_PART_BASS), "bass") == 0);
    assert(voice_part_name(VOICE_PART_COUNT) == NULL);
}

static void test_order(void) {
    assert(voice_part_is_above(VOICE_PART_SOPRANO, VOICE_PART_ALTO));
    assert(voice_part_is_above(VOICE_PART_ALTO, VOICE_PART_TENOR));
    assert(voice_part_is_above(VOICE_PART_TENOR, VOICE_PART_BASS));
    assert(voice_part_is_above(VOICE_PART_SOPRANO, VOICE_PART_BASS));
    assert(!voice_part_is_above(VOICE_PART_BASS, VOICE_PART_SOPRANO));
    assert(!voice_part_is_above(VOICE_PART_ALTO, VOICE_PART_ALTO));
    assert(!voice_part_is_above(VOICE_PART_COUNT, VOICE_PART_BASS));
    assert(!voice_part_is_above(VOICE_PART_SOPRANO, VOICE_PART_COUNT));
}

int main(void) {
    test_validity();
    test_names();
    test_order();
    printf("test_voice_part: ok\n");
    return 0;
}
