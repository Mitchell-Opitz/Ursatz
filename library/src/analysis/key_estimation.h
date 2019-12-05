#ifndef MUSIC_ANALYSIS_KEY_ESTIMATION_H
#define MUSIC_ANALYSIS_KEY_ESTIMATION_H

#include <stddef.h>

#include "analysis/analyzer.h"
#include "structure/structure.h"
#include "theory/framework.h"

/*
 * KeyEstimation (Procedure -> Analyzer implementation, Registry #17):
 * estimates which key (tonic + mode) best fits an analyzed region under
 * a given Framework (theory/framework.h, L7 -- legal for L8 to depend
 * on) and reports the result as a single Interpretation backed by
 * Evidence (TDD Sec242), per the Analyzer interface (analysis/
 * analyzer.h).
 *
 * Key estimation is framework-relative the same way chord
 * identification is (analysis/chord_identification.h). KeyEstimation is
 * now wired to a real Theory Framework instance --
 * frameworks/common-practice-minimal/ -- so it produces a real,
 * populated Key (interpretation/key.h, L6) rather than a bare
 * framework-agnostic claim. As with ChordIdentification, the Key is
 * built and then unwrapped via key_take_interpretation
 * (interpretation/key.h) into the Interpretation the AnalysisResult
 * carries (AnalysisResult only ever holds bare Interpretation/Evidence
 * objects, never a Key wrapper).
 *
 * This pass is deliberately single-answer and deterministic, matching
 * common-practice-minimal-v1's own key_identification.h contract: no
 * multi-candidate/confidence-weighted Uncertainty is produced here (a
 * later, separate refinement). When the region's pitch content matches
 * neither major nor natural minor, this analyzer reports
 * ANALYZER_ERR_ANALYSIS_FAILED rather than forcing a guess.
 *
 * PitchIdentity carries no numeric semantics yet (no tuning system, no
 * PitchClass): KeyEstimation cannot derive tonic/mode from raw
 * PitchIdentity tokens itself. The caller supplies the candidate tonic's
 * name and each note's interval in semitones (0-11) from that tonic,
 * already measured by whoever has the tuning-system knowledge to do so
 * -- the same caller-supplies-the-dimension pattern
 * key_identification.h documents. KeyEstimation does not itself search
 * over candidate tonics: proposing which tonic to test is a separate,
 * out-of-scope concern (the same restraint ChordIdentification applies
 * to inversions).
 *
 * region is a borrowed (not owned) Structure (structure/structure.h)
 * identifying the analyzed span by its own Entity id, the same target
 * pattern ScaleAnalysis uses (analysis/scale_analysis.h). framework is
 * a borrowed (not owned) Framework. tonic_name and
 * tonic_relative_pitch_classes are borrowed for the duration of the
 * call only.
 */

typedef struct {
    const Structure *region;
    const Framework *framework;
    const char *tonic_name;
    const int *tonic_relative_pitch_classes;
    size_t pitch_class_count;
} KeyEstimationInput;

/*
 * Conforms to AnalyzerFn (analysis/analyzer.h): input MUST point to a
 * KeyEstimationInput with a non-NULL region, a non-NULL framework, a
 * non-empty tonic_name, and at least one pitch class; context and
 * config are accepted per the interface but unused (key estimation
 * needs neither yet).
 *
 * On a match, produces exactly one Interpretation and one backing
 * Evidence: the Interpretation's target is region's own Entity id, its
 * claim is the matched key's label (e.g. "C major"), its
 * framework_reference names framework. The backing Evidence's strength
 * is 1/1 (this pass's matching is a hard yes/no, not a weighted fit);
 * relevance/reliability have no dimension to source them from yet and
 * are fixed at 1/1 (the same placeholder-fraction treatment
 * ScaleAnalysis and ChordIdentification use); source is framework's
 * name.
 *
 * Returns ANALYZER_ERR_ANALYSIS_FAILED (leaving *out untouched) when
 * tonic_relative_pitch_classes matches neither major nor natural minor
 * under common-practice-minimal-v1 -- this analyzer does not force a
 * bad match.
 */
AnalyzerResult key_estimation_analyze(const void *input, const Context *context, const Value *config,
                                       AnalysisResult **out);

extern const Analyzer KEY_ESTIMATION_ANALYZER;

#endif
