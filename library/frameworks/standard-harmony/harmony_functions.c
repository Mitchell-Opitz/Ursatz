#include "harmony_functions.h"

#include <stddef.h>

typedef struct {
    HarmonicFunction function;
    HarmonyPredominantStrength strength; /* only for a predominant */
} Entry;

#define TONIC {HARMONIC_FUNCTION_TONIC, HARMONY_PREDOMINANT_WEAK}
#define DOMINANT {HARMONIC_FUNCTION_DOMINANT, HARMONY_PREDOMINANT_WEAK}
#define STRONG {HARMONIC_FUNCTION_PREDOMINANT, HARMONY_PREDOMINANT_STRONG}
#define WEAK {HARMONIC_FUNCTION_PREDOMINANT, HARMONY_PREDOMINANT_WEAK}

/* Indexed by degree - 1. In minor, degree 7 is VII (weak); vii is below. */
static const Entry MAJOR[7] = {TONIC, STRONG, WEAK, STRONG, DOMINANT, WEAK, DOMINANT};
static const Entry MINOR[7] = {TONIC, STRONG, WEAK, STRONG, DOMINANT, WEAK, WEAK};
static const Entry MINOR_LEADING_TONE = DOMINANT;

static const Entry *lookup(HarmonyMode mode, const HarmonyChord *chord) {
    if (!harmony_mode_valid(mode) || !harmony_chord_valid(chord)) {
        return NULL;
    }
    if (mode == HARMONY_MODE_MAJOR) {
        return &MAJOR[chord->degree - 1];
    }
    return chord->leading_tone ? &MINOR_LEADING_TONE : &MINOR[chord->degree - 1];
}

bool harmony_function_of(HarmonyMode mode, const HarmonyChord *chord, HarmonicFunction *out) {
    const Entry *entry = lookup(mode, chord);
    if (entry == NULL || out == NULL) {
        return false;
    }
    *out = entry->function;
    return true;
}

bool harmony_predominant_strength(HarmonyMode mode, const HarmonyChord *chord, HarmonyPredominantStrength *out) {
    const Entry *entry = lookup(mode, chord);
    if (entry == NULL || out == NULL || entry->function != HARMONIC_FUNCTION_PREDOMINANT) {
        return false;
    }
    *out = entry->strength;
    return true;
}
