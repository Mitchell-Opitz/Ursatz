#include "theory/vocabulary/voice_part.h"

bool voice_part_valid(VoicePart part) {
    return part >= 0 && part < VOICE_PART_COUNT;
}
