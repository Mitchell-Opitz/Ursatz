#ifndef MUSIC_ANALYSIS_KEY_ESTIMATION_H
#define MUSIC_ANALYSIS_KEY_ESTIMATION_H

#include <stddef.h>

#include "analysis/analyzer.h"
#include "interpretation/uncertainty.h"
#include "structure/structure.h"
#include "theory/framework.h"

/*
 * KeyEstimation (Procedure -> Analyzer implementation, Registry #17):
 * estimates which key (tonic + mode) best fits an analyzed region under
 * a given Framework (theory/framework.h, L7 -- legal for L8 to depend
 * on) and reports the result as a single Interpretation backed by
 * Evidence and Uncertainty (TDD Sec242), per the Analyzer interface
 * (analysis/analyzer.h).
 *
 * Key estimation is framework-relative the same way chord
 * identification is (analysis/chord_identification.h): the same region
 * can be read in different keys depending on the theoretical framework
 * applying (e.g. a relative-minor reading vs. a modal-mixture reading).
 * Key itself belongs to L6 music-interpretation per the Registry (out
 * of scope for this L8 branch, deferred as a follow-up alongside
 * Chord) -- KeyEstimation does not construct a Key object; it only
 * reports a framework-relative Interpretation about the region.
 *
 * Interpretation (interpretation/interpretation.h) deliberately has NO
 * resolved framework field of its own (an open issue flagged in that
 * header) -- framework's influence is recorded in the backing
 * Evidence's source field (the framework's name), the same treatment
 * ChordIdentification uses.
 *
 * PitchIdentity carries no numeric semantics yet (no tuning system, no
 * PitchClass): KeyEstimation cannot derive tonic/mode from raw
 * PitchIdentity tokens. Candidate keys and their fit weights are
 * supplied already computed by whoever has the tuning-system and
 * framework knowledge to measure them (the same caller-supplies-the-
 * dimension pattern ScaleAnalysis and ChordIdentification use for
 * their candidates).
 *
 * region is a borrowed (not owned) Structure (structure/structure.h)
 * identifying the analyzed span by its own Entity id, the same target
 * pattern ScaleAnalysis uses (analysis/scale_analysis.h). framework is
 * a borrowed (not owned) Framework.
 */

typedef struct {
    const Structure *region;
    const Framework *framework;
    const UncertaintyCandidate *candidates;
    size_t candidate_count;
} KeyEstimationInput;

/*
 * Conforms to AnalyzerFn (analysis/analyzer.h): input MUST point to a
 * KeyEstimationInput with a non-NULL region, a non-NULL framework, and
 * at least one candidate; context and config are accepted per the
 * interface but unused (key estimation needs neither yet).
 *
 * Produces exactly one Interpretation and one backing Evidence: the
 * Interpretation's target is region's own Entity id, its claim is the
 * top-weighted candidate's label (copied bytes, null terminator
 * included), and its uncertainty is the full candidate set
 * (UNCERTAINTY_KIND_CANDIDATE_SET) exactly as supplied. The backing
 * Evidence's strength is the top candidate's weight fraction, its
 * source is framework's name; relevance/reliability have no dimension
 * to source them from yet and are fixed at 1/1 (the same
 * placeholder-fraction treatment ScaleAnalysis and ChordIdentification
 * use).
 */
AnalyzerResult key_estimation_analyze(const void *input, const Context *context, const Value *config,
                                       AnalysisResult **out);

/* The Analyzer value pluggable wherever an Analyzer is expected. */
extern const Analyzer KEY_ESTIMATION_ANALYZER;

#endif