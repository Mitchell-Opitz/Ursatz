#include "harmony_pivot.h"

#include <stdio.h>

#include "harmony_applied.h"

typedef enum { TRIAD_MAJOR = 0, TRIAD_MINOR, TRIAD_DIMINISHED } Triad;

/* The quality of the triad on each degree of the natural scale. */
static const Triad MAJOR_TRIADS[7] = {TRIAD_MAJOR, TRIAD_MINOR, TRIAD_MINOR, TRIAD_MAJOR,
                                      TRIAD_MAJOR, TRIAD_MINOR, TRIAD_DIMINISHED};
static const Triad MINOR_TRIADS[7] = {TRIAD_MINOR, TRIAD_DIMINISHED, TRIAD_MAJOR, TRIAD_MINOR,
                                      TRIAD_MINOR, TRIAD_MAJOR,      TRIAD_MAJOR};

static Triad quality(HarmonyMode mode, int degree) {
    return mode == HARMONY_MODE_MAJOR ? MAJOR_TRIADS[degree - 1] : MINOR_TRIADS[degree - 1];
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

bool harmony_pivot_label(HarmonyMode home, const HarmonyRelatedKey *key, const HarmonyPivot *pivot, char *out,
                         size_t size) {
    if (out == NULL || !harmony_pivot_valid(home, key, pivot)) {
        return false;
    }
    char label[32];
    int length = snprintf(label, sizeof(label), "%s becomes %s", harmony_triad_numeral(home, pivot->old_degree),
                          harmony_triad_numeral(key->mode, pivot->new_degree));
    if (length < 0 || (size_t)length >= size) {
        return false;
    }
    snprintf(out, size, "%s", label);
    return true;
}
