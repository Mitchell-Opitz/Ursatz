#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_PREDOMINANT_SEVENTH_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_PREDOMINANT_SEVENTH_RULES_H

#include <stdbool.h>

#include "harmony_chord.h"
#include "harmony_predominant_seventh.h"
#include "rules/constraint.h"
#include "rules/rule.h"
#include "semantics/voice_part.h"

/*
 * Treatment rules for predominant seventh chords, standard-harmony-v1
 * (harmony_predominant_seventh.h). The subject is three chords in four voices: the chord
 * before, the predominant seventh chord, and the chord after, with their notes as diatonic
 * steps for each voice part from the soprano to the bass (harmony_steps.h; do is 0, re is 1,
 * an octave is 7). All the predominant sevenths share two guidelines for the chordal seventh:
 * it is approached by step or common tone, and it resolves down by step.
 *
 * Hard:
 *
 *   PROGRESSION         the middle chord is a predominant seventh chord (ii7 or an inversion,
 *                       IV7, vi7, or iii7)
 *   CHORD_NOTES         the middle chord has only the notes of its chord, with its root, third,
 *                       and seventh present (the fifth may be omitted, the root doubled), and
 *                       the bass on the root, third, fifth, or seventh of its inversion
 *   SEVENTH_APPROACH    each voice on the chordal seventh reaches it by step or common tone
 *   SEVENTH_RESOLVES    each voice on the chordal seventh goes down a step into the chord
 *                       after
 *   NO_PARALLELS        no two voices make parallel fifths or octaves (a unison counts as an
 *                       octave) from one chord to the next
 *
 * Soft ("best"):
 *
 *   PRECEDED_BY_I6      a ii7 chord (any inversion) is not preceded by I in root position: the
 *                       tonic before it is best I6, which avoids potential parallels. This
 *                       always holds for the other chords
 *
 * Every check returns false for an invalid subject.
 */

typedef struct {
    HarmonyMode mode;
    HarmonyChord chords[3];
    int steps[3][VOICE_PART_COUNT];
} HarmonyPredominantSeventh;

typedef enum {
    HARMONY_PREDOMINANT_SEVENTH_PROGRESSION = 0,
    HARMONY_PREDOMINANT_SEVENTH_CHORD_NOTES,
    HARMONY_PREDOMINANT_SEVENTH_SEVENTH_APPROACH,
    HARMONY_PREDOMINANT_SEVENTH_SEVENTH_RESOLVES,
    HARMONY_PREDOMINANT_SEVENTH_NO_PARALLELS,
    HARMONY_PREDOMINANT_SEVENTH_PRECEDED_BY_I6,
    HARMONY_PREDOMINANT_SEVENTH_RULE_COUNT
} HarmonyPredominantSeventhRule;

/* True for a valid mode and three valid chords. */
bool harmony_predominant_seventh_valid(const HarmonyPredominantSeventh *subject);

const Rule *harmony_predominant_seventh_rule(HarmonyPredominantSeventhRule which);
const Constraint *harmony_predominant_seventh_constraint(HarmonyPredominantSeventhRule which);

/* "progression" "chord-notes" "seventh-approach" "seventh-resolves" "no-parallels"
   "preceded-by-i6"; NULL for an invalid rule. */
const char *harmony_predominant_seventh_rule_name(HarmonyPredominantSeventhRule which);

/* True when the subject satisfies the rule. False when it does not, and for an invalid
   subject or an invalid rule. */
bool harmony_predominant_seventh_rule_holds(HarmonyPredominantSeventhRule which,
                                            const HarmonyPredominantSeventh *subject);

/* The rules it breaks: hard_violations has bit (1u << rule) set for each hard rule that fails,
   soft_violations for each soft rule. False (both untouched) for an invalid subject. */
bool harmony_predominant_seventh_check(const HarmonyPredominantSeventh *subject, unsigned *hard_violations,
                                       unsigned *soft_violations);

#endif
