#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_PREDOMINANT_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_PREDOMINANT_RULES_H

#include <stdbool.h>

#include "harmony_chord.h"
#include "rules/constraint.h"
#include "rules/rule.h"
#include "semantics/voice_part.h"

/*
 * Strong predominant part-writing rules for standard-harmony-v1: the end of a
 * phrase written in four voices with a strong predominant (IV, ii6, or ii,
 * harmony_strong_predominants.h) before V and, in an authentic cadence, I. For a
 * half cadence, leave off the I chord (has_tonic false). V and I are triads in
 * root position, so the bass is their root.
 *
 * The subject is the predominant chord and the notes of the three chords as
 * diatonic steps for each voice part from the soprano to the bass
 * (harmony_steps.h), so that the direction of each voice's motion is known.
 *
 * The bass is fa to sol (to do): fa is the note harmonized by IV and ii6. Writing
 * the soprano starts from V: put an active note (ti or re) in the soprano over V,
 * resolve it over I, and approach it from above, to make contrary motion with
 * the bass (which moves up from fa to sol), or by common tone.
 *
 * The strong predominant area is where parallel octaves and fifths tend to show
 * up, most of all with IV, whose root moves by step to V. They are the most
 * common part-writing error. Moving as many upper voices as possible in contrary
 * motion (down) avoids them, and all voices moving the same way is a sign of
 * trouble.
 *
 * Hard:
 *
 *   STRONG_PREDOMINANT   the first chord is IV, ii6, or ii
 *   BASS_MOTION          the bass is fa for IV and ii6, or re for ii, then sol, then
 *                        do (when there is a tonic)
 *   CHORD_NOTES          each chord has only the notes of its triad (IV: fa la do;
 *                        ii: re fa la; V: sol ti re; I: do mi sol), with its root
 *                        and third present: the root names the chord and the third
 *                        gives its quality
 *   ACTIVE_NOTE_RESOLVES with a tonic, ti goes up a step to do and re down a step
 *                        to do
 *   MINOR_LEADING_TONE   in minor, V has the leading tone (ti), making it a major
 *                        triad; in major this always holds
 *   NO_PARALLELS         no two voices make parallel fifths or octaves (a unison
 *                        counts as an octave) from the predominant to V, or from V
 *                        to I
 *
 * Soft:
 *
 *   NOT_ALL_VOICES_SAME_DIRECTION
 *                        the voices do not all move in the same direction into V
 *   SOPRANO_APPROACH     the soprano reaches V from above or by common tone, not
 *                        from below
 *   IV_DOUBLES_BASS      a root position IV has its bass doubled almost always
 *                        (ii6, a first inversion chord, doubles any note)
 *   II_SOPRANO           over root position ii the soprano is re or fa, not la
 *   ROOT_II_MAJOR_ONLY   root position ii is for a major key
 *
 * Every check returns false for an invalid ending.
 */

typedef struct {
    HarmonyMode mode;
    HarmonyChord predominant;
    int predominant_steps[VOICE_PART_COUNT];
    int dominant_steps[VOICE_PART_COUNT];
    int tonic_steps[VOICE_PART_COUNT]; /* ignored without a tonic */
    bool has_tonic;
    bool dominant_has_leading_tone;
} HarmonyPredominantEnding;

typedef enum {
    HARMONY_PREDOMINANT_STRONG_PREDOMINANT = 0,
    HARMONY_PREDOMINANT_BASS_MOTION,
    HARMONY_PREDOMINANT_CHORD_NOTES,
    HARMONY_PREDOMINANT_ACTIVE_NOTE_RESOLVES,
    HARMONY_PREDOMINANT_MINOR_LEADING_TONE,
    HARMONY_PREDOMINANT_NO_PARALLELS,
    HARMONY_PREDOMINANT_NOT_ALL_VOICES_SAME_DIRECTION,
    HARMONY_PREDOMINANT_SOPRANO_APPROACH,
    HARMONY_PREDOMINANT_IV_DOUBLES_BASS,
    HARMONY_PREDOMINANT_II_SOPRANO,
    HARMONY_PREDOMINANT_ROOT_II_MAJOR_ONLY,
    HARMONY_PREDOMINANT_RULE_COUNT
} HarmonyPredominantRule;

/* True for a valid mode and predominant chord. */
bool harmony_predominant_ending_valid(const HarmonyPredominantEnding *ending);

const Rule *harmony_predominant_rule(HarmonyPredominantRule which);
const Constraint *harmony_predominant_constraint(HarmonyPredominantRule which);

/* "strong-predominant" "bass-motion" "chord-notes" "active-note-resolves"
   "minor-leading-tone" "no-parallels" "not-all-voices-same-direction"
   "soprano-approach" "iv-doubles-bass" "ii-soprano" "root-ii-major-only"; NULL for
   an invalid rule. */
const char *harmony_predominant_rule_name(HarmonyPredominantRule which);

/* True when the ending satisfies the rule. False when it does not, and for an
   invalid ending or an invalid rule. */
bool harmony_predominant_rule_holds(HarmonyPredominantRule which, const HarmonyPredominantEnding *ending);

/* The rules the ending breaks: hard_violations has bit (1u << rule) set for each
   hard rule that fails, soft_violations for each soft rule. False (both
   untouched) for an invalid ending. */
bool harmony_predominant_check(const HarmonyPredominantEnding *ending, unsigned *hard_violations,
                               unsigned *soft_violations);

#endif
