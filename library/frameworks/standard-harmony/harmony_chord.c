#include "harmony_chord.h"

#include <stddef.h>

static const char *const NAMES[HARMONY_MODE_COUNT] = {"major", "minor"};

bool harmony_mode_valid(HarmonyMode mode) {
    return mode >= 0 && mode < HARMONY_MODE_COUNT;
}

const char *harmony_mode_name(HarmonyMode mode) {
    return harmony_mode_valid(mode) ? NAMES[mode] : NULL;
}

bool harmony_chord_valid(const HarmonyChord *chord) {
    return chord != NULL && chord->degree >= 1 && chord->degree <= 7 && chord->inversion >= 0 &&
           chord->inversion <= 3 && (chord->inversion < 3 || chord->seventh) &&
           (!chord->leading_tone || chord->degree == 7) &&
           (!chord->cadential || (chord->degree == 1 && chord->inversion == 2 && !chord->seventh)) &&
           (!chord->half_diminished || (chord->degree == 7 && chord->seventh));
}
