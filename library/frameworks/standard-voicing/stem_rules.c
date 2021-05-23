#include "stem_rules.h"

#include <stddef.h>

bool standard_voicing_staff_for_part(VoicePart part, StandardVoicingStaff *out) {
    if (out == NULL || !voice_part_valid(part)) {
        return false;
    }
    *out = (part == VOICE_PART_SOPRANO || part == VOICE_PART_ALTO) ? STANDARD_VOICING_STAFF_TREBLE
                                                                    : STANDARD_VOICING_STAFF_BASS;
    return true;
}

bool standard_voicing_stem_direction_for_part(VoicePart part, StandardVoicingStemDirection *out) {
    if (out == NULL || !voice_part_valid(part)) {
        return false;
    }
    *out = (part == VOICE_PART_SOPRANO || part == VOICE_PART_TENOR) ? STANDARD_VOICING_STEM_UP
                                                                     : STANDARD_VOICING_STEM_DOWN;
    return true;
}

bool standard_voicing_stem_direction_valid(VoicePart part, StandardVoicingStemDirection direction) {
    StandardVoicingStemDirection required;
    if (!standard_voicing_stem_direction_for_part(part, &required)) {
        return false;
    }
    return direction == STANDARD_VOICING_STEM_UP || direction == STANDARD_VOICING_STEM_DOWN
               ? direction == required
               : false;
}

bool standard_voicing_stems_valid(const StandardVoicingStemDirection directions[VOICE_PART_COUNT]) {
    if (directions == NULL) {
        return false;
    }
    for (int i = 0; i < VOICE_PART_COUNT; i++) {
        if (!standard_voicing_stem_direction_valid((VoicePart)i, directions[i])) {
            return false;
        }
    }
    return true;
}
