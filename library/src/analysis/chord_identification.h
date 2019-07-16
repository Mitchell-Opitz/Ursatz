#ifndef MUSIC_ANALYSIS_CHORD_IDENTIFICATION_H
#define MUSIC_ANALYSIS_CHORD_IDENTIFICATION_H

#include <stddef.h>

#include "analysis/analyzer.h"
#include "interpretation/uncertainty.h"
#include "semantics/sonority.h"
#include "theory/framework.h"

/*
 * ChordIdentification (Procedure -> Analyzer implementation, Registry
 * #16): identifies which chord label(s) best fit a Sonority
 * (semantics/sonority.h) under a given Framework (theory/framework.h,
 * L7 -- legal for L8 to depend on) and reports the result as a single
 * Interpretation backed by Evidence and Uncertainty (TDD Sec242), per
 * the Analyzer interface (analysis/analyzer.h).
 *
 * Chord identification is framework-relative: the same Sonority can be
 * labeled differently depending on the theoretical framework applying
 * (e.g. a tertian-harmony reading vs. a quartal-harmony reading of the
 * same pitch set). sonority.h explicitly defers this ("A Sonority is
 * NOT automatically a Chord... that is Chord, Phase 8, depends on
 * Framework/L7, not built here") -- ChordIdentification is that next
 * step, but it still does not construct a Chord object (Chord itself
 * belongs to L6 music-interpretation per the Registry, out of scope for
 * this L8 branch; see the branch's own follow-up note). It only reports
 * a framework-relative Interpretation about the Sonority.
 *
 * Interpretation (interpretation/interpretation.h) deliberately has NO
 * resolved framework field of its own (an open issue flagged in that
 * header). ChordIdentification therefore does not attempt to attach
 * framework as an Interpretation field either -- framework's influence
 * is recorded in the backing Evidence's source field (the framework's
 * name) instead, consistent with that documented gap.
 *
 * PitchIdentity carries no numeric semantics yet (no tuning system, no
 * PitchClass): ChordIdentification cannot derive root/quality/inversion
 * from raw PitchIdentity tokens. Candidate chord labels and their fit
 * weights are supplied already computed by whoever has the
 * tuning-system and framework knowledge to measure them (the same
 * caller-supplies-the-dimension pattern ScaleAnalysis uses for its
 * candidates, analysis/scale_analysis.h).
 *
 * Interpretation's target_id is a single reference, but a Sonority is a
 * set of simultaneous events with no identity of its own (semantics/
 * sonority.h documents it as a Value): ChordIdentification anchors its
 * Interpretation to sonority's first referenced event id
 * (sonority_event_id_at(sonority, 0)) as a representative target,
 * rather than inventing a new identity concept Sonority does not have.
 *
 * sonority and framework are borrowed (not owned) references.
 */

typedef struct {
    const Sonority *sonority;
    const Framework *framework;
    const UncertaintyCandidate *candidates;
    size_t candidate_count;
} ChordIdentificationInput;

/*
 * Conforms to AnalyzerFn (analysis/analyzer.h): input MUST point to a
 * ChordIdentificationInput with a non-NULL sonority, a non-NULL
 * framework, and at least one candidate; context and config are
 * accepted per the interface but unused (chord matching needs neither
 * yet).
 *
 * Produces exactly one Interpretation and one backing Evidence: the
 * Interpretation's target is sonority's first event id, its claim is
 * the top-weighted candidate's label (copied bytes, null terminator
 * included), and its uncertainty is the full candidate set
 * (UNCERTAINTY_KIND_CANDIDATE_SET) exactly as supplied. The backing
 * Evidence's strength is the top candidate's weight fraction, its
 * source is framework's name; relevance/reliability have no dimension
 * to source them from yet and are fixed at 1/1 (the same
 * placeholder-fraction treatment ScaleAnalysis uses).
 */
AnalyzerResult chord_identification_analyze(const void *input, const Context *context, const Value *config,
                                             AnalysisResult **out);

extern const Analyzer CHORD_IDENTIFICATION_ANALYZER;

#endif