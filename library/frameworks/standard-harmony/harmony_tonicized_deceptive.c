#include "harmony_tonicized_deceptive.h"

#include <stddef.h>

bool harmony_tonicized_deceptive_is(HarmonyMode mode, const HarmonyChord *dominant,
                                    const HarmonyAppliedChord *applied, const HarmonyChord *resolution) {
    if (dominant == NULL || applied == NULL || resolution == NULL || !harmony_mode_valid(mode) ||
        !harmony_chord_valid(dominant) || !harmony_chord_valid(resolution) || !harmony_applied_valid(mode, applied)) {
        return false;
    }
    return dominant->degree == 5 && !dominant->cadential && applied->kind == APPLIED_CHORD_KIND_SECONDARY_LEADING_TONE &&
           applied->target == 6 && resolution->degree == 6 && resolution->inversion == 0 && !resolution->seventh;
}
