#include "harmony_strong_predominants.h"

#include <stddef.h>

#include "harmony_functions.h"

static const char *const NAMES[HARMONY_STRONG_PREDOMINANT_COUNT] = {"iv", "ii6", "ii"};

bool harmony_strong_predominant_valid(HarmonyStrongPredominant kind) {
    return kind >= 0 && kind < HARMONY_STRONG_PREDOMINANT_COUNT;
}

const char *harmony_strong_predominant_name(HarmonyStrongPredominant kind) {
    return harmony_strong_predominant_valid(kind) ? NAMES[kind] : NULL;
}

bool harmony_strong_predominant_of(HarmonyMode mode, const HarmonyChord *chord, HarmonyStrongPredominant *out) {
    HarmonyPredominantStrength strength;
    if (out == NULL || !harmony_predominant_strength(mode, chord, &strength) ||
        strength != HARMONY_PREDOMINANT_STRONG || chord->seventh) {
        return false;
    }
    if (chord->degree == 4 && chord->inversion == 0) {
        *out = HARMONY_STRONG_PREDOMINANT_IV;
        return true;
    }
    if (chord->degree == 2 && chord->inversion == 1) {
        *out = HARMONY_STRONG_PREDOMINANT_II6;
        return true;
    }
    if (chord->degree == 2 && chord->inversion == 0) {
        *out = HARMONY_STRONG_PREDOMINANT_II;
        return true;
    }
    return false;
}
