#ifndef MUSIC_SEMANTICS_EXPANSION_PLACEMENT_H
#define MUSIC_SEMANTICS_EXPANSION_PLACEMENT_H

#include <stdbool.h>

/*
 * ExpansionPlacement (Value): where an expansion adds its length. A plain enum: it names a value and carries no EntityID, no
 * container, and no framework claim.
 *
 *   INTERNAL              between the beginning and the cadence of the phrase
 *   EXTERNAL              before the beginning or after the cadence of the phrase
 *
 * Which techniques belong to each placement is a convention of a Framework
 * (frameworks/standard-form), not of this type.
 */

typedef enum {
    EXPANSION_PLACEMENT_INTERNAL = 0,
    EXPANSION_PLACEMENT_EXTERNAL,
    EXPANSION_PLACEMENT_COUNT
} ExpansionPlacement;

/* True for the 2 values above; false for any other value. */
bool expansion_placement_valid(ExpansionPlacement value);

/* "internal" "external"; NULL for an invalid
   value. */
const char *expansion_placement_name(ExpansionPlacement value);

#endif
