#include "theory/harmony/harmony_key_change.h"

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

bool harmony_key_change_confirm(KeyChangeExtent length_extent, Rational region_start, Rational region_end,
                                const Rational *cadence_arrivals, size_t cadence_count, KeyChangeExtent *out) {
    if (out == NULL || !key_change_extent_valid(length_extent) || rational_cmp(region_end, region_start) < 0) {
        return false;
    }
    if (cadence_arrivals == NULL) {
        *out = length_extent;
        return true;
    }
    bool confirmed = false;
    for (size_t i = 0; i < cadence_count && !confirmed; i++) {
        confirmed = rational_cmp(cadence_arrivals[i], region_start) >= 0 && rational_cmp(cadence_arrivals[i], region_end) < 0;
    }
    if (confirmed) {
        *out = key_change_extent_is_modulation(length_extent) ? length_extent : KEY_CHANGE_EXTENT_WEAK_MODULATION;
    } else {
        *out = length_extent == KEY_CHANGE_EXTENT_EXTENDED_TONICIZATION ? length_extent : KEY_CHANGE_EXTENT_TONICIZATION;
    }
    return true;
}

