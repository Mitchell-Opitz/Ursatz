#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_CADENTIAL_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_CADENTIAL_RULES_H

#include <stdbool.h>

#include "harmony_chord.h"
#include "rules/constraint.h"
#include "rules/rule.h"
#include "semantics/voice_part.h"

/*
 * Cadential 6/4 rules for standard-harmony-v1 (Strengthening Endings with
 * Cadential 6/4). The cadential 6/4 is an embellishment of the dominant that
 * comes from two embellishing tones a sixth and a fourth above the bass sol: a
 * passing tone and a suspension, for example. They always sound as a sixth and a
 * fourth above the bass, and they make the expectation of a cadence stronger,
 * hence the name, though the 6/4 may appear anywhere in a phrase to embellish V
 * or V7. The 6/4 and its resolution to V(7) are one unit, and the chord is
 * labeled by how it functions as an elaboration of V, not as I in second
 * inversion (harmony_chord.h: `cadential`). A tonic triad after a predominant
 * would suggest predominant to tonic, which is not what is heard with sol in the
 * bass.
 *
 * Spelling it in four voices: sol in the bass; choose the sixth above (mi) or the
 * fourth above (do) for the soprano; the other goes in an inner voice; and one
 * voice doubles the bass, a necessity to avoid parallels. So the chord is sol,
 * sol, mi, and do. In minor the sixth above sol is me; degrees ignore that.
 *
 * Resolving it: the figures say to keep each motion in the same voice. The voice
 * with the sixth above the bass (mi) falls to the fifth above (re), and the voice
 * with the fourth above (do) falls to the third above (ti), so 6-5 and 4-3. The
 * bass stays on sol. To make the chord a V7, the voice doubling the bass moves
 * down a step to the seventh of the chord (fa), the figures 8-7; for a V triad it
 * stays on sol.
 *
 * The subject is the notes of the 6/4 chord and of the V or V7 it resolves to,
 * as diatonic steps for each voice part from the soprano to the bass
 * (harmony_steps.h; do is 0, re is 1, ti below do is -1). The V is a V7 when a fa
 * sounds in it.
 *
 * Hard:
 *
 *   BASS_STAYS          the bass is sol in both chords, and stays on the same note
 *   SIX_FOUR_NOTES      the 6/4 is sol, sol, mi, and do: the bass doubled, and the
 *                       sixth and the fourth above it once each
 *   RESOLVES_SIX_FIVE_FOUR_THREE
 *                       the voice on mi falls a step to re, and the voice on do
 *                       falls a step to ti
 *   DOUBLING_VOICE      the voice that doubles the bass stays on sol (V) or falls a
 *                       step to fa (V7)
 *   MINOR_LEADING_TONE  in minor, V has the leading tone (ti); in major this always
 *                       holds
 *
 * Every check returns false for an invalid cadential 6/4.
 */

typedef struct {
    HarmonyMode mode;
    int six_four_steps[VOICE_PART_COUNT];
    int dominant_steps[VOICE_PART_COUNT];
    bool dominant_has_leading_tone;
} HarmonyCadentialSixFour;

typedef enum {
    HARMONY_CADENTIAL_BASS_STAYS = 0,
    HARMONY_CADENTIAL_SIX_FOUR_NOTES,
    HARMONY_CADENTIAL_RESOLVES_SIX_FIVE_FOUR_THREE,
    HARMONY_CADENTIAL_DOUBLING_VOICE,
    HARMONY_CADENTIAL_MINOR_LEADING_TONE,
    HARMONY_CADENTIAL_RULE_COUNT
} HarmonyCadentialRule;

/* True for a valid mode. */
bool harmony_cadential_valid(const HarmonyCadentialSixFour *cadential);

const Rule *harmony_cadential_rule(HarmonyCadentialRule which);
const Constraint *harmony_cadential_constraint(HarmonyCadentialRule which);

/* "bass-stays" "six-four-notes" "resolves-six-five-four-three" "doubling-voice"
   "minor-leading-tone"; NULL for an invalid rule. */
const char *harmony_cadential_rule_name(HarmonyCadentialRule which);

/* True when the cadential 6/4 satisfies the rule. False when it does not, and for
   an invalid cadential 6/4 or an invalid rule. */
bool harmony_cadential_rule_holds(HarmonyCadentialRule which, const HarmonyCadentialSixFour *cadential);

/* The rules it breaks: hard_violations has bit (1u << rule) set for each hard rule
   that fails, soft_violations for each soft rule. False (both untouched) for an
   invalid cadential 6/4. */
bool harmony_cadential_check(const HarmonyCadentialSixFour *cadential, unsigned *hard_violations,
                             unsigned *soft_violations);

/* Whether the V it resolves to is a V7 (a fa sounds in it), when it breaks no hard
   rule. False (*seventh untouched) when a hard rule fails and for an invalid
   cadential 6/4. */
bool harmony_cadential_resolves_to_seventh(const HarmonyCadentialSixFour *cadential, bool *seventh);

#endif
