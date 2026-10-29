#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_ANALYSIS_ANALYSIS_FUNCTION_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_ANALYSIS_ANALYSIS_FUNCTION_H

#include <stdbool.h>
#include <stddef.h>

#include "analysis_input.h"
#include "interpretation/interpretation.h"
#include "standard-harmony/harmonic_function.h"

/*
 * Harmonic function per segment (the standard-harmony analysis adapter, step 1). One Interpretation per valid
 * segment: target = the segment's Sonority id, FrameworkReference standard-harmony, claim = a
 * HarmonyFunctionClaim, the uncertainty carried from the input.
 *
 * The function is the existing rules': harmony_function_of (harmony_functions.h) for a diatonic chord, by the
 * scale degree of its root (I tonic; ii IV strong and iii vi weak predominants; V vii dominant; the inversion does
 * not change it), and harmony_applied_function (harmony_applied.h) for an applied chord, which is DOMINANT toward
 * its target: the claim records function DOMINANT, applied true and applied_to the target's degree (and the kind).
 *
 * A segment that is not a valid chord, or whose chord the rules give no function (never the case for a valid
 * chord), has no Interpretation.
 */

#define HARMONY_FUNCTION_CLAIM_TYPE "standard-harmony/harmonic-function"

typedef struct {
    int function;      /* HarmonicFunction */
    bool applied;      /* an applied chord: dominant toward applied_to */
    int applied_to;    /* the target's scale degree, 1..7; 0 when not applied */
    int applied_kind;  /* AppliedChordKind; meaningful only when applied */
} HarmonyFunctionClaim;

typedef struct {
    const char *segment_id;       /* borrowed from the input */
    bool has_function;            /* false: no Interpretation */
    HarmonicFunction function;
    bool applied;
    int applied_to;
    double uncertainty;           /* the input's */
    Interpretation *interpretation; /* owned; NULL when !has_function */
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

/* Reads the claim back out of a function Interpretation. False if it is not one. */
bool harmony_function_claim_read(const Interpretation *interpretation, HarmonyFunctionClaim *out);

#endif
