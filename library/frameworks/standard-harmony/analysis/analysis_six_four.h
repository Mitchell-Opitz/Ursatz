#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_ANALYSIS_ANALYSIS_SIX_FOUR_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_ANALYSIS_ANALYSIS_SIX_FOUR_H

#include <stdbool.h>
#include <stddef.h>

#include "analysis_input.h"
#include "interpretation/interpretation.h"
#include "standard-harmony/six_four_type.h"

/*
 * 6/4 chords (the standard-harmony analysis adapter, step 3). Every valid diatonic segment that is a triad in
 * second inversion is a 6/4 segment. It gets a SixFourType Interpretation when it matches one of the types below,
 * and is left UNCLASSIFIED (no Interpretation) when it matches none.
 *
 * RUNS. Consecutive segments that make the same chord are one chord sounded over several segments. A 6/4 is judged
 * as a run: its neighbors are the chord before the run and the chord after it, its beat strength is the run's first
 * segment's, and every segment of the run gets the same result. A neighbor that is invalid or applied makes the 6/4
 * unclassified (the exception is the cadential 6/4, which only needs the chord after).
 *
 * THE TYPES, by the rules of harmony_six_four.h (harmony_six_four_classify), whose conditions are not repeated:
 *
 *   CADENTIAL     a tonic triad in second inversion with the bass on scale degree 5, on a STRONGER beat than the V
 *                 after it, which has the same bass (degree 5). harmony_analysis_prepare marks it (the chord's
 *                 `cadential` flag), so the function, phrase and six-four steps all read it as the V's embellishment
 *   PASSING       the bass goes by step through the 6/4 in one direction between chords of the same function, and
 *                 the 6/4 is on a weak beat: weaker than the chord before it, or weak
 *   NEIGHBOR      the bass is held, between the same root-position chord on both sides (the pedal 6/4)
 *   ARPEGGIATING  the bass leaps to the fifth of the same chord while the upper voices sustain it
 *
 * VOICES. The input has only the bass and soprano scale degrees, and the rules read four voices. The bass steps are
 * the bass degrees, each placed in the octave nearest the one before (so a motion is read as the smaller of its
 * two directions). The soprano's steps are placed the same way; the alto and the tenor are taken to hold the
 * soprano's step (they are not known), so "the upper voices sustain" means the soprano sustains. With no soprano
 * the upper voices are all taken to sustain. A 6/4 over a missing bass degree is unclassified.
 *
 * Each classified segment gets one Interpretation: target = the segment's Sonority id, FrameworkReference
 * standard-harmony, claim = a HarmonySixFourClaim, the segment's uncertainty.
 */

#define HARMONY_SIX_FOUR_CLAIM_TYPE "standard-harmony/six-four"

typedef struct {
    int type; /* SixFourType */
} HarmonySixFourClaim;

typedef struct {
    const char *segment_id;       /* borrowed from the input */
    bool is_six_four;             /* a triad in second inversion */
    bool classified;              /* matched a type; false: unclassified, no Interpretation */
    SixFourType type;
    Interpretation *interpretation; /* owned; NULL unless classified */
} HarmonySixFourResult;

typedef enum {
    HARMONY_SIX_FOUR_OK = 0,
    HARMONY_SIX_FOUR_ERR_NULL_ARGUMENT,
    HARMONY_SIX_FOUR_ERR_ALLOC
} HarmonySixFourStatus;

/* Marks the cadential 6/4s (the `cadential` flag of their chords) by the rule above. Called by
   harmony_analysis_prepare; idempotent. */
void harmony_analysis_mark_cadential(HarmonySegment *segments, size_t count);

/* One result per prepared segment, in order. On OK, *out is owned by the caller (harmony_six_four_results_destroy);
   otherwise untouched. */
HarmonySixFourStatus harmony_analysis_classify_six_fours(const HarmonySegment *segments, size_t count,
                                                         HarmonySixFourResult **out);

void harmony_six_four_results_destroy(HarmonySixFourResult *results, size_t count);

/* Reads the claim back out of a six-four Interpretation. False if it is not one. */
bool harmony_six_four_claim_read(const Interpretation *interpretation, HarmonySixFourClaim *out);

#endif
