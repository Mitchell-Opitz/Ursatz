#include <assert.h>
#include <stdio.h>

#include "semantics/voice_part.h"

static void test_validity(void) {
    for (int i = 0; i < VOICE_PART_COUNT; i++) {
        assert(voice_part_valid((VoicePart)i));
    }
    assert(!voice_part_valid(VOICE_PART_COUNT));
    assert(!voice_part_valid((VoicePart)-1));
}

int main(void) {
    test_validity();
    printf("test_voice_part: ok\n");
    return 0;
}
