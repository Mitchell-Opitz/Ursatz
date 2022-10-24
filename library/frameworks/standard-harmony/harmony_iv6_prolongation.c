#include "harmony_iv6_prolongation.h"

#include <stddef.h>

bool harmony_iv6_prolongation_kind_valid(HarmonyIv6ProlongationKind kind) {
    return kind >= 0 && kind < HARMONY_IV6_PROLONGATION_KIND_COUNT;
}

const char *harmony_iv6_prolongation_kind_name(HarmonyIv6ProlongationKind kind) {
    switch (kind) {
        case HARMONY_IV6_PROLONGATION_V:
            return "v";
        case HARMONY_IV6_PROLONGATION_V6:
            return "v6";
        default:
            return NULL;
    }
}

static bool is_triad(const HarmonyChord *chord, int degree, int inversion) {
    return chord->degree == degree && chord->inversion == inversion && !chord->seventh && !chord->cadential;
}

bool harmony_iv6_prolongation_of(const HarmonyChord chords[4], HarmonyIv6ProlongationKind *out) {
    if (chords == NULL || out == NULL) {
        return false;
    }
    for (int i = 0; i < 4; i++) {
        if (!harmony_chord_valid(&chords[i])) {
            return false;
        }
    }
    if (!is_triad(&chords[0], 1, 0) || !is_triad(&chords[1], 4, 1) || !is_triad(&chords[3], 1, 0)) {
        return false;
    }
    if (is_triad(&chords[2], 5, 0)) {
        *out = HARMONY_IV6_PROLONGATION_V;
        return true;
    }
    if (is_triad(&chords[2], 5, 1)) {
        *out = HARMONY_IV6_PROLONGATION_V6;
        return true;
    }
    return false;
}
