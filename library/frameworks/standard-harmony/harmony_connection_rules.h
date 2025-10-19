#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_CONNECTION_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_CONNECTION_RULES_H

#include <stdbool.h>

#include "harmony_chord.h"
#include "rules/constraint.h"
#include "rules/preference.h"
#include "rules/rule.h"
#include "semantics/voice_part.h"

/*
 * La connecting the tonic and strong predominant areas, for standard-harmony-v1 (La in the
 * Bass at Beginnings, Middles, and Endings). In the middle of a phrase the bass can
 * arpeggiate down do la fa, and la is harmonized by vi (a weak predominant) or IV6, to
 * connect tonic to a strong predominant: three chords, I in root position, then vi (the
 * triad on degree 6, root position) or IV6, then IV (root position) or ii6. It is quite easy
 * to write: the upper voices move by step or common tone. For the phrase model labels, the
 * PD label goes on the first strong predominant before the cadence, so the vi or IV6 that
 * connects the areas is not labeled (harmony_phrase_labels.h).
 *
 * The subject is the three chords and their notes as diatonic steps for each voice part from
 * the soprano to the bass (harmony_steps.h; do is 0, re is 1, an octave is 7).
 *
 * Hard:
 *
 *   PROGRESSION          the chords are I, then vi or IV6, then IV or ii6, all triads
 *   BASS_LINE            the bass is do la fa, and each move is a leap down
 *   CHORD_NOTES          each chord has only the notes of its chord, with its root and third
 *                        present, over the bass note of the line
 *   STEP_OR_COMMON_TONE  each upper voice moves by step or stays on its note
 *   NO_PARALLELS         no two voices make parallel fifths or octaves (a unison counts as an
 *                        octave)
 *
 * Every check returns false for an invalid subject.
 */

typedef struct {
    HarmonyMode mode;
    HarmonyChord chords[3];
    int steps[3][VOICE_PART_COUNT];
} HarmonyConnection;

typedef enum {
    HARMONY_CONNECTION_PROGRESSION = 0,
    HARMONY_CONNECTION_BASS_LINE,
    HARMONY_CONNECTION_CHORD_NOTES,
    HARMONY_CONNECTION_STEP_OR_COMMON_TONE,
    HARMONY_CONNECTION_NO_PARALLELS,
    HARMONY_CONNECTION_RULE_COUNT
} HarmonyConnectionRule;

/* True when the three chords are I (root position), then vi (root position) or IV6, then IV
   (root position) or ii6, all triads and none cadential. False for any other chords, an
   invalid chord, and NULL. */
bool harmony_connection_progression_is(const HarmonyChord chords[3]);

/* True for a valid mode and three valid chords. */
bool harmony_connection_valid(const HarmonyConnection *connection);

const Rule *harmony_connection_rule(HarmonyConnectionRule which);
const Constraint *harmony_connection_constraint(HarmonyConnectionRule which);
/* The soft rule's Preference (scores the rule 1/1 when it holds, 0/1 when not); NULL for a hard rule. */
const Preference *harmony_connection_preference(HarmonyConnectionRule which);

/* "progression" "bass-line" "chord-notes" "step-or-common-tone" "no-parallels"; NULL for an
   invalid rule. */
const char *harmony_connection_rule_name(HarmonyConnectionRule which);

/* True when the connection satisfies the rule. False when it does not, and for an invalid
   subject or an invalid rule. */
bool harmony_connection_rule_holds(HarmonyConnectionRule which, const HarmonyConnection *connection);

/* The rules it breaks: hard_violations has bit (1u << rule) set for each hard rule that fails.
   soft_violations is always written as 0, since every rule here is hard. False (both
   untouched) for an invalid subject. */
bool harmony_connection_check(const HarmonyConnection *connection, unsigned *hard_violations,
                              unsigned *soft_violations);

#endif
