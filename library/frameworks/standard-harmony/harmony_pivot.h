#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_PIVOT_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_PIVOT_H

#include <stdbool.h>
#include <stddef.h>

#include "harmony_chord.h"
#include "harmony_related_key.h"

/*
 * Pivot chords for standard-harmony-v1 (Extended Tonicization and Modulation to Closely
 * Related Keys). A pivot chord is a single chord that is diatonic in both the old key and the new
 * key, so a modulation through it is subtle (ModulationKind, modulation_kind.h). It is
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

/*
 * How good a pivot is, by the function of the chord in each key (harmony_functions.h). Several
 * chords could serve as the pivot; V usually appears soon after it, so the best are predominants
 * in both keys, which suggests we are on the way to V in both. The second best is a tonic in the
 * old key becoming a predominant in the new one (I becomes IV), since a tonic can go directly to
 * V. Poor ones involve the dominant: "V becomes I" says the same chord is both unstable and very
 * stable, which is not how it is heard.
 *
 *   BEST   predominant in both keys
 *   GOOD   tonic in the old key, predominant in the new
 *   FAIR   any other pair that does not involve a dominant
 *   POOR   a dominant in either key
 */
typedef enum {
    HARMONY_PIVOT_BEST = 0,
    HARMONY_PIVOT_GOOD,
    HARMONY_PIVOT_FAIR,
    HARMONY_PIVOT_POOR,
    HARMONY_PIVOT_RANK_COUNT
} HarmonyPivotRank;

/* True for the four ranks above; false for any other value. */
bool harmony_pivot_rank_valid(HarmonyPivotRank rank);

/* "best" "good" "fair" "poor"; NULL for an invalid rank. */
const char *harmony_pivot_rank_name(HarmonyPivotRank rank);

/* The rank of the pivot. False (*out untouched) for a pair that is not a pivot between the two
   keys, an invalid mode, and NULL. */
bool harmony_pivot_rank(HarmonyMode home, const HarmonyRelatedKey *key, const HarmonyPivot *pivot,
                        HarmonyPivotRank *out);

#endif
