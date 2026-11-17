#ifndef MUSIC_SEMANTICS_EXTERNAL_EXPANSION_KIND_H
#define MUSIC_SEMANTICS_EXTERNAL_EXPANSION_KIND_H

#include <stdbool.h>

/*
 * ExternalExpansionKind (Value): a kind of expansion outside the bounds of a phrase. A plain enum: it names a value and carries no EntityID, no
 * container, and no framework claim.
 *
 *   PREFIX                added before the beginning of the phrase, such as an introduction
 *   POST_CADENTIAL_EXTENSIONa short suffix after the cadence, within a section
 *   CODETTA               a medium suffix after the end of a section
 *   CODA                  a long suffix after the end of a piece or movement
 *
 * The terms are used flexibly, and which ones are suffixes is a convention of a Framework
 * (frameworks/standard-form), not of this type.
 */

typedef enum {
    EXTERNAL_EXPANSION_KIND_PREFIX = 0,
    EXTERNAL_EXPANSION_KIND_POST_CADENTIAL_EXTENSION,
    EXTERNAL_EXPANSION_KIND_CODETTA,
    EXTERNAL_EXPANSION_KIND_CODA,
    EXTERNAL_EXPANSION_KIND_COUNT
} ExternalExpansionKind;

/* True for the 4 values above; false for any other value. */
bool external_expansion_kind_valid(ExternalExpansionKind value);

/* "prefix" "post-cadential-extension" "codetta" "coda"; NULL for an invalid
   value. */
const char *external_expansion_kind_name(ExternalExpansionKind value);

#endif
