#include "harmony_mediant.h"

#include <stddef.h>

#include "harmony_strong_predominants.h"

bool harmony_mediant_kind_valid(HarmonyMediantKind kind) {
    return kind >= 0 && kind < HARMONY_MEDIANT_KIND_COUNT;
}

const char *harmony_mediant_kind_name(HarmonyMediantKind kind) {
    switch (kind) {
        case HARMONY_MEDIANT_TO_PREDOMINANT:
            return "to-predominant";
        case HARMONY_MEDIANT_TO_DOMINANT:
            return "to-dominant";
        default:
            return NULL;
    }
}

bool harmony_mediant_of(HarmonyMode mode, const HarmonyChord chords[3], HarmonyMediantKind *out) {
    if (chords == NULL || out == NULL || !harmony_mode_valid(mode)) {
        return false;
    }
    for (int i = 0; i < 3; i++) {
        if (!harmony_chord_valid(&chords[i]) || chords[i].seventh || chords[i].cadential) {
            return false;
        }
    }
    const HarmonyChord *tonic = &chords[0];
    const HarmonyChord *mediant = &chords[1];
    const HarmonyChord *next = &chords[2];
    if (tonic->degree != 1 || tonic->inversion != 0 || mediant->degree != 3 || mediant->inversion > 1) {
        return false;
    }
    HarmonyStrongPredominant predominant;
    if (harmony_strong_predominant_of(mode, next, &predominant)) {
        *out = HARMONY_MEDIANT_TO_PREDOMINANT;
        return true;
    }
    if (next->degree == 5 && next->inversion == 0) {
        *out = HARMONY_MEDIANT_TO_DOMINANT;
        return true;
    }
    return false;
}
