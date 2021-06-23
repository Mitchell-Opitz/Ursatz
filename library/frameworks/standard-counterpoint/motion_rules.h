#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_MOTION_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_MOTION_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "counterpoint_line.h"
#include "pitch/pitch_spelling.h"
#include "rules/constraint.h"
#include "rules/rule.h"
#include "semantics/motion_type.h"

/*
 * Motion rules for standard-counterpoint-v1: how two parts move from one
 * pair of simultaneous notes to the next. A part "moves" when its next
 * note is a different pitch (letter, accidental, or octave).
 *
 *   CONTRARY   both parts move, in opposite directions
 *   SIMILAR    both parts move in the same direction, by different
 *              distances
 *   PARALLEL   both parts move in the same direction by the same
 *              distance, so the interval between them keeps its generic
 *              size (thirds stay thirds, whatever their quality)
 *   OBLIQUE    one part moves, the other keeps its pitch
 *   STATIC     neither part moves
 *
 * "Distance" is counted in letter names (diatonic steps). The categories
 * are exclusive; same-direction motion is PARALLEL when the distances are
 * equal and SIMILAR otherwise (motion_type_is_same_direction is true for
 * both). The type itself is semantics/motion_type.h.
 */

typedef enum {
    STANDARD_COUNTERPOINT_MOTION_RULE_CONTRARY = 0,
    STANDARD_COUNTERPOINT_MOTION_RULE_SIMILAR,
    STANDARD_COUNTERPOINT_MOTION_RULE_PARALLEL,
    STANDARD_COUNTERPOINT_MOTION_RULE_OBLIQUE,
    STANDARD_COUNTERPOINT_MOTION_RULE_STATIC,
    STANDARD_COUNTERPOINT_MOTION_RULE_COUNT
} StandardCounterpointMotionRule;

const Rule *standard_counterpoint_motion_rule(StandardCounterpointMotionRule which);
const Constraint *standard_counterpoint_motion_constraint(StandardCounterpointMotionRule which);

/* The motion of two parts from one pair of notes to the next. False (*out
   untouched) when any pitch is NULL. */
bool standard_counterpoint_classify_motion(const PitchSpelling *lower_from, const PitchSpelling *lower_to,
                                           const PitchSpelling *upper_from, const PitchSpelling *upper_to,
                                           MotionType *out);

/* The motion between positions `index` and `index + 1` of a two-part
   texture. False (*out untouched) for an invalid pair or an index with no
   next position. */
bool standard_counterpoint_pair_motion(const StandardCounterpointPair *pair, size_t index, MotionType *out);

#endif
