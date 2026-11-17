#ifndef MUSIC_SEMANTICS_APPLIED_CHORD_KIND_H
#define MUSIC_SEMANTICS_APPLIED_CHORD_KIND_H

#include <stdbool.h>

/*
 * AppliedChordKind (Value): the kind of an applied chord, a chromatic chord that tonicizes a
 * non-tonic chord, making it temporarily sound like tonic. A plain enum: it names a kind and
 * carries no EntityID, no chord, and no framework claim. In analysis the label of an applied
 * chord is the kind followed by the chord being tonicized, written with a slash: V/V is read
 * "five of five".
 *
 *   SECONDARY_DOMINANT       an applied V or V7, a dominant chord borrowed from the temporary
 *                            key; a major triad or a dominant seventh chord
 *   SECONDARY_LEADING_TONE   an applied vii diminished, vii diminished 7, or (rarely) vii
 *                            half diminished 7, a leading-tone chord borrowed from the
 *                            temporary key
 *
 * Which chords can be tonicized, and how an applied chord is spelled and resolved, are
 * conventions of a Framework (frameworks/standard-harmony), not of this type.
 */

typedef enum {
    APPLIED_CHORD_KIND_SECONDARY_DOMINANT = 0,
    APPLIED_CHORD_KIND_SECONDARY_LEADING_TONE,
    APPLIED_CHORD_KIND_COUNT
} AppliedChordKind;

/* True for the two kinds above; false for any other value. */
bool applied_chord_kind_valid(AppliedChordKind kind);

/* "secondary-dominant" "secondary-leading-tone"; NULL for an invalid kind. */
const char *applied_chord_kind_name(AppliedChordKind kind);

#endif
