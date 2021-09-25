#include "semantics/beat_strength.h"

#include <stddef.h>

static const char *const NAMES[BEAT_STRENGTH_COUNT] = {"strong", "medium", "weak"};

bool beat_strength_valid(BeatStrength strength) {
    return strength >= 0 && strength < BEAT_STRENGTH_COUNT;
}

const char *beat_strength_name(BeatStrength strength) {
    return beat_strength_valid(strength) ? NAMES[strength] : NULL;
}

bool beat_strength_is_stronger(BeatStrength a, BeatStrength b) {
    return beat_strength_valid(a) && beat_strength_valid(b) && a < b;
}
