#ifndef MUSIC_SEMANTICS_VOICE_PART_H
#define MUSIC_SEMANTICS_VOICE_PART_H

#include <stdbool.h>

/*
 * VoicePart (Value): one of the four voice parts of SATB writing --
 * soprano, alto, tenor, bass -- ordered from the highest part to the
 * lowest. A plain enum: it names a role in a four-part texture and
 * carries no EntityID, no range, no clef, and no framework claim.
 *
 * VoicePart is NOT Voice (containers/voice.h). Voice identifies a single
 * line and is deliberately id-only; a Voice MAY be written as any part,
 * or none. Ranges, staff placement, and stem direction are conventions
 * of a Framework (frameworks/standard-voicing), not of this type.
 */

typedef enum {
    VOICE_PART_SOPRANO = 0,
    VOICE_PART_ALTO,
    VOICE_PART_TENOR,
    VOICE_PART_BASS,
    VOICE_PART_COUNT
} VoicePart;

/* True for the four parts above; false for any other value. */
bool voice_part_valid(VoicePart part);

/* "soprano" "alto" "tenor" "bass"; NULL for an invalid part. */
const char *voice_part_name(VoicePart part);

/* True when `a` is a higher part than `b` in the soprano-to-bass order
   (soprano above alto above tenor above bass). False when either is
   invalid, or for equal parts. */
bool voice_part_is_above(VoicePart a, VoicePart b);

#endif
