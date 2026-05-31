#include "harmony_key_change.h"

#include <math.h>
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

bool harmony_key_change_confirm(KeyChangeExtent length_extent, double region_start, double region_end,
                                const double *cadence_arrivals, size_t cadence_count, KeyChangeExtent *out) {
    if (out == NULL || !key_change_extent_valid(length_extent) || !isfinite(region_start) || !isfinite(region_end) ||
        region_end < region_start) {
        return false;
    }
    if (cadence_arrivals == NULL) {
        *out = length_extent;
        return true;
    }
    bool confirmed = false;
    for (size_t i = 0; i < cadence_count && !confirmed; i++) {
        confirmed = cadence_arrivals[i] >= region_start - 1e-9 && cadence_arrivals[i] < region_end - 1e-9;
    }
    if (confirmed) {
        *out = key_change_extent_is_modulation(length_extent) ? length_extent : KEY_CHANGE_EXTENT_WEAK_MODULATION;
    } else {
        *out = length_extent == KEY_CHANGE_EXTENT_EXTENDED_TONICIZATION ? length_extent : KEY_CHANGE_EXTENT_TONICIZATION;
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
    char label[8];
    int length = snprintf(label, sizeof(label), "%s", harmony_triad_numeral(home, key->degree));
    if (length < 0 || (size_t)length >= size) {
        return false;
    }
    snprintf(out, size, "%s", label);
    return true;
}
