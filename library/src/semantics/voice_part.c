#include "semantics/voice_part.h"

#include <stddef.h>

static const char *const NAMES[VOICE_PART_COUNT] = {"soprano", "alto", "tenor", "bass"};

bool voice_part_valid(VoicePart part) {
    return part >= 0 && part < VOICE_PART_COUNT;
}

const char *voice_part_name(VoicePart part) {
    return voice_part_valid(part) ? NAMES[part] : NULL;
}

bool voice_part_is_above(VoicePart a, VoicePart b) {
    return voice_part_valid(a) && voice_part_valid(b) && a < b;
}
