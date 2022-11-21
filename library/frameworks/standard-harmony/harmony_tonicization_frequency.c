#include "harmony_tonicization_frequency.h"

#include <stddef.h>

#include "harmony_applied.h"

static const char *const NAMES[HARMONY_TONICIZATION_FREQUENCY_COUNT] = {"common", "frequent", "uncommon"};

bool harmony_tonicization_frequency_valid(HarmonyTonicizationFrequency frequency) {
    return frequency >= 0 && frequency < HARMONY_TONICIZATION_FREQUENCY_COUNT;
}

const char *harmony_tonicization_frequency_name(HarmonyTonicizationFrequency frequency) {
    return harmony_tonicization_frequency_valid(frequency) ? NAMES[frequency] : NULL;
}

HarmonyTonicizationFrequency harmony_tonicization_frequency(HarmonyMode mode, int target) {
    if (!harmony_applied_target_valid(mode, target)) {
        return HARMONY_TONICIZATION_FREQUENCY_COUNT;
    }
    if (target == 5 || target == 4 || (mode == HARMONY_MODE_MAJOR && target == 2)) {
        return HARMONY_TONICIZATION_COMMON;
    }
    if (target == 6 || (mode == HARMONY_MODE_MINOR && target == 3)) {
        return HARMONY_TONICIZATION_FREQUENT;
    }
    return HARMONY_TONICIZATION_UNCOMMON;
}
