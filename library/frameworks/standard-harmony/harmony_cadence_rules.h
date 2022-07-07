#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_CADENCE_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_CADENCE_RULES_H

#include <stdbool.h>

#include "harmony_chord.h"
#include "rules/constraint.h"
#include "rules/rule.h"
#include "semantics/cadence_type.h"

/*
 * Cadence classification rules for standard-harmony-v1. A cadence is a melodic
 * and harmonic pattern that ends a phrase, like punctuation in a sentence.
 * There are two primary kinds:
 *
 *   authentic   the progression V to I (V to i in minor), which sounds
 *               conclusive. Perfect (PAC) when both chords are in root position
 *               and do is in the soprano over the tonic chord; imperfect (IAC)
 *               when either condition fails (mi or sol in the soprano, or V, I,
 *               or both inverted)
 *   half        a phrase that ends on V, whatever chord x came before it (the
 *               progression x to V), which sounds inconclusive
 *
 * A progression that is neither is not a cadence, though it may still be some
 * kind of ending. A potential cadence that is avoided, because what follows
 * repeats material from the middle of the phrase, is subverted. Whether it is
 * subverted needs the music that follows, so the caller says so (`subverted`).
 *
 * The strength of a cadence runs from the half cadence (weakest), through the
 * imperfect authentic, to the perfect authentic (strongest). An imperfect
 * authentic cadence can itself be made more or less strong (the melody, the
 * rhythm, a rest after it); that is not modeled.
 *
 * Hard:
 *
 *   CADENTIAL_PROGRESSION   the progression is V to I or x to V
 *
 * The subject is the last two chords of the phrase and the scale degree of the
 * soprano over the last one (1 to 7: do is 1, mi is 3).
 */

typedef struct {
    HarmonyChord from;
    HarmonyChord to;
    int soprano_degree;
    bool subverted;
} HarmonyCadence;

typedef enum {
    HARMONY_CADENCE_CADENTIAL_PROGRESSION = 0,
    HARMONY_CADENCE_RULE_COUNT
} HarmonyCadenceRule;

/* True for two valid chords and a soprano degree from 1 to 7. */
bool harmony_cadence_valid(const HarmonyCadence *cadence);

const Rule *harmony_cadence_rule(HarmonyCadenceRule which);
const Constraint *harmony_cadence_constraint(HarmonyCadenceRule which);

/* "cadential-progression"; NULL for an invalid rule. */
const char *harmony_cadence_rule_name(HarmonyCadenceRule which);

/* True when the cadence satisfies the rule. False when it does not, and for an
   invalid cadence or an invalid rule. */
bool harmony_cadence_rule_holds(HarmonyCadenceRule which, const HarmonyCadence *cadence);

/* The rules the cadence breaks: hard_violations has bit (1u << rule) set for each
   hard rule that fails; soft_violations is always 0 (there are no soft rules).
   False (both untouched) for an invalid cadence. */
bool harmony_cadence_check(const HarmonyCadence *cadence, unsigned *hard_violations, unsigned *soft_violations);

/* The type of the cadence: subverted when the progression is cadential and
   `subverted` is set, otherwise PAC, IAC, or HC by the rules above. False (*out
   untouched) when the progression is not cadential and for an invalid cadence. */
bool harmony_cadence_classify(const HarmonyCadence *cadence, CadenceType *out);

/* How conclusive a type is: 0 subverted (no cadence), 1 half, 2 imperfect
   authentic, 3 perfect authentic. -1 for an invalid type. */
int harmony_cadence_strength(CadenceType type);

#endif
