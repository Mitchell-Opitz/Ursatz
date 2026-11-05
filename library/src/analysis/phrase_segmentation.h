#ifndef MUSIC_ANALYSIS_PHRASE_SEGMENTATION_H
#define MUSIC_ANALYSIS_PHRASE_SEGMENTATION_H

/*
 * Not the phrase segmentation owner (standard_form_segment_phrases). Kept as an internal step of the
 * kernel boundary detector (docs/analytic-job-owners.md).
 */

#include <stdbool.h>
#include <stddef.h>

#include "analysis/analyzer.h"
#include "events/note_event.h"

/*
 * PhraseSegmentation (Procedure -> Analyzer implementation, Registry
 * #21): classifies whether a candidate point is a phrase boundary and
 * reports each as an Interpretation backed by Evidence and Uncertainty
 * (TDD Sec242), per the Analyzer interface (analysis/analyzer.h).
 *
 * Phrase itself is an explicitly deferred Structure subtype
 * (structure/structure.h lists Figure, Phrase, Theme, Section,
 * Movement, Work as "later/deferred", not built in this pass): unlike
 * MotifDetection (whose target, MotifOccurrence, already exists at L5),
 * PhraseSegmentation cannot construct a Phrase object -- there is
 * nothing yet to construct. It only reports, per candidate point,
 * whether that point reads as a phrase boundary; a caller with a real
 * Phrase type builds it from the boundary points this analyzer marks
 * true, the same deferred-construction division CadenceDetection uses
 * for Cadence (analysis/cadence_detection.h).
 *
 * PitchIdentity carries no numeric semantics yet (no tuning system, no
 * PitchClass): PhraseSegmentation cannot derive breath points or
 * cadential closure from raw PitchIdentity tokens. Each point's
 * is_boundary classification and confidence are supplied already
 * determined (the same caller-supplies-the-dimension pattern
 * VoiceLeadingAnalysis uses for its motion classification, analysis/
 * voice_leading_analysis.h) -- this analyzer's job is to package that
 * classification into the Interpretation/Evidence/Uncertainty
 * architecture, not to compute it.
 *
 * Like VoiceLeadingAnalysis, this classification is exact given the
 * supplied inputs, not ambiguous: its Uncertainty is a scalar
 * confidence, not a candidate set.
 *
 * event is a borrowed reference (not owned) into the caller's NoteEvent
 * sequence.
 */

typedef struct {
    const NoteEvent *event;
    bool is_boundary;
    unsigned long confidence_numerator;
    unsigned long confidence_denominator;
} PhraseSegmentationPoint;

typedef struct {
    const PhraseSegmentationPoint *points;
    size_t point_count;
} PhraseSegmentationInput;

/* The claim packaged into each Interpretation -- a plain copy of what the point already stated. */
typedef struct {
    bool is_boundary;
} PhraseSegmentationClaim;

/*
 * Conforms to AnalyzerFn (analysis/analyzer.h): input MUST point to a
 * PhraseSegmentationInput; context and config are accepted per the
 * interface but unused (boundary classification needs neither yet).
 * Every point's event MUST be non-NULL, with a positive confidence_
 * denominator and confidence_numerator <= confidence_denominator.
 *
 * Produces exactly one Interpretation and one backing Evidence per
 * point, in input order: interpretation N's target is point N's event
 * (by id reference), its claim is a PhraseSegmentationClaim copy of
 * point N's is_boundary, and its uncertainty is point N's confidence as
 * a scalar confidence. point_count == 0 is valid and yields an empty
 * AnalysisResult.
 */
AnalyzerResult phrase_segmentation_analyze(const void *input, const Context *context, const Value *config,
                                            AnalysisResult **out);

extern const Analyzer PHRASE_SEGMENTATION_ANALYZER;

#endif