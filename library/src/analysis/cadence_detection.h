#ifndef MUSIC_ANALYSIS_CADENCE_DETECTION_H
#define MUSIC_ANALYSIS_CADENCE_DETECTION_H

#include <stddef.h>

#include "analysis/analyzer.h"
#include "interpretation/uncertainty.h"
#include "structure/structure.h"
#include "theory/framework.h"

/*
 * CadenceDetection (Procedure -> Analyzer implementation, Registry
 * #19): identifies which cadence type(s) best fit an analyzed region
 * and reports the result as a single Interpretation backed by Evidence
 * and Uncertainty (TDD Sec242), per the Analyzer interface (analysis/
 * analyzer.h).
 *
 * structure/cadence.h explicitly defers this: "Cadence DETECTION
 * (recognizing that a given region of events forms a cadence) is Phase
 * 8 (music-analysis) work and is out of scope here -- nothing in this
 * file analyzes harmony or infers cadence type." CadenceDetection is
 * that Phase 8 work -- but it reports its finding as an Interpretation
 * ABOUT the region, the same way ScaleAnalysis/ChordIdentification/
 * KeyEstimation do (analysis/scale_analysis.h, analysis/
 * chord_identification.h, analysis/key_estimation.h); it does not
 * itself construct a Cadence (structure/cadence.h). Whoever consumes
 * this Interpretation's claim decides whether and how to call
 * cadence_create with region's own event refs -- CadenceDetection has
 * no way to know which of region's events are the cadence's "defining
 * events" (cadence_create's event_ids) versus incidental ones, only
 * whether/what type of cadence the region as a whole reads as.
 *
 * PitchIdentity carries no numeric semantics yet (no tuning system, no
 * PitchClass): CadenceDetection cannot derive harmonic motion, let
 * alone cadence type, from raw PitchIdentity tokens. Candidate cadence
 * types and their fit weights are supplied already computed by whoever
 * has the tuning-system knowledge to measure them (the same
 * caller-supplies-the-dimension pattern ScaleAnalysis uses for its
 * candidates).
 *
 * Cadence type identification is ambiguous (e.g. a half cadence vs. a
 * deceptive cadence can share a similar melodic shape): candidates is
 * therefore wrapped as an UNCERTAINTY_KIND_CANDIDATE_SET Uncertainty,
 * the same architecture ScaleAnalysis/ChordIdentification/KeyEstimation
 * use.
 *
 * region is a borrowed (not owned) Structure identifying the analyzed
 * span by its own Entity id, the same target pattern ScaleAnalysis
 * uses.
 *
 * framework is an OPTIONAL borrowed (not owned) Framework: when
 * supplied, the resulting Interpretation's framework_reference names it
 * (the same entity_id_value(framework_id(framework)) ->
 * framework_reference_create pattern chord_vocabulary.c/
 * key_vocabulary.c use), signaling that the candidate labels are
 * relative to that Framework's vocabulary (e.g. common-practice-
 * minimal's cadence types). When NULL, the Interpretation's
 * framework_reference stays unset, the existing framework-neutral
 * behavior.
 */

typedef struct {
    const Structure *region;
    const UncertaintyCandidate *candidates;
    size_t candidate_count;
    const Framework *framework;
} CadenceDetectionInput;

/*
 * Conforms to AnalyzerFn (analysis/analyzer.h): input MUST point to a
 * CadenceDetectionInput with a non-NULL region and at least one
 * candidate; context and config are accepted per the interface but
 * unused (cadence-type matching needs neither yet).
 *
 * Produces exactly one Interpretation and one backing Evidence: the
 * Interpretation's target is region's own Entity id, its claim is the
 * top-weighted candidate's label (copied bytes, null terminator
 * included), and its uncertainty is the full candidate set
 * (UNCERTAINTY_KIND_CANDIDATE_SET) exactly as supplied. The backing
 * Evidence's strength is the top candidate's weight fraction;
 * relevance/reliability have no dimension to source them from yet and
 * are fixed at 1/1 (the same placeholder-fraction treatment
 * ScaleAnalysis uses).
 */
AnalyzerResult cadence_detection_analyze(const void *input, const Context *context, const Value *config,
                                          AnalysisResult **out);

extern const Analyzer CADENCE_DETECTION_ANALYZER;

#endif