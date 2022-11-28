#include "harmony_key_change.h"

#include <stdio.h>

bool harmony_key_change_classify(const HarmonyKeyChange *change, KeyChangeExtent *out) {
    if (change == NULL || out == NULL || change->chords_in_new_key < 1 ||
        (change->continues_after_cadence && !change->cadence_in_new_key)) {
        return false;
    }
    if (change->cadence_in_new_key) {
        *out = change->continues_after_cadence ? KEY_CHANGE_EXTENT_MODULATION : KEY_CHANGE_EXTENT_WEAK_MODULATION;
    } else {
        *out = change->chords_in_new_key <= 2 ? KEY_CHANGE_EXTENT_TONICIZATION
                                              : KEY_CHANGE_EXTENT_EXTENDED_TONICIZATION;
    }
    return true;
}

ModulationKind harmony_modulation_kind(const HarmonyPivot *pivot) {
    return pivot != NULL ? MODULATION_KIND_PIVOT : MODULATION_KIND_DIRECT;
}

bool harmony_extended_tonicization_label(HarmonyMode home, const HarmonyRelatedKey *key, char *out, size_t size) {
    if (out == NULL || !harmony_key_is_closely_related(home, key)) {
        return false;
    }
    const char *numeral = harmony_triad_numeral(home, key->degree);
    int length = snprintf(out, size, "%s", numeral);
    return length >= 0 && (size_t)length < size;
}
