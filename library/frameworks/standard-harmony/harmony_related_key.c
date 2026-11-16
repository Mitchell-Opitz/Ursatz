#include "harmony_related_key.h"

#include <stddef.h>

#include "harmony_applied.h"

/* The related keys in scale order, with the mode of each key. */
static const HarmonyRelatedKey MAJOR_KEYS[5] = {{2, HARMONY_MODE_MINOR}, {3, HARMONY_MODE_MINOR},
                                                {4, HARMONY_MODE_MAJOR}, {5, HARMONY_MODE_MAJOR},
                                                {6, HARMONY_MODE_MINOR}};
static const HarmonyRelatedKey MINOR_KEYS[5] = {{3, HARMONY_MODE_MAJOR}, {4, HARMONY_MODE_MINOR},
                                                {5, HARMONY_MODE_MINOR}, {6, HARMONY_MODE_MAJOR},
                                                {7, HARMONY_MODE_MAJOR}};

int harmony_related_key_count(HarmonyMode home) {
    return harmony_mode_valid(home) ? 5 : 0;
}

bool harmony_related_key_at(HarmonyMode home, int index, HarmonyRelatedKey *out) {
    if (out == NULL || !harmony_mode_valid(home) || index < 0 || index >= 5) {
        return false;
    }
    *out = home == HARMONY_MODE_MAJOR ? MAJOR_KEYS[index] : MINOR_KEYS[index];
    return true;
}

bool harmony_related_key_of(HarmonyMode home, int degree, HarmonyRelatedKey *out) {
    for (int i = 0; i < harmony_related_key_count(home); i++) {
        HarmonyRelatedKey key;
        if (harmony_related_key_at(home, i, &key) && key.degree == degree) {
            if (out == NULL) {
                return false;
            }
            *out = key;
            return true;
        }
    }
    return false;
}

bool harmony_key_is_closely_related(HarmonyMode home, const HarmonyRelatedKey *key) {
    HarmonyRelatedKey found;
    return key != NULL && harmony_related_key_of(home, key->degree, &found) && found.mode == key->mode;
}

int harmony_related_key_semitone(HarmonyMode home, const HarmonyRelatedKey *key) {
    return harmony_key_is_closely_related(home, key) ? harmony_applied_scale_semitone(home, key->degree) : -1;
}

