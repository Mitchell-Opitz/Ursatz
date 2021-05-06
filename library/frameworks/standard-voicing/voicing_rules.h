#ifndef URSATZ_FRAMEWORKS_STANDARD_VOICING_VOICING_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_VOICING_VOICING_RULES_H

#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * The six rules a chord in strict SATB style follows, as standard-voicing-v1
 * names them. Each has a backing Rule/Constraint pair (hard) referencing
 * this Framework; the checks themselves are in the header named below.
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

const Rule *standard_voicing_rule(StandardVoicingRule which);
const Constraint *standard_voicing_constraint(StandardVoicingRule which);

/* "stem-direction" "chord-construction" "range" "spacing" "voice-crossing"
   "doubling"; NULL for an invalid rule. */
const char *standard_voicing_rule_name(StandardVoicingRule which);

#endif
