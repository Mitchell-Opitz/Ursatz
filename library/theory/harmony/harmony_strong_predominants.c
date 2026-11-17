#include "theory/harmony/harmony_strong_predominants.h"

#include <stddef.h>

#include "theory/harmony/harmony_functions.h"

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
