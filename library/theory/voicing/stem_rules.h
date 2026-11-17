#ifndef URSATZ_FRAMEWORKS_STANDARD_VOICING_STEM_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_VOICING_STEM_RULES_H

#include <stdbool.h>

#include "theory/vocabulary/voice_part.h"
#include "theory/voicing/voicing_chord.h"

/*
 * Stem direction rules (STANDARD_VOICING_RULE_STEM_DIRECTION). On a grand
 * staff in SATB style the soprano and alto are written on the treble
 * staff (upper), the tenor and bass on the bass staff (lower). The
 * soprano and tenor take up-stems; the alto and bass take down-stems.
 * A crossed stem direction is an error.
 *
 * This is a rule about voices, not about position (notes above the middle
 * line down, below up): in SATB writing the voice decides the direction,
 * whatever the note's position on its staff.
 */

typedef enum { STANDARD_VOICING_STEM_UP = 0, STANDARD_VOICING_STEM_DOWN } StandardVoicingStemDirection;

/* The stem direction the part takes. False (output untouched) for an
   invalid part or a NULL output. */
bool standard_voicing_stem_direction_for_part(VoicePart part, StandardVoicingStemDirection *out);

/* True when `direction` is the one the part takes. False for an invalid
   part or direction. */
bool standard_voicing_stem_direction_valid(VoicePart part, StandardVoicingStemDirection direction);

/* True when every voice's direction is the one its part takes. */
bool standard_voicing_stems_valid(const StandardVoicingStemDirection directions[VOICE_PART_COUNT]);

#endif
