#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_ANALYSIS_ANALYSIS_MOTION_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_ANALYSIS_ANALYSIS_MOTION_H

/*
 * Owner of motion classification (standard_counterpoint_classify_motions) (docs/analytic-job-owners.md,
 * Analytic job owners).
 */

#include <stdbool.h>
#include <stddef.h>

#include "analysis/counterpoint/analysis_grid.h"
#include "theory/counterpoint/motion_type.h"

/*
 * Motion between voice pairs (the standard-counterpoint analysis adapter, step 1). For every pair of voices and every pair
 * of CONSECUTIVE segments where both voices sound in both, the motion is classified by the existing rule
 * (standard_counterpoint_classify_motion, motion_rules.h): CONTRARY, SIMILAR, PARALLEL (same direction, same distance in
 * letter names), OBLIQUE (one voice holds its pitch), STATIC (neither moves). Nothing is repeated here.
 *
 * Pairs are (upper, lower) with upper < lower in voice index (voice 0 is the top). A voice that rests in either segment
 * gives no motion for any pair it is in.
 *
 * Each motion carries its type, voices and segments, and
 *
 *   uncertainty = 1 - (the least voice-assignment confidence of the cells involved)
 *
 * (no harmony rule is involved here, so there is no chord uncertainty to combine).
 */


typedef struct {
    int motion_type; /* MotionType */
    int upper_voice;
    int lower_voice;
    int segment_from;
    int segment_to;
} StandardCounterpointMotionClaim;

typedef struct {
    size_t segment_from; /* the motion is from this segment to segment_from + 1 */
    size_t segment_to;
    size_t upper_voice;
    size_t lower_voice;
    MotionType type;
    double uncertainty;
} StandardCounterpointMotion;

typedef struct {
    StandardCounterpointMotion *items; /* owned; in segment order, then voice pair; NULL when none */
    size_t count;
} StandardCounterpointMotions;

typedef enum {
    STANDARD_COUNTERPOINT_MOTION_OK = 0,
    STANDARD_COUNTERPOINT_MOTION_ERR_NULL_ARGUMENT,
    STANDARD_COUNTERPOINT_MOTION_ERR_ALLOC
} StandardCounterpointMotionStatus;

/* Classifies every motion. On OK, *out is owned by the caller (standard_counterpoint_motions_destroy); otherwise
   untouched. */
StandardCounterpointMotionStatus standard_counterpoint_classify_motions(const StandardCounterpointPreparedGrid *grid,
                                                                        StandardCounterpointMotions *out);

void standard_counterpoint_motions_destroy(StandardCounterpointMotions *motions);

#endif
