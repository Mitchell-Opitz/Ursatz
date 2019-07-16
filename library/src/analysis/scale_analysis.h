#ifndef MUSIC_ANALYSIS_SCALE_ANALYSIS_H
#define MUSIC_ANALYSIS_SCALE_ANALYSIS_H

#include <stddef.h>

#include "analysis/analyzer.h"
#include "interpretation/uncertainty.h"
#include "structure/structure.h"

/*
 * ScaleAnalysis (Procedure -> Analyzer implementation, Registry #15):
 * identifies which scale(s)/mode(s) best fit an analyzed region and
 * reports the result as a single Interpretation backed by Evidence and
 * Uncertainty (TDD Sec242), per the Analyzer interface (analysis/
 * analyzer.h).
 *
 * PitchIdentity carries no numeric semantics yet (pitch/pitch_identity.h
 * -- no tuning system, no PitchClass): ScaleAnalysis cannot derive
 * pitch-class content, let alone scale fit, from raw PitchIdentity
 * tokens. Candidate scales and their fit weights are therefore supplied
 * already computed by whoever has the tuning-system knowledge to
 * measure them (the same caller-supplies-the-dimension pattern
 * IntervalAnalysis uses for its directed_semitones, analysis/
 * interval_analysis.h) -- ScaleAnalysis's job is to package that
 * candidate set into the Interpretation/Evidence/Uncertainty
 * architecture, not to compute pitch-class membership itself.
 *
 * Scale identification is classically ambiguous (e.g. C major and A
 * natural minor share the same pitch classes): candidates is therefore
 * wrapped as an UNCERTAINTY_KIND_CANDIDATE_SET Uncertainty
 * (interpretation/uncertainty.h) rather than collapsed to a single
 * scalar confidence, the same architecture IntervalAnalysis's scalar
 * confidence output would be wrong to reuse here.
 *
 * region is a borrowed (not owned) Structure (structure/structure.h)
 * identifying the analyzed span by its own Entity id -- ScaleAnalysis
 * does not inspect region's event references itself (TDD Sec152: it
 * would only be able to reference them further, never resolve or
 * duplicate them, and has no pitch-class data to derive a fit from even
 * if it did).
 */

typedef struct {
    const Structure *region;
    const UncertaintyCandidate *candidates;
    size_t candidate_count;
} ScaleAnalysisInput;

/*
 * Conforms to AnalyzerFn (analysis/analyzer.h): input MUST point to a
 * ScaleAnalysisInput with a non-NULL region and at least one candidate;
 * context and config are accepted per the interface but unused (scale
 * matching needs neither yet).
 *
 * Produces exactly one Interpretation and one backing Evidence: the
 * Interpretation's target is region's own Entity id, its claim is the
 * top-weighted candidate's label (copied bytes, non-null-terminated
 * length included), and its uncertainty is the full candidate set
 * (UNCERTAINTY_KIND_CANDIDATE_SET) exactly as supplied. The backing
 * Evidence's strength is the top candidate's weight fraction;
 * relevance/reliability have no dimension to source them from yet and
 * are fixed at 1/1 (the same placeholder-fraction treatment used
 * elsewhere pending a real basis).
 */
AnalyzerResult scale_analysis_analyze(const void *input, const Context *context, const Value *config,
                                       AnalysisResult **out);

extern const Analyzer SCALE_ANALYSIS_ANALYZER;

#endif