#include "harmony_phrygian.h"

#include <stddef.h>

bool harmony_phrygian_half_cadence_is(HarmonyMode mode, const HarmonyChord chords[2]) {
    if (mode != HARMONY_MODE_MINOR || chords == NULL) {
        return false;
    }
    for (int i = 0; i < 2; i++) {
        if (!harmony_chord_valid(&chords[i]) || chords[i].cadential) {
            return false;
        }
    }
    const HarmonyChord *subdominant = &chords[0];
    const HarmonyChord *dominant = &chords[1];
    return subdominant->degree == 4 && subdominant->inversion == 1 && !subdominant->seventh &&
           dominant->degree == 5 && dominant->inversion == 0;
}
