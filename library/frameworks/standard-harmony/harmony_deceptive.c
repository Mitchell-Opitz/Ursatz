#include "harmony_deceptive.h"

#include <stddef.h>

bool harmony_deceptive_of(const HarmonyChord chords[2], HarmonyDeceptiveKind *out) {
    if (chords == NULL || out == NULL) {
        return false;
    }
    for (int i = 0; i < 2; i++) {
        if (!harmony_chord_valid(&chords[i]) || chords[i].cadential) {
            return false;
        }
    }
    const HarmonyChord *dominant = &chords[0];
    const HarmonyChord *to = &chords[1];
    if (dominant->degree != 5 || dominant->inversion != 0 || to->seventh) {
        return false;
    }
    if (to->degree == 6 && to->inversion == 0) {
        *out = HARMONY_DECEPTIVE_VI;
        return true;
    }
    if (to->degree == 4 && to->inversion == 1) {
        *out = HARMONY_DECEPTIVE_IV6;
        return true;
    }
    return false;
}
