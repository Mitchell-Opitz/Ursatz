#include "harmony_predominant_seventh.h"

#include <stddef.h>

bool harmony_predominant_seventh_of(const HarmonyChord *chord, HarmonyPredominantSeventhKind *out) {
    if (out == NULL || !harmony_chord_valid(chord) || !chord->seventh || chord->cadential) {
        return false;
    }
    switch (chord->degree) {
        case 2:
            *out = (HarmonyPredominantSeventhKind)(HARMONY_PREDOMINANT_SEVENTH_II7 + chord->inversion);
            return true;
        case 4:
            *out = HARMONY_PREDOMINANT_SEVENTH_IV7;
            return true;
        case 6:
            *out = HARMONY_PREDOMINANT_SEVENTH_VI7;
            return true;
        case 3:
            *out = HARMONY_PREDOMINANT_SEVENTH_III7;
            return true;
        default:
            return false;
    }
}
