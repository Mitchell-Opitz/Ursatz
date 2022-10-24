#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_IV6_PROLONGATION_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_IV6_PROLONGATION_RULES_H

#include <stdbool.h>

#include "harmony_chord.h"
#include "harmony_iv6_prolongation.h"
#include "rules/constraint.h"
#include "rules/rule.h"
#include "semantics/voice_part.h"

/*
 * Writing rules for tonic prolongation with IV6 as a predominant, standard-harmony-v1:
 * I IV6 V(6) I (harmony_iv6_prolongation.h) in four voices. The subject is the four chords
 * and their notes as diatonic steps for each voice part from the soprano to the bass
 * (harmony_steps.h; do is 0, re is 1, an octave is 7). Writing it is tricky, particularly
 * IV6 to V, since there is a danger both of parallels and of a doubled leading tone. The
 * soprano line do do re do helps, and V6 in place of V mitigates the danger of parallels.
 *
 * Hard:
 *
 *   PROGRESSION               the four chords are I, IV6, V or V6, I
 *   BASS_LINE                 the bass is do la sol do (V) or do la ti do (V6)
 *   CHORD_NOTES               each chord has only the notes of its chord, with its root and
 *                             third present and the bass on the root (the third in IV6 and
 *                             V6)
 *   NO_PARALLELS              no two voices make parallel fifths or octaves (a unison
 *                             counts as an octave) from one chord to the next
 *   NO_DOUBLED_LEADING_TONE   ti is in only one voice of the V
 *
 * Soft:
 *
 *   SOPRANO_LINE              the soprano is do do re do
 *   PREFER_V6                 the dominant is V6, not V
 *   AVOID_IN_MINOR            the key is major: in minor the bass makes an augmented second
 *                             from le to ti (the rare uses raise le to la, which is not
 *                             modeled)
 *
 * Every check returns false for an invalid subject.
 */

typedef struct {
    HarmonyMode mode;
    HarmonyChord chords[4];
    int steps[4][VOICE_PART_COUNT];
} HarmonyIv6Prolongation;

typedef enum {
    HARMONY_IV6_PROLONGATION_PROGRESSION = 0,
    HARMONY_IV6_PROLONGATION_BASS_LINE,
    HARMONY_IV6_PROLONGATION_CHORD_NOTES,
    HARMONY_IV6_PROLONGATION_NO_PARALLELS,
    HARMONY_IV6_PROLONGATION_NO_DOUBLED_LEADING_TONE,
    HARMONY_IV6_PROLONGATION_SOPRANO_LINE,
    HARMONY_IV6_PROLONGATION_PREFER_V6,
    HARMONY_IV6_PROLONGATION_AVOID_IN_MINOR,
    HARMONY_IV6_PROLONGATION_RULE_COUNT
} HarmonyIv6ProlongationRule;

/* True for a valid mode and four valid chords. */
bool harmony_iv6_prolongation_valid(const HarmonyIv6Prolongation *prolongation);

const Rule *harmony_iv6_prolongation_rule(HarmonyIv6ProlongationRule which);
const Constraint *harmony_iv6_prolongation_constraint(HarmonyIv6ProlongationRule which);

/* "progression" "bass-line" "chord-notes" "no-parallels" "no-doubled-leading-tone"
   "soprano-line" "prefer-v6" "avoid-in-minor"; NULL for an invalid rule. */
const char *harmony_iv6_prolongation_rule_name(HarmonyIv6ProlongationRule which);

/* True when the prolongation satisfies the rule. False when it does not, and for an
   invalid subject or an invalid rule. */
bool harmony_iv6_prolongation_rule_holds(HarmonyIv6ProlongationRule which,
                                         const HarmonyIv6Prolongation *prolongation);

/* The rules it breaks: hard_violations has bit (1u << rule) set for each hard rule that
   fails, soft_violations for each soft rule. False (both untouched) for an invalid
   subject. */
bool harmony_iv6_prolongation_check(const HarmonyIv6Prolongation *prolongation, unsigned *hard_violations,
                                    unsigned *soft_violations);

#endif
