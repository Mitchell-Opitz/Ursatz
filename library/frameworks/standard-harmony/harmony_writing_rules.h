#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_WRITING_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_WRITING_RULES_H

#include <stdbool.h>

#include "harmony_chord.h"
#include "rules/constraint.h"
#include "rules/rule.h"
#include "semantics/cadence_type.h"

/*
 * Cadence writing rules for standard-harmony-v1: the outer voices of a cadence
 * written with the triads I and V only (root position, so the bass is the root).
 * Write the entire bass, then the entire soprano; the inner voices come after,
 * from the notes still needed to complete each chord, usually doubling the bass
 * in a root position chord. Voicing the inner voices is
 * frameworks/standard-voicing's, so it is not modeled here. A cadence is two
 * chords, so the bass and the soprano are each two scale degrees (1 to 7: do is
 * 1, sol is 5), the first over the first chord and the second over the second.
 *
 *   PAC   bass sol to do; soprano re to do, or ti to do
 *   IAC   bass sol to do; soprano re to mi, or sol to sol
 *   HC    bass do to sol; soprano do to ti, or mi to re (sol to sol is possible,
 *         but not common)
 *
 * In minor, the V chord needs the leading tone (ti), which makes it a major
 * triad, not minor, to give it momentum toward the tonic.
 *
 * Hard:
 *
 *   BASS_MOTION         the bass makes the motion the type needs
 *   SOPRANO_MELODY      the soprano makes one of the motions the type allows
 *   MINOR_LEADING_TONE  in minor, the V chord has the leading tone; in major this
 *                       always holds
 *
 * Soft:
 *
 *   COMMON_HC_SOPRANO   a half cadence's soprano is do to ti or mi to re, not the
 *                       uncommon sol to sol; this always holds for the other types
 *
 * A subverted cadence is not written, so every rule fails for it. Every check
 * returns false for an invalid cadence.
 */

typedef struct {
    HarmonyMode mode;
    CadenceType type;
    int bass[2];
    int soprano[2];
    bool dominant_has_leading_tone;
} HarmonyCadenceWriting;

typedef enum {
    HARMONY_WRITING_BASS_MOTION = 0,
    HARMONY_WRITING_SOPRANO_MELODY,
    HARMONY_WRITING_MINOR_LEADING_TONE,
    HARMONY_WRITING_COMMON_HC_SOPRANO,
    HARMONY_WRITING_RULE_COUNT
} HarmonyWritingRule;

/* True for a valid mode, a PAC, IAC, or HC, and bass and soprano degrees from 1 to 7. */
bool harmony_writing_valid(const HarmonyCadenceWriting *writing);

const Rule *harmony_writing_rule(HarmonyWritingRule which);
const Constraint *harmony_writing_constraint(HarmonyWritingRule which);

/* "bass-motion" "soprano-melody" "minor-leading-tone" "common-hc-soprano"; NULL
   for an invalid rule. */
const char *harmony_writing_rule_name(HarmonyWritingRule which);

/* True when the cadence satisfies the rule. False when it does not, and for an
   invalid cadence or an invalid rule. */
bool harmony_writing_rule_holds(HarmonyWritingRule which, const HarmonyCadenceWriting *writing);

/* The rules the cadence breaks: hard_violations has bit (1u << rule) set for each
   hard rule that fails, soft_violations for each soft rule. False (both
   untouched) for an invalid cadence. */
bool harmony_writing_check(const HarmonyCadenceWriting *writing, unsigned *hard_violations,
                           unsigned *soft_violations);

#endif
