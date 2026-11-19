#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_ANALYSIS_ANALYSIS_FUNCTION_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_ANALYSIS_ANALYSIS_FUNCTION_H

#include <stdbool.h>
#include <stddef.h>

#include "analysis/harmony/analysis_input.h"
#include "theory/harmony/harmonic_function.h"

/*
 * Harmonic function per segment (the standard-harmony analysis adapter, step 1). One result per prepared segment:
 * the function of a valid chord, the uncertainty carried from the input.
 *
 * The function is the existing rules': harmony_function_of (harmony_functions.h) for a diatonic chord, by the
 * scale degree of its root (I tonic; ii IV strong and iii vi weak predominants; V vii dominant; the inversion does
 * not change it), and harmony_applied_function (harmony_applied.h) for an applied chord, which is DOMINANT toward
 * its target: the result records function DOMINANT, applied true and applied_to the target's degree.
 *
 * A segment that is not a valid chord, or whose chord the rules give no function (never the case for a valid
 * chord), has no function (has_function false).
 */

typedef struct {
    const char *segment_id;       /* borrowed from the input */
    bool has_function;            /* false: no function */
    HarmonicFunction function;
    bool applied;
    int applied_to;
    double uncertainty;           /* the input's */
} HarmonyFunctionResult;

typedef enum {
    HARMONY_FUNCTION_OK = 0,
    HARMONY_FUNCTION_ERR_NULL_ARGUMENT,
    HARMONY_FUNCTION_ERR_ALLOC
} HarmonyFunctionStatus;

/* One result per prepared segment, in order. On OK, *out is owned by the caller (harmony_function_results_destroy);
   otherwise untouched. */
HarmonyFunctionStatus harmony_analysis_assign_functions(const HarmonySegment *segments, size_t count,
                                                        HarmonyFunctionResult **out);

void harmony_function_results_destroy(HarmonyFunctionResult *results, size_t count);

#endif
