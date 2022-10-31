#include "harmony_predominant_seventh.h"

#include <stddef.h>

static const char *const KIND_NAMES[HARMONY_PREDOMINANT_SEVENTH_COUNT] = {"ii7", "ii6/5", "ii4/3", "ii4/2",
                                                                          "iv7", "vi7",   "iii7"};
static const char *const RANK_NAMES[HARMONY_PREDOMINANT_SEVENTH_RANK_COUNT] = {"most", "somewhat", "less", "least"};

static const HarmonyPredominantSeventhRank RANKS[HARMONY_PREDOMINANT_SEVENTH_COUNT] = {
    HARMONY_PREDOMINANT_SEVENTH_RANK_SOMEWHAT, HARMONY_PREDOMINANT_SEVENTH_RANK_MOST,
    HARMONY_PREDOMINANT_SEVENTH_RANK_LESS,     HARMONY_PREDOMINANT_SEVENTH_RANK_LEAST,
    HARMONY_PREDOMINANT_SEVENTH_RANK_LESS,     HARMONY_PREDOMINANT_SEVENTH_RANK_LESS,
    HARMONY_PREDOMINANT_SEVENTH_RANK_LEAST};

bool harmony_predominant_seventh_kind_valid(HarmonyPredominantSeventhKind kind) {
    return kind >= 0 && kind < HARMONY_PREDOMINANT_SEVENTH_COUNT;
}

const char *harmony_predominant_seventh_kind_name(HarmonyPredominantSeventhKind kind) {
    return harmony_predominant_seventh_kind_valid(kind) ? KIND_NAMES[kind] : NULL;
}

bool harmony_predominant_seventh_rank_valid(HarmonyPredominantSeventhRank rank) {
    return rank >= 0 && rank < HARMONY_PREDOMINANT_SEVENTH_RANK_COUNT;
}

const char *harmony_predominant_seventh_rank_name(HarmonyPredominantSeventhRank rank) {
    return harmony_predominant_seventh_rank_valid(rank) ? RANK_NAMES[rank] : NULL;
}

HarmonyPredominantSeventhRank harmony_predominant_seventh_rank(HarmonyPredominantSeventhKind kind) {
    return harmony_predominant_seventh_kind_valid(kind) ? RANKS[kind] : HARMONY_PREDOMINANT_SEVENTH_RANK_COUNT;
}

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
