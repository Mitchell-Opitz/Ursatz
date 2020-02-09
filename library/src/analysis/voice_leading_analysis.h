#ifndef MUSIC_ANALYSIS_VOICE_LEADING_ANALYSIS_H
#define MUSIC_ANALYSIS_VOICE_LEADING_ANALYSIS_H

#include <stdbool.h>
#include <stddef.h>

#include "analysis/analyzer.h"
#include "events/note_event.h"

/*
 * VoiceLeadingAnalysis (Procedure -> Analyzer implementation, Registry
 * #18): classifies the relationship between two voices' motion from
 * one Sonority to the next (parallel/similar/contrary/oblique, and
 * whether the arrival interval is perfect) and reports each pair as an
 * Interpretation backed by Evidence and Uncertainty (TDD Sec242), per
 * the Analyzer interface (analysis/analyzer.h).
 *
 * This only OBSERVES and reports the motion -- it does not judge
 * whether a pair is a rule violation (e.g. forbidden parallel fifths).
 * That judgment belongs to Rule/Constraint (music-rules, L7) consuming
 * this Interpretation, or to a species-counterpoint extension module
 * (Phase 10/11) -- both explicitly out of scope for this L8 branch.
 *
 * PitchIdentity carries no numeric semantics yet (no tuning system, no
 * PitchClass): VoiceLeadingAnalysis cannot classify motion or interval
 * quality from raw PitchIdentity tokens itself. Each
 * VoiceLeadingAnalysisPair therefore carries its motion classification
 * and interval-perfection flag already determined (the same
 * caller-supplies-the-dimension pattern IntervalAnalysis uses for its
 * directed_semitones, analysis/interval_analysis.h) -- this analyzer's
 * job is to package that classification into the Interpretation/
 * Evidence/Uncertainty architecture, not to compute it.
 *
 * Unlike ScaleAnalysis/ChordIdentification/KeyEstimation, this
 * classification is exact given the supplied inputs, not ambiguous:
 * its Uncertainty is a scalar confidence of 1/1, the same treatment
 * IntervalAnalysis uses, not a candidate set.
 *
 * voice_a_to/voice_b_to are borrowed references (not owned) into the
 * caller's NoteEvent sequence -- the two voices' arrival events at the
 * "to" Sonority. The Interpretation for a pair targets voice_a_to (by
 * id reference): Interpretation's target_id is a single reference, and
 * a voice-leading pair is inherently about two events, so one is
 * chosen as the anchor -- the same single-target limitation
 * ChordIdentification and KeyEstimation document.
 */

typedef enum {
    VOICE_LEADING_MOTION_PARALLEL,
    VOICE_LEADING_MOTION_SIMILAR,
    VOICE_LEADING_MOTION_CONTRARY,
    VOICE_LEADING_MOTION_OBLIQUE
} VoiceLeadingMotion;

/*
 * Bass motion between two consecutive sonorities' lowest-sounding voice.
 * Like VoiceLeadingMotion, this is a caller-supplied dimension: PitchIdentity
 * carries no numeric semantics, so this analyzer cannot itself determine
 * whether the bass moved by step, by leap, or stayed on the same pitch.
 */
typedef enum {
    VOICE_LEADING_BASS_MOTION_STEP,
    VOICE_LEADING_BASS_MOTION_LEAP,
    VOICE_LEADING_BASS_MOTION_SAME
} VoiceLeadingBassMotion;

typedef struct {
    const NoteEvent *voice_a_to;
    const NoteEvent *voice_b_to;
    VoiceLeadingMotion motion;
    bool resulting_interval_is_perfect;
    VoiceLeadingBassMotion bass_motion;
} VoiceLeadingAnalysisPair;

typedef struct {
    const VoiceLeadingAnalysisPair *pairs;
    size_t pair_count;
} VoiceLeadingAnalysisInput;

/* The claim packaged into each Interpretation -- a plain copy of what the pair already stated. */
typedef struct {
    VoiceLeadingMotion motion;
    bool resulting_interval_is_perfect;
    VoiceLeadingBassMotion bass_motion;
} VoiceLeadingClaim;

/*
 * Conforms to AnalyzerFn (analysis/analyzer.h): input MUST point to a
 * VoiceLeadingAnalysisInput; context and config are accepted per the
 * interface but unused (voice-leading classification needs neither
 * yet). Every pair's voice_a_to/voice_b_to MUST be non-NULL.
 *
 * Produces exactly one Interpretation and one backing Evidence per
 * pair, in input order: interpretation N's target is pair N's
 * voice_a_to NoteEvent (by id reference), its claim is a
 * VoiceLeadingClaim copy of pair N's motion/resulting_interval_is_
 * perfect/bass_motion, and its uncertainty is a scalar confidence of 1/1 (the
 * classification was supplied already determined, not estimated).
 * pair_count == 0 is valid and yields an empty AnalysisResult.
 */
AnalyzerResult voice_leading_analysis_analyze(const void *input, const Context *context, const Value *config,
                                               AnalysisResult **out);

extern const Analyzer VOICE_LEADING_ANALYSIS_ANALYZER;

#endif