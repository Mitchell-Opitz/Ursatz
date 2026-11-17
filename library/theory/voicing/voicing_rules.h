#ifndef URSATZ_FRAMEWORKS_STANDARD_VOICING_VOICING_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_VOICING_VOICING_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "model/pitch/pitch_spelling.h"
#include "theory/vocabulary/voice_part.h"
#include "theory/voicing/stem_rules.h"
#include "theory/voicing/voicing_chord.h"

/*
 * The six rules a chord in strict SATB style follows, as standard-voicing-v1
 * names them. Each is hard; the checks themselves are in the header named
 * below.
 *
 *   STEM_DIRECTION       stem_rules.h: soprano and alto on the treble
 *                        staff, tenor and bass on the bass staff; soprano
 *                        and tenor up-stems, alto and bass down-stems
 *   CHORD_CONSTRUCTION   construction_rules.h: the bass matches the
 *                        inversion, no chord tone is missing, no note
 *                        outside the chord
 *   RANGE                range_rules.h: each voice stays in its range
 *   SPACING              spacing_rules.h: no more than an octave between
 *                        adjacent upper voices, a twelfth tenor to bass
 *   VOICE_CROSSING       crossing_rules.h: soprano above alto above tenor
 *                        above bass
 *   DOUBLING             doubling_rules.h: the leading tone and a chordal
 *                        seventh are never doubled; a triad usually
 *                        doubles its bass; a seventh chord doubles nothing
 */

typedef enum {
    STANDARD_VOICING_RULE_STEM_DIRECTION = 0,
    STANDARD_VOICING_RULE_CHORD_CONSTRUCTION,
    STANDARD_VOICING_RULE_RANGE,
    STANDARD_VOICING_RULE_SPACING,
    STANDARD_VOICING_RULE_VOICE_CROSSING,
    STANDARD_VOICING_RULE_DOUBLING,
    STANDARD_VOICING_RULE_COUNT
} StandardVoicingRule;

/*
 * Checking a whole voicing against the six rules. The input is the
 * voiced chord plus what the caller knows about the chord it realizes:
 *
 *   chord           the four voices (every one non-NULL)
 *   tones           the chord's tones in root position order
 *   tone_count      3 (triad) or 4 (seventh chord)
 *   inversion       0 root position, up to tone_count - 1
 *   leading_tone    the key's leading tone, or NULL when none is in play
 *   stem_directions the written stem direction of each voice, indexed by
 *                   VoicePart, or NULL when the chord has not been
 *                   engraved (the stem rule is then not checked)
 */

typedef struct {
    const StandardVoicingChord *chord;
    const PitchSpelling *const *tones;
    size_t tone_count;
    int inversion;
    const PitchSpelling *leading_tone;
    const StandardVoicingStemDirection *stem_directions;
} StandardVoicingInput;

/* The rules the voicing breaks, as a bit mask: bit (1u << rule) is set for
   each StandardVoicingRule that fails. Zero when the voicing follows all
   six. The soft preference that a triad double its bass is not a rule
   and never sets a bit. False (*violations untouched) for invalid input:
   NULL, a voice that is NULL, a tone count that is not 3 or 4, an
   inversion out of range. */
bool standard_voicing_check(const StandardVoicingInput *input, unsigned *violations);

#endif
