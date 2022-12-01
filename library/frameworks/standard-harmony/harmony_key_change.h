#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_KEY_CHANGE_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_KEY_CHANGE_H

#include <stdbool.h>
#include <stddef.h>

#include "harmony_chord.h"
#include "harmony_pivot.h"
#include "harmony_related_key.h"
#include "semantics/key_change_extent.h"
#include "semantics/modulation_kind.h"

/*
 * Tonicization versus modulation, for standard-harmony-v1 (Extended Tonicization and
 * Modulation to Closely Related Keys). The two are poles of a spectrum based on the length of
 * the passage and the strength with which a new key is established (KeyChangeExtent,
 * semantics/key_change_extent.h). The facts are the analyst's, since this module does not
 * listen: how many chords of the passage are in the new key, whether a cadence establishes the
 * new key, and whether the passage continues in the new key after that cadence. Cadences
 * establish keys, so:
 *
 *   no cadence in the new key, two chords or fewer   TONICIZATION (V to x)
 *   no cadence in the new key, more than two chords  EXTENDED_TONICIZATION
 *   a cadence in the new key, going on in it         MODULATION
 *   a cadence in the new key, then back home         WEAK_MODULATION
 *
 * An extended tonicization is analyzed temporarily in the new key, with the Roman numeral of
 * the new key's tonic under a bracket under the whole passage: F minor in E-flat major is ii.
 * Sometimes it also helps transition to another key, serving as a pivot area. The signs are
 * x/ii, y/ii, z/ii over and over in a short space, or Roman numerals that stop making
 * functional sense in the home key while the upcoming cadence is still in the home key; spotting
 * those, like finding cadences and repeated accidentals, is the analyst's.
 *
 * A modulation is a pivot modulation when a pivot chord (harmony_pivot.h) is given, and a
 * direct one when it is not.
 */

typedef struct {
    int chords_in_new_key;
    bool cadence_in_new_key;
    bool continues_after_cadence;
} HarmonyKeyChange;

/* The extent of the key change. False (*out untouched) for fewer than one chord, for a
   continuation without a cadence, and for NULL. */
bool harmony_key_change_classify(const HarmonyKeyChange *change, KeyChangeExtent *out);

/* PIVOT when a pivot is given, DIRECT when it is NULL. */
ModulationKind harmony_modulation_kind(const HarmonyPivot *pivot);

/* The label of an extended tonicization: the Roman numeral of the new key's tonic in the home
   key ("ii" for the key of ii, "v" for the minor key of v), with a terminating zero. False (*out
   untouched) when the key is not closely related to the home key, a NULL pointer, and a buffer
   too small. */
bool harmony_extended_tonicization_label(HarmonyMode home, const HarmonyRelatedKey *key, char *out, size_t size);

#endif
