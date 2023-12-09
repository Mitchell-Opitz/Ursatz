#ifndef MUSIC_SEMANTICS_MODULATION_KIND_H
#define MUSIC_SEMANTICS_MODULATION_KIND_H

#include <stdbool.h>

/*
 * ModulationKind (Value): how a composer introduces a new key in a modulation, a longer-term
 * change of tonic (sometimes called a key change). A plain enum: it names a kind and carries no
 * EntityID, no key, and no framework claim.
 *
 *   DIRECT   an abrupt modulation: the composer goes directly to the new key, often at a phrase
 *            boundary (also called a phrase modulation). In analysis the new key is simply
 *            labeled and the harmonic analysis continues in it
 *   PIVOT    a subtle modulation through a pivot chord, a single chord that is diatonic in both
 *            the old key and the new key, labeled with both Roman numerals
 *
 * What counts as a pivot chord, and which keys are closely related, are conventions of a
 * Framework (frameworks/standard-harmony), not of this type.
 */

typedef enum {
    MODULATION_KIND_DIRECT = 0,
    MODULATION_KIND_PIVOT,
    MODULATION_KIND_COUNT
} ModulationKind;

/* True for the two kinds above; false for any other value. */
bool modulation_kind_valid(ModulationKind kind);

/* "direct" "pivot"; NULL for an invalid kind. */
const char *modulation_kind_name(ModulationKind kind);

#endif
