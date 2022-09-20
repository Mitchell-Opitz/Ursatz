#include "harmony_plagal.h"

#include <stddef.h>

bool harmony_plagal_placement_valid(HarmonyPlagalPlacement placement) {
    return placement >= 0 && placement < HARMONY_PLAGAL_PLACEMENT_COUNT;
}

const char *harmony_plagal_placement_name(HarmonyPlagalPlacement placement) {
    switch (placement) {
        case HARMONY_PLAGAL_AFTER_CADENCE:
            return "after-cadence";
        case HARMONY_PLAGAL_PHRASE_BEGINNING:
            return "phrase-beginning";
        default:
            return NULL;
    }
}

bool harmony_plagal_progression_is(const HarmonyChord chords[2], HarmonyPlagalPlacement placement) {
    if (chords == NULL || !harmony_plagal_placement_valid(placement)) {
        return false;
    }
    for (int i = 0; i < 2; i++) {
        if (!harmony_chord_valid(&chords[i]) || chords[i].seventh || chords[i].cadential) {
            return false;
        }
    }
    const HarmonyChord *iv = &chords[0];
    const HarmonyChord *tonic = &chords[1];
    if (iv->degree != 4 || iv->inversion != 0 || tonic->degree != 1) {
        return false;
    }
    return tonic->inversion == 0 || (tonic->inversion == 1 && placement == HARMONY_PLAGAL_PHRASE_BEGINNING);
}
