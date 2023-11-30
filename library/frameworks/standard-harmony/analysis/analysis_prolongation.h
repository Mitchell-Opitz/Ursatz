#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_ANALYSIS_ANALYSIS_PROLONGATION_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_ANALYSIS_ANALYSIS_PROLONGATION_H

#include <stdbool.h>
#include <stddef.h>

#include "analysis_input.h"
#include "harmonic_function.h"
#include "interpretation/interpretation.h"
#include "relationship/relationship.h"

/*
 * Prolongations of tonic and dominant (the standard-harmony analysis adapter, step 4). To prolong a harmony is to
 * make its influence last past a single chord. A prolongation here is a SPAN from one chord of function F (tonic or
 * dominant) to the next chord of function F, whose INTERIOR chords are all explained by the existing rules:
 *
 *   PASSING_SIX_FOUR      a 6/4 classified PASSING (harmony_six_four.h): I V6/4 I6
 *   NEIGHBOR_SIX_FOUR     a 6/4 classified NEIGHBOR: V I6/4 V, I IV6/4 I
 *   ARPEGGIATING_SIX_FOUR a 6/4 classified ARPEGGIATING; it may prolong any harmony
 *   TONIC_LINE            the middle chord of a tonic progression harmony_prolongation_of (harmony_prolongation.h)
 *                         recognises: I, then V6, V6/5, V4/3, V4/2 or the leading-tone chord, then I or I6
 *   APPLIED               an applied chord (of any kind) whose target is the closing chord, the chord just before
 *                         it: the applied chord leads into the closing chord and leaves the function standing
 *
 * Chords are runs: consecutive segments making the same chord count once. Any other interior chord (an unexplained
 * chord, an invalid or unclassified segment) means there is no prolongation from that chord. A span with no interior
 * chord (the same function twice in a row) is not a prolongation, there being nothing between. Tonic lines chain: I V6/5 I V4/3 I6 is two spans, the
 * second beginning where the first ends (harmony_prolongation_chain.h). The existing rules model spans only from
 * one T (or D) to the NEXT, so spans never nest; where a nesting could be read the spans are the outermost
 * single-function ones as found, and none is split.
 *
 * OUTPUT, the durable claim pattern, one pair per interior SEGMENT of a span:
 *   - a Relationship, predicate "prolongs": source = the span's first segment's Sonority id (the prolonged harmony's
 *     first segment), target = the interior segment's Sonority id; scope = a HarmonyProlongationScope (the function,
 *     the closing segment's id, how many interior segments); qualifiers = a HarmonyProlongationReason;
 *   - an Interpretation targeting that Relationship's id (not a segment): FrameworkReference standard-harmony,
 *     claim = a HarmonyProlongationClaim, uncertainty = the largest of the span's runs' input uncertainties.
 */

#define HARMONY_PROLONGS "prolongs"
#define HARMONY_PROLONGATION_CLAIM_TYPE "standard-harmony/prolongation"
#define HARMONY_PROLONGATION_SCOPE_TYPE "standard-harmony/prolongation-scope"
#define HARMONY_PROLONGATION_REASON_TYPE "standard-harmony/prolongation-reason"

typedef enum {
    HARMONY_PROLONGATION_REASON_PASSING_SIX_FOUR = 0,
    HARMONY_PROLONGATION_REASON_NEIGHBOR_SIX_FOUR,
    HARMONY_PROLONGATION_REASON_ARPEGGIATING_SIX_FOUR,
    HARMONY_PROLONGATION_REASON_TONIC_LINE,
    HARMONY_PROLONGATION_REASON_APPLIED,
    HARMONY_PROLONGATION_REASON_COUNT
} HarmonyProlongationReason;

const char *harmony_prolongation_reason_name(HarmonyProlongationReason reason);

typedef struct {
    int function;           /* HarmonicFunction: TONIC or DOMINANT */
    char closing_segment[96];
    size_t interior_count;  /* the interior segments of the span */
} HarmonyProlongationScope;

typedef struct {
    int reason;             /* HarmonyProlongationReason */
} HarmonyProlongationReasonValue;

typedef struct {
    int function;           /* HarmonicFunction */
    int reason;             /* HarmonyProlongationReason */
} HarmonyProlongationClaim;

typedef struct {
    HarmonicFunction function;
    size_t first_segment;          /* the first segment of the prolonged harmony */
    size_t closing_segment;        /* the first segment of the closing chord */
    size_t *interior_segments;     /* owned: every segment between them, in order */
    HarmonyProlongationReason *reasons; /* owned: one per interior segment */
    Relationship **relationships;  /* owned: one per interior segment */
    Interpretation **interpretations; /* owned: one per relationship, targeting its id */
    size_t interior_count;
} HarmonyProlongationSpan;

typedef struct {
    HarmonyProlongationSpan *spans; /* owned; in order of first_segment; NULL when none */
    size_t count;
} HarmonyProlongations;

typedef enum {
    HARMONY_PROLONGATION_OK = 0,
    HARMONY_PROLONGATION_ERR_NULL_ARGUMENT,
    HARMONY_PROLONGATION_ERR_ALLOC
} HarmonyProlongationStatus;

/* Finds the tonic and dominant prolongations. On OK, *out is owned by the caller (harmony_prolongations_destroy);
   otherwise untouched. */
HarmonyProlongationStatus harmony_analysis_find_prolongations(const HarmonySegment *segments, size_t count,
                                                              HarmonyProlongations *out);

void harmony_prolongations_destroy(HarmonyProlongations *prolongations);

/* Reads the claim back out of a prolongation Interpretation. False if it is not one. */
bool harmony_prolongation_claim_read(const Interpretation *interpretation, HarmonyProlongationClaim *out);

#endif
