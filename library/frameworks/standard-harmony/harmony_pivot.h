#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_PIVOT_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_PIVOT_H

#include <stdbool.h>
#include <stddef.h>

#include "harmony_chord.h"
#include "harmony_related_key.h"

/*
 * Pivot chords for standard-harmony-v1 (Extended Tonicization and Modulation to Closely
 * Related Keys). A pivot chord is a single chord that is diatonic in both the old key and the new
 * key, so a modulation through it is subtle (ModulationKind, semantics/modulation_kind.h). It is
 * labeled with two Roman numerals, the old key's above and the new key's below: "iv becomes ii".
 *
 * To find the potential pivots between two keys, list the triads of the old key, list the triads
 * of the new key, and match the ones that appear in both: the same root and the same quality
 * (major, minor, or diminished). Triads are those of the natural scale (the natural minor in
 * minor, so the harmonic minor's V and vii diminished are not counted); a seventh is carried by
 * the chord and does not make a different pivot ("iv7 becomes ii7" is the pivot iv to ii). The
 * new key is one of the home key's closely related keys (harmony_related_key.h), given from the
 * old key, which is the home key.
 */

typedef struct {
    int old_degree;
    int new_degree;
} HarmonyPivot;

/* At most seven triads are shared. */
#define HARMONY_PIVOT_MAX 7

/* The triads diatonic in both keys, as pairs of scale degrees (the degree in the old key and in
   the new one), in order of the old key's degree, written to out. Returns how many (1 to 7 for
   closely related keys), or 0 when the key is not closely related to the home key, for an
   invalid mode, and for NULL. */
int harmony_pivot_find(HarmonyMode home, const HarmonyRelatedKey *key, HarmonyPivot out[HARMONY_PIVOT_MAX]);

/* True when the pair is one of the triads diatonic in both keys. */
bool harmony_pivot_valid(HarmonyMode home, const HarmonyRelatedKey *key, const HarmonyPivot *pivot);

/* The label, for example "iv becomes ii" (the old key's numeral, then the new key's), with a
   terminating zero. False (*out untouched) for a pair that is not a pivot, a NULL pointer, and a
   buffer too small. */
bool harmony_pivot_label(HarmonyMode home, const HarmonyRelatedKey *key, const HarmonyPivot *pivot, char *out,
                         size_t size);

#endif
