#ifndef MUSIC_SEMANTICS_MOTION_TYPE_H
#define MUSIC_SEMANTICS_MOTION_TYPE_H

#include <stdbool.h>

/*
 * MotionType (Value): how two parts move relative to each other from one
 * pair of simultaneous notes to the next. A plain enum: it names a
 * relationship and carries no EntityID, no pitches, and no framework claim.
 *
 *   CONTRARY   the parts move in opposite directions (one up, one down)
 *   SIMILAR    the parts move in the same direction by different amounts,
 *              so the interval between them changes type
 *   PARALLEL   the parts move in the same direction by the same distance,
 *              so the starting and ending intervals are of the same type
 *   OBLIQUE    one part moves, the other stays on the same pitch
 *   STATIC     neither part moves
 *
 * The categories are exclusive: a motion is exactly one of them. Parallel
 * motion is the refined case of similar motion (the parts move the same
 * way, and also keep their interval type), so a pair that moves in the
 * same direction is PARALLEL when it keeps the interval and SIMILAR when
 * it does not. motion_type_is_same_direction is true for both.
 *
 * Deciding which type a pair of moves is takes pitch arithmetic, which
 * belongs to a Framework (frameworks/standard-counterpoint). This type
 * only names the result.
 *
 * MotionType is NOT VoiceLeadingMotion (analysis/voice_leading_analysis.h).
 * That enum is a caller-supplied observation of the four classic types
 * and has no STATIC case.
 */

typedef enum {
    MOTION_TYPE_CONTRARY = 0,
    MOTION_TYPE_SIMILAR,
    MOTION_TYPE_PARALLEL,
    MOTION_TYPE_OBLIQUE,
    MOTION_TYPE_STATIC,
    MOTION_TYPE_COUNT
} MotionType;

/* True for the five types above; false for any other value. */
bool motion_type_valid(MotionType type);

/* "contrary" "similar" "parallel" "oblique" "static"; NULL for an invalid
   type. */
const char *motion_type_name(MotionType type);

/* True for SIMILAR and PARALLEL: both parts move the same way. False for
   every other type, and for an invalid type. */
bool motion_type_is_same_direction(MotionType type);

#endif
