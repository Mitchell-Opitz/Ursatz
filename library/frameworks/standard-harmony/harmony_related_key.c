#include "harmony_related_key.h"

#include <stddef.h>

#include "harmony_applied.h"

static const char *const MAJOR_NUMERALS[7] = {"I", "ii", "iii", "IV", "V", "vi", "viio"};
static const char *const MINOR_NUMERALS[7] = {"i", "iio", "III", "iv", "v", "VI", "VII"};

/* The related keys in scale order, with the mode of each key. */
static const HarmonyRelatedKey MAJOR_KEYS[5] = {{2, HARMONY_MODE_MINOR}, {3, HARMONY_MODE_MINOR},
                                                {4, HARMONY_MODE_MAJOR}, {5, HARMONY_MODE_MAJOR},
                                                {6, HARMONY_MODE_MINOR}};
static const HarmonyRelatedKey MINOR_KEYS[5] = {{3, HARMONY_MODE_MAJOR}, {4, HARMONY_MODE_MINOR},
                                                {5, HARMONY_MODE_MINOR}, {6, HARMONY_MODE_MAJOR},
                                                {7, HARMONY_MODE_MAJOR}};

static const char *const RANK_NAMES[HARMONY_KEY_RELATION_COUNT] = {"common", "other"};

const char *harmony_triad_numeral(HarmonyMode mode, int degree) {
    if (!harmony_mode_valid(mode) || degree < 1 || degree > 7) {
        return NULL;
    }
    return mode == HARMONY_MODE_MAJOR ? MAJOR_NUMERALS[degree - 1] : MINOR_NUMERALS[degree - 1];
}

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

bool harmony_key_relation_rank_valid(HarmonyKeyRelationRank rank) {
    return rank >= 0 && rank < HARMONY_KEY_RELATION_COUNT;
}

const char *harmony_key_relation_rank_name(HarmonyKeyRelationRank rank) {
    return harmony_key_relation_rank_valid(rank) ? RANK_NAMES[rank] : NULL;
}

HarmonyKeyRelationRank harmony_key_relation_rank(HarmonyMode home, const HarmonyRelatedKey *key) {
    if (!harmony_key_is_closely_related(home, key)) {
        return HARMONY_KEY_RELATION_COUNT;
    }
    bool common = home == HARMONY_MODE_MAJOR ? (key->degree == 5 || key->degree == 6)
                                             : (key->degree == 3 || key->degree == 5);
    return common ? HARMONY_KEY_RELATION_COMMON : HARMONY_KEY_RELATION_OTHER;
}
