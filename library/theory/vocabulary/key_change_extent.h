#ifndef MUSIC_SEMANTICS_KEY_CHANGE_EXTENT_H
#define MUSIC_SEMANTICS_KEY_CHANGE_EXTENT_H

#include <stdbool.h>

/*
 * KeyChangeExtent (Value): how far a passage goes toward a new key. Tonicization and
 * modulation are two poles of a spectrum based on the length of the passage and the strength
 * with which a new key is established: modulations are longer and stronger than tonicizations.
 * A plain enum: it names an extent and carries no EntityID, no key, and no framework claim.
 *
 *   TONICIZATION            a non-tonic chord made to sound like a temporary tonic, a short
 *                           passage with no cadence in the new key
 *   EXTENDED_TONICIZATION   the gray area: longer than a tonicization (more than two chords)
 *                           but not so long that a cadence establishes the new key; analyzed
 *                           temporarily in the new key, with its Roman numeral under a bracket
 *   WEAK_MODULATION         the new key is established by a cadence, but the passage goes
 *                           right back to the home key
 *   MODULATION              the new key is established by a cadence and the passage continues
 *                           in it afterward, the clearest modulation
 *
 * The thresholds, and what establishes a key, are conventions of a Framework
 * (frameworks/standard-harmony), not of this type. A region is confirmed as a modulation by a
 * cadence in the new key (harmony_key_change_confirm); without one it is a tonicization. People hear the same passage differently,
 * so an analysis represents a hearing, not a fact.
 */

typedef enum {
    KEY_CHANGE_EXTENT_TONICIZATION = 0,
    KEY_CHANGE_EXTENT_EXTENDED_TONICIZATION,
    KEY_CHANGE_EXTENT_WEAK_MODULATION,
    KEY_CHANGE_EXTENT_MODULATION,
    KEY_CHANGE_EXTENT_COUNT
} KeyChangeExtent;

/* True for the four extents above; false for any other value. */
bool key_change_extent_valid(KeyChangeExtent extent);

/* "tonicization" "extended-tonicization" "weak-modulation" "modulation"; NULL for an invalid
   extent. */
const char *key_change_extent_name(KeyChangeExtent extent);

/* True for a weak modulation and a modulation, which establish a new key with a cadence. False
   for the tonicizations and for an invalid extent. */
bool key_change_extent_is_modulation(KeyChangeExtent extent);

#endif
