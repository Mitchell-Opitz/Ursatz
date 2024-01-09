#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_ANALYSIS_ANALYSIS_PERFECTS_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_ANALYSIS_ANALYSIS_PERFECTS_H

#include <stdbool.h>
#include <stddef.h>

#include "analysis_grid.h"
#include "interpretation/interpretation.h"
#include "structure/structure.h"

/*
 * Parallel and direct perfect intervals across separated voices (the standard-counterpoint analysis adapter, step 2). The
 * rules are the existing ones (harmonic_rules.h): the adapter feeds them two-note lines built from the grid and reads their
 * answer; it defines no interval or motion of its own.
 *
 *   PARALLEL_FIFTHS    the same voice pair makes a perfect fifth (compound included: a twelfth is a fifth) in two
 *                      consecutive segments, both voices moving in the same direction by the same distance
 *                      (PARALLEL motion, motion_rules.h). A repeated fifth is not motion and is not flagged. Every pair
 *   PARALLEL_OCTAVES   the same with perfect octaves (compound included) or unisons (a unison counts as an octave).
 *                      Every pair
 *   DIRECT_PERFECTS    the OUTER pair only (the top and the bottom voice): arrival at a perfect fifth, octave or unison by
 *                      similar motion, with the TOP voice moving by LEAP (a third or more). The existing rule is wider
 *                      (any pair, no leap condition; it serves the species exercises), and this is its narrower use in
 *                      real music: direct fifths and octaves are a fault in the outer voices, where a leap makes them
 *                      audible. A pair already flagged parallel is not also flagged direct
 *
 * Pairs where either voice rests in either segment, and pairs whose interval cannot be formed from the spellings, are not
 * checked. The intervals are between the two pitches whichever sounds higher, so voices that have crossed are still
 * read; there is no crossing check here yet (a later analyzer reports crossing, doubling and spacing).
 *
 * Each occurrence is a Structure shell (the note ids of the two voices in both segments) and an Interpretation targeting the
 * shell's id: FrameworkReference standard-counterpoint, claim = a StandardCounterpointViolationClaim (the rule name, the voice
 * pair, the segment pair, the interval arrived at), and
 *
 *   uncertainty = 1 - (the least voice-assignment confidence of the four cells involved).
 */

#define STANDARD_COUNTERPOINT_VIOLATION_CLAIM_TYPE "standard-counterpoint/voice-leading-violation"

typedef enum {
    STANDARD_COUNTERPOINT_VIOLATION_PARALLEL_FIFTHS = 0,
    STANDARD_COUNTERPOINT_VIOLATION_PARALLEL_OCTAVES,
    STANDARD_COUNTERPOINT_VIOLATION_DIRECT_PERFECTS,
    STANDARD_COUNTERPOINT_VIOLATION_COUNT
} StandardCounterpointViolationRule;

/* "parallel-fifths" "parallel-octaves" "direct-perfects"; NULL for an invalid rule. */
const char *standard_counterpoint_violation_rule_name(StandardCounterpointViolationRule rule);

typedef struct {
    int rule; /* StandardCounterpointViolationRule */
    char rule_name[24];
    int upper_voice;
    int lower_voice;
    int segment_from;
    int segment_to;
    int arrival_interval; /* the simple size arrived at: 5 for a fifth, 8 for an octave, 1 for a unison */
} StandardCounterpointViolationClaim;

typedef struct {
    StandardCounterpointViolationRule rule;
    size_t segment_from;
    size_t segment_to;
    size_t upper_voice;
    size_t lower_voice;
    int arrival_interval;
    double uncertainty;
    Structure *shell;               /* owned */
    Interpretation *interpretation; /* owned; targets the shell's id */
} StandardCounterpointViolation;

typedef struct {
    StandardCounterpointViolation *items; /* owned; in segment order, then voice pair; NULL when none */
    size_t count;
} StandardCounterpointViolations;

typedef enum {
    STANDARD_COUNTERPOINT_PERFECTS_OK = 0,
    STANDARD_COUNTERPOINT_PERFECTS_ERR_NULL_ARGUMENT,
    STANDARD_COUNTERPOINT_PERFECTS_ERR_ALLOC
} StandardCounterpointPerfectsStatus;

/* Finds every occurrence. On OK, *out is owned by the caller (standard_counterpoint_violations_destroy); otherwise
   untouched. */
StandardCounterpointPerfectsStatus standard_counterpoint_check_perfects(const StandardCounterpointPreparedGrid *grid,
                                                                        StandardCounterpointViolations *out);

void standard_counterpoint_violations_destroy(StandardCounterpointViolations *violations);

/* Reads the claim back out of a violation Interpretation. False if it is not one. */
bool standard_counterpoint_violation_claim_read(const Interpretation *interpretation,
                                                StandardCounterpointViolationClaim *out);

#endif
