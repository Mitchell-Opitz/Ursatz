#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_ANALYSIS_ANALYSIS_INPUT_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_ANALYSIS_ANALYSIS_INPUT_H

#include <stdbool.h>
#include <stddef.h>

#include "harmony_applied.h"
#include "harmony_chord.h"
#include "semantics/applied_chord_kind.h"
#include "semantics/beat_strength.h"
#include "time/metrical_position.h"
#include "time/time_span.h"

/*
 * The standard-harmony analysis adapter (L3): turns chord segments into this framework's inputs and returns
 * Interpretations. This file is the neutral input. standard-harmony does not include standard-tonality, so the
 * adapter defines the per-segment record here and the CALLER fills it from whatever produced the Roman numerals
 * (an integration test chains standard-tonality's output into it; framework sources never do).
 *
 * HarmonySegmentInput, one per chord segment, in time order:
 *
 *   segment_id       the segment's Sonority id (non-empty); the target of every Interpretation made for it
 *   span             the segment's TimeSpan
 *   position         the MetricalPosition of the segment's start
 *   beat_strength    the BeatStrength of that start
 *   degree           the chord's root as a scale degree, 1..7, in the key the chord is read in. For an applied
 *                    chord (applied_to != 0) it is the degree in the TEMPORARY key: 5 for a V or V7 of the target,
 *                    7 for a vii-diminished of it (what standard-tonality reports)
 *   intervals        the chord's quality as a root-relative interval set in semitones, ascending, starting at 0:
 *                    {0,4,7} major, {0,3,7} minor, {0,3,6} diminished, {0,4,8} augmented triads; {0,4,7,10}
 *                    dominant or major-minor, {0,4,7,11}, {0,3,7,10}, {0,3,6,10} half-diminished and {0,3,6,9}
 *                    fully diminished sevenths
 *   interval_count   3 for a triad, 4 for a seventh chord
 *   inversion        the index in `intervals` of the chord member in the bass: 0 root position, 1 first
 *                    inversion, 2 second, 3 third (sevenths only)
 *   applied_to       0 for a diatonic chord; otherwise the scale degree (1..7, in the home key) of the chord it
 *                    tonicizes
 *   bass_degree      the scale degree of the bass note in the home key, 1..7, or 0 when absent
 *   soprano_degree   the same for the soprano, or 0 when absent
 *   key_ref          the id of the key the chord is read in (non-empty); borrowed
 *   mode             the key's mode
 *   uncertainty      [0, 1], carried into every Interpretation made from the segment
 *
 * Every pointer is borrowed: the caller keeps the TimeSpan, MetricalPosition and strings alive for as long as the
 * prepared segments are used. The module reads no other framework and never finds a chord itself.
 *
 * A segment whose fields do not make a chord this framework reads (a degree outside 1..7, an inversion beyond the
 * chord, an interval set it has no name for, an applied chord whose target it cannot tonicize) is NOT an error: it
 * is passed through as not valid, and gets no Interpretation.
 */

#define HARMONY_SEGMENT_MAX_INTERVALS 4

typedef struct {
    const char *segment_id;
    const TimeSpan *span;
    const MetricalPosition *position;
    BeatStrength beat_strength;
    int degree;
    int intervals[HARMONY_SEGMENT_MAX_INTERVALS];
    size_t interval_count;
    int inversion;
    int applied_to;
    int bass_degree;
    int soprano_degree;
    const char *key_ref;
    HarmonyMode mode;
    double uncertainty;
} HarmonySegmentInput;


/*
 * A prepared segment: the input read as the plain data the existing rules take. `valid` is false for a segment
 * that is not a chord this framework reads (see above); nothing else in the record is meaningful then.
 *
 *   chord          the HarmonyChord (harmony_chord.h) of a DIATONIC segment: the degree, the inversion, whether it
 *                  is a seventh chord, the leading tone of a minor-key vii (a diminished chord on degree 7), and
 *                  half-diminished. `cadential` is false here: it is set by the six-four step, not by the caller
 *   applied        true for an applied chord (applied_to != 0); applied_chord is then its HarmonyAppliedChord
 *                  (kind, quality, target), and `chord` is not meaningful
 */
typedef struct {
    const HarmonySegmentInput *input;
    bool valid;
    HarmonyChord chord;
    bool applied;
    HarmonyAppliedChord applied_chord;
} HarmonySegment;

typedef enum {
    HARMONY_ANALYSIS_OK = 0,
    HARMONY_ANALYSIS_ERR_NULL_ARGUMENT,
    HARMONY_ANALYSIS_ERR_BAD_INPUT, /* a missing segment id, key ref, span or position, or a bad uncertainty */
    HARMONY_ANALYSIS_ERR_ALLOC
} HarmonyAnalysisStatus;

/*
 * Reads `count` inputs into prepared segments, in order. On OK, *out is owned by the caller
 * (harmony_analysis_destroy) and holds `count` segments, each pointing at its input, which must outlive it;
 * otherwise *out is untouched. `count` may be 0.
 */
HarmonyAnalysisStatus harmony_analysis_prepare(const HarmonySegmentInput *inputs, size_t count,
                                               HarmonySegment **out);

void harmony_analysis_destroy(HarmonySegment *segments);

#endif
