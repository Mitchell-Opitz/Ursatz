#ifndef MUSIC_SEMANTICS_ALTERNATIVE_PATH_KIND_H
#define MUSIC_SEMANTICS_ALTERNATIVE_PATH_KIND_H

#include <stdbool.h>

/*
 * AlternativePathKind (Value): how an alternative path leaves the expected course of a phrase. A plain enum: it names a value and carries no EntityID, no
 * container, and no framework claim.
 *
 *   DETOUR                a temporary deviation that returns to previous material before the cadence
 *   REROUTE               a permanent deviation that reaches the cadence without returning
 *
 * What counts as returning is a convention of a Framework (frameworks/standard-form), not of this
 * type.
 */

typedef enum {
    ALTERNATIVE_PATH_KIND_DETOUR = 0,
    ALTERNATIVE_PATH_KIND_REROUTE,
    ALTERNATIVE_PATH_KIND_COUNT
} AlternativePathKind;

/* True for the 2 values above; false for any other value. */
bool alternative_path_kind_valid(AlternativePathKind value);

/* "detour" "reroute"; NULL for an invalid
   value. */
const char *alternative_path_kind_name(AlternativePathKind value);

#endif
