#ifndef MUSIC_SEMANTICS_SEVENTH_RESOLUTION_H
#define MUSIC_SEMANTICS_SEVENTH_RESOLUTION_H

#include <stdbool.h>

/*
 * SeventhResolution (Value): a way a dominant seventh chord (V7) resolves to
 * tonic in four voices. A plain enum: it names a way and carries no EntityID,
 * no pitches, and no framework claim.
 *
 *   DEFAULT            every active note resolves by its tendency (fa to mi, re
 *                      to do, ti to do), leaving a tonic chord with three roots
 *                      and one third and no fifth
 *   INCOMPLETE         the fifth is omitted from the V7 and its root doubled, so
 *                      the tonic chord that follows is complete
 *   LEADING_TONE_DROP  the leading tone of a complete V7 leaps down to sol in an
 *                      inner voice instead of resolving up, so the tonic chord
 *                      is complete
 *
 * Which voicings make each resolution is a convention of a Framework
 * (frameworks/standard-harmony), not of this type.
 */

typedef enum {
    SEVENTH_RESOLUTION_DEFAULT = 0,
    SEVENTH_RESOLUTION_INCOMPLETE,
    SEVENTH_RESOLUTION_LEADING_TONE_DROP,
    SEVENTH_RESOLUTION_COUNT
} SeventhResolution;

/* True for the three resolutions above; false for any other value. */
bool seventh_resolution_valid(SeventhResolution resolution);

/* "default" "incomplete" "leading-tone-drop"; NULL for an invalid resolution. */
const char *seventh_resolution_name(SeventhResolution resolution);

/* True for the resolutions that make a complete tonic chord: the incomplete V7
   and the leading-tone drop. False for the default and for an invalid
   resolution. */
bool seventh_resolution_gives_complete_tonic(SeventhResolution resolution);

#endif
