#include "theory/harmony/harmony_pivot.h"

#include <stdio.h>

#include "theory/harmony/harmony_applied.h"
#include "theory/harmony/harmony_functions.h"
#include "theory/tonality/triad_in_key_rules.h"

/* The quality of the triad on a degree of the natural scale of the mode. */
static StandardTonalityTriadQuality quality(HarmonyMode mode, int degree) {
    StandardTonalityTriadQuality triad = STANDARD_TONALITY_TRIAD_MAJOR;
    standard_tonality_triad_quality_in_key(mode == HARMONY_MODE_MAJOR ? STANDARD_TONALITY_TRIAD_KEY_MAJOR
                                                                      : STANDARD_TONALITY_TRIAD_KEY_MINOR_NATURAL,
                                           degree, &triad);
    return triad;
}

/* The triad on the new key's degree, as its root in semitones above the old tonic. */
static int new_root(HarmonyMode home, const HarmonyRelatedKey *key, int degree) {
    return (harmony_related_key_semitone(home, key) + harmony_applied_scale_semitone(key->mode, degree)) % 12;
}

int harmony_pivot_find(HarmonyMode home, const HarmonyRelatedKey *key, HarmonyPivot out[HARMONY_PIVOT_MAX]) {
    if (out == NULL || !harmony_key_is_closely_related(home, key)) {
        return 0;
    }
    int count = 0;
    for (int old = 1; old <= 7; old++) {
        for (int next = 1; next <= 7; next++) {
            if (harmony_applied_scale_semitone(home, old) == new_root(home, key, next) &&
                quality(home, old) == quality(key->mode, next)) {
                out[count].old_degree = old;
                out[count].new_degree = next;
                count++;
            }
        }
    }
    return count;
}

bool harmony_pivot_valid(HarmonyMode home, const HarmonyRelatedKey *key, const HarmonyPivot *pivot) {
    HarmonyPivot found[HARMONY_PIVOT_MAX];
    if (pivot == NULL) {
        return false;
    }
    int count = harmony_pivot_find(home, key, found);
    for (int i = 0; i < count; i++) {
        if (found[i].old_degree == pivot->old_degree && found[i].new_degree == pivot->new_degree) {
            return true;
        }
    }
    return false;
}

static bool function_of(HarmonyMode mode, int degree, HarmonicFunction *out) {
    HarmonyChord chord = {degree, 0, false, false, false, false};
    return harmony_function_of(mode, &chord, out);
}

bool harmony_pivot_rank(HarmonyMode home, const HarmonyRelatedKey *key, const HarmonyPivot *pivot,
                        HarmonyPivotRank *out) {
    HarmonicFunction old_function, new_function;
    if (out == NULL || !harmony_pivot_valid(home, key, pivot) || !function_of(home, pivot->old_degree, &old_function) ||
        !function_of(key->mode, pivot->new_degree, &new_function)) {
        return false;
    }
    if (old_function == HARMONIC_FUNCTION_DOMINANT || new_function == HARMONIC_FUNCTION_DOMINANT) {
        *out = HARMONY_PIVOT_POOR;
    } else if (old_function == HARMONIC_FUNCTION_PREDOMINANT && new_function == HARMONIC_FUNCTION_PREDOMINANT) {
        *out = HARMONY_PIVOT_BEST;
    } else if (old_function == HARMONIC_FUNCTION_TONIC && new_function == HARMONIC_FUNCTION_PREDOMINANT) {
        *out = HARMONY_PIVOT_GOOD;
    } else {
        *out = HARMONY_PIVOT_FAIR;
    }
    return true;
}
