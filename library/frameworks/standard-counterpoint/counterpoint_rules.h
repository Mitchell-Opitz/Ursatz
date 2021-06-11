#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_COUNTERPOINT_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_COUNTERPOINT_RULES_H

#include <stdbool.h>

#include "counterpoint_limits.h"
#include "counterpoint_line.h"
#include "pitch/pitch_spelling.h"

/*
 * Checking a whole two-part exercise against the rule groups of
 * standard-counterpoint-v1:
 *
 *   CANTUS_FIRMUS   cantus_firmus_rules.h, on the part that is the
 *                   cantus firmus
 *   MELODIC         melodic_rules.h, on both parts
 *   HARMONIC        harmonic_rules.h, on the two parts together
 *   TENDENCY        tendency_rules.h, on both parts (soft only)
 *
 * The result reports a bit (1u << group) in hard_violations for each group
 * with a hard rule broken, and in soft_violations for each group with a
 * soft rule (a "limit", "avoid", or "prefer" rule) broken. An exercise is
 * valid when no hard rule is broken; soft violations are advice. To learn
 * exactly which rule failed, ask the group's own check.
 */

typedef enum {
    STANDARD_COUNTERPOINT_GROUP_CANTUS_FIRMUS = 0,
    STANDARD_COUNTERPOINT_GROUP_MELODIC,
    STANDARD_COUNTERPOINT_GROUP_HARMONIC,
    STANDARD_COUNTERPOINT_GROUP_TENDENCY,
    STANDARD_COUNTERPOINT_GROUP_COUNT
} StandardCounterpointGroup;

/* "cantus-firmus" "melodic" "harmonic" "tendency"; NULL for an invalid
   group. */
const char *standard_counterpoint_group_name(StandardCounterpointGroup group);

/* Which part of the pair is the cantus firmus. */
typedef enum {
    STANDARD_COUNTERPOINT_CANTUS_IN_LOWER = 0,
    STANDARD_COUNTERPOINT_CANTUS_IN_UPPER
} StandardCounterpointCantusPart;

/*
 * The exercise and what the caller knows about it:
 *
 *   texture         the two parts (every note non-NULL, equal lengths)
 *   cantus_part     which part is the cantus firmus
 *   tonic           the tonic of the mode, or NULL when it is not known
 *                   (the cantus firmus group is then skipped)
 *   limits          the limits for the melodic rules, or NULL for
 *                   standard_counterpoint_default_limits()
 */

typedef struct {
    const StandardCounterpointPair *texture;
    StandardCounterpointCantusPart cantus_part;
    const PitchSpelling *tonic;
    const StandardCounterpointLimits *limits;
} StandardCounterpointInput;

typedef struct {
    unsigned hard_violations;
    unsigned soft_violations;
} StandardCounterpointResult;

/* Runs the groups over the exercise. False (*out untouched) for invalid
   input: NULL, an invalid pair, or a cantus part that is not one of the
   two. */
bool standard_counterpoint_check(const StandardCounterpointInput *input, StandardCounterpointResult *out);

/* True when the input is valid and breaks no hard rule. */
bool standard_counterpoint_valid(const StandardCounterpointInput *input);

#endif
