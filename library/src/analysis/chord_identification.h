#ifndef MUSIC_ANALYSIS_CHORD_IDENTIFICATION_H
#define MUSIC_ANALYSIS_CHORD_IDENTIFICATION_H

/*
 * Not the chord identification owner (common_practice_identify_weighted_sequence and
 * common_practice_identify_weighted). Kept only for the subset coverage reports (known duplicate-id
 * gap); the analyzer moves into the chain in P22 (docs/ARCHITECTURE.md, Analytic job owners).
 */

#include <stddef.h>

#include "analysis/analyzer.h"
#include "semantics/sonority.h"
#include "theory/framework.h"

/*
 * ChordIdentification (Procedure -> Analyzer implementation, Registry
 * #16): identifies which chord label best fits a Sonority
 * (semantics/sonority.h) under a given Framework (theory/framework.h,
 * L7 -- legal for L8 to depend on) and reports the result as a single
 * Interpretation backed by Evidence (TDD Sec242), per the Analyzer
 * interface (analysis/analyzer.h).
 *
 * Chord identification is framework-relative: the same Sonority can be
 * labeled differently depending on the theoretical framework applying
 * (e.g. a tertian-harmony reading vs. a quartal-harmony reading of the
 * same pitch set). ChordIdentification is now wired to a real Theory
 * Framework instance -- frameworks/common-practice-minimal/ (L8's first
 * legal dependency on a Framework extension module, TDD Dependency &
 * Layer Spec v2 SS2-3) -- so it produces a real, populated Chord
 * (interpretation/chord.h, L6) rather than a bare framework-agnostic
 * claim. It does not embed the Chord object itself in the
 * AnalysisResult it returns (AnalysisResult, analysis/analyzer.h, only
 * ever holds bare Interpretation/Evidence objects): the Chord is built,
 * then unwrapped via chord_take_interpretation (interpretation/chord.h)
 * into the Interpretation AnalysisResult carries.
 *
 * This pass is deliberately single-answer and deterministic, matching
 * common-practice-minimal-v1's own triad_identification.h contract: no
 * multi-candidate/confidence-weighted Uncertainty is produced here (that
 * refinement is planned separately, once basic identification is proven
 * working end-to-end). When the Sonority's scale degrees do not match
 * any of the framework's seven diatonic triads, this analyzer reports
 * ANALYZER_ERR_ANALYSIS_FAILED rather than forcing a guess.
 *
 * PitchIdentity carries no numeric semantics yet (no tuning system, no
 * PitchClass): ChordIdentification cannot derive scale degrees from raw
 * PitchIdentity tokens itself. The caller supplies each note's diatonic
 * scale degree (1-7, relative to the already-declared key), already
 * measured by whoever has the tuning-system and key knowledge to do so
 * -- the same caller-supplies-the-dimension pattern
 * triad_identification.h documents.
 *
 * Seventh-chord quality (frameworks/common-practice-minimal/
 * seventh_identification.h) cannot be resolved from scale degree alone,
 * so root_relative_intervals is a second, independent caller-supplied
 * dimension: each chord tone's semitone interval (0-11) relative to the
 * chord's own candidate root. When scale_degrees matches one of the
 * seven diatonic triads, that match wins; root_relative_intervals is
 * tried only when scale_degrees is absent or does not match a triad. At
 * least one of the two dimensions must be supplied.
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
    const int *scale_degrees;
    size_t scale_degree_count;
    const int *root_relative_intervals;
    size_t interval_count;
} ChordIdentificationInput;

/*
 * Conforms to AnalyzerFn (analysis/analyzer.h): input MUST point to a
 * ChordIdentificationInput with a non-NULL sonority, a non-NULL
 * framework, and at least one of (scale_degrees, scale_degree_count) or
 * (root_relative_intervals, interval_count) populated; context and
 * config are accepted per the interface but unused (chord matching
 * needs neither yet).
 *
 * On a match, produces exactly one Interpretation and one backing
 * Evidence: the Interpretation's target is sonority's first event id,
 * its claim is the matched triad's or seventh-chord quality's label,
 * its framework_reference names framework. The backing Evidence's
 * strength is 1/1 (this pass's matching is a hard yes/no, not a
 * weighted fit); relevance/reliability have no dimension to source them
 * from yet and are fixed at 1/1 (the same placeholder-fraction
 * treatment ScaleAnalysis uses); source is framework's name.
 *
 * Returns ANALYZER_ERR_ANALYSIS_FAILED (leaving *out untouched) when
 * scale_degrees does not match any of common-practice-minimal-v1's
 * seven diatonic triads AND root_relative_intervals (if supplied) does
 * not match any of its four diatonic seventh-chord qualities -- this
 * analyzer does not force a bad match.
 */
AnalyzerResult chord_identification_analyze(const void *input, const Context *context, const Value *config,
                                             AnalysisResult **out);

extern const Analyzer CHORD_IDENTIFICATION_ANALYZER;

#endif
