#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_SEVENTH_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_SEVENTH_RULES_H

#include <stdbool.h>

#include "harmony_chord.h"
#include "rules/constraint.h"
#include "rules/rule.h"
#include "seventh_resolution.h"
#include "semantics/voice_part.h"

/*
 * V7 resolution rules for standard-harmony-v1: a dominant seventh chord, V7
 * (a V triad with a seventh above its root, which adds a dissonant note and so
 * strengthens the pull toward tonic), resolving to tonic in four voices. The
 * subject is the chords' notes as scale degrees (1 to 7: do is 1, sol is 5), a
 * degree for each voice part from the soprano to the bass, in root position (the
 * bass is the root of each chord). V7 is sol (root), ti (third), re (fifth), and
 * fa (seventh); I is do, mi, and sol.
 *
 * The active notes of V7 resolve by their tendencies:
 *
 *   fa (the seventh)   down by step to mi (me in minor)
 *   ti (the leading    up by step to do
 *   tone)
 *   re                 down by step to do (up to mi is possible, but very
 *                      uncommon: it makes a tonic chord with two roots and two
 *                      thirds, which usually leads to voice-leading problems)
 *
 * and the bass goes sol to do. Resolving all of them gives a tonic chord with
 * three roots and one third and no fifth, a completely normal resolution: the
 * fifth gives no essential information about the chord, while the root names it
 * and the third gives its quality. In minor the V7 uses ti, not te, to be a
 * major-minor seventh chord.
 *
 * Two alternative resolutions make the tonic chord complete (root, third, and
 * fifth) instead of leaving out the fifth, with the same steps for writing:
 *
 *   incomplete V7      the fifth (re) is omitted from the V7 and the root (sol)
 *                      doubled to keep four voices, so sol, sol, ti, fa. Only the
 *                      root can be doubled: doubling ti or fa would make parallel
 *                      octaves when both resolve by their tendencies. The doubled
 *                      sol stays a sol over the tonic chord's fifth
 *   leading-tone drop  in a complete V7, ti leaps down to sol (never to mi, which
 *                      is further away) instead of up to do, in the alto or tenor
 *                      only, so that the leap is hidden. It works because ti is
 *                      not dissonant, an inner voice is not very noticeable, and
 *                      sol is the closest note it can move to without making
 *                      parallels
 *
 * Hard:
 *
 *   BASS_SOL_TO_DO      the bass is sol in V7 and do in I
 *   V7_NOTES            V7 is a complete chord (sol, ti, re, and fa, one each) or
 *                       an incomplete one (sol twice, ti, and fa)
 *   ACTIVE_NOTES_RESOLVE
 *                       fa goes to mi, re goes to do or mi, and ti goes to do; or
 *                       ti goes to sol, in the alto or tenor of a complete V7
 *   TONIC_NOTES         the tonic chord has only do, mi, and sol, and has do and
 *                       mi (the root and the third are essential)
 *   MINOR_LEADING_TONE  in minor, the V7 has the leading tone (ti); in major
 *                       this always holds
 *
 * Soft:
 *
 *   AVOID_RE_TO_MI      no voice goes from re to mi
 *
 * Every check returns false for an invalid resolution.
 */

typedef struct {
    HarmonyMode mode;
    int dominant[VOICE_PART_COUNT];
    int tonic[VOICE_PART_COUNT];
    bool dominant_has_leading_tone;
} HarmonySeventhResolution;

typedef enum {
    HARMONY_SEVENTH_BASS_SOL_TO_DO = 0,
    HARMONY_SEVENTH_V7_NOTES,
    HARMONY_SEVENTH_ACTIVE_NOTES_RESOLVE,
    HARMONY_SEVENTH_TONIC_NOTES,
    HARMONY_SEVENTH_MINOR_LEADING_TONE,
    HARMONY_SEVENTH_AVOID_RE_TO_MI,
    HARMONY_SEVENTH_RULE_COUNT
} HarmonySeventhRule;

/* True for a valid mode and a degree from 1 to 7 for every voice of both chords. */
bool harmony_seventh_valid(const HarmonySeventhResolution *resolution);

const Rule *harmony_seventh_rule(HarmonySeventhRule which);
const Constraint *harmony_seventh_constraint(HarmonySeventhRule which);

/* "bass-sol-to-do" "v7-notes" "active-notes-resolve" "tonic-notes"
   "minor-leading-tone" "avoid-re-to-mi"; NULL for an invalid rule. */
const char *harmony_seventh_rule_name(HarmonySeventhRule which);

/* True when the resolution satisfies the rule. False when it does not, and for an
   invalid resolution or an invalid rule. */
bool harmony_seventh_rule_holds(HarmonySeventhRule which, const HarmonySeventhResolution *resolution);

/* The resolution of a V7 voicing that breaks no hard rule: INCOMPLETE when the V7
   omits the fifth, LEADING_TONE_DROP when a ti goes down to sol, and DEFAULT
   otherwise. False (*out untouched) when a hard rule fails or the resolution is
   invalid. */
bool harmony_seventh_classify(const HarmonySeventhResolution *resolution, SeventhResolution *out);

/* The rules the resolution breaks: hard_violations has bit (1u << rule) set for
   each hard rule that fails, soft_violations for each soft rule. False (both
   untouched) for an invalid resolution. */
bool harmony_seventh_check(const HarmonySeventhResolution *resolution, unsigned *hard_violations,
                           unsigned *soft_violations);

#endif
