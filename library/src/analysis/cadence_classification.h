#ifndef MUSIC_ANALYSIS_CADENCE_CLASSIFICATION_H
#define MUSIC_ANALYSIS_CADENCE_CLASSIFICATION_H

/*
 * Not the cadence detection owner (harmony_analysis_find_cadences). Kept as packaging of caller
 * candidates into Cadence shells (docs/ARCHITECTURE.md, Analytic job owners).
 */

#include <stdbool.h>

#include "analysis/analyzer.h"
#include "analysis/cadence_detection.h"
#include "analysis/voice_leading_analysis.h"
#include "structure/structure.h"
#include "theory/framework.h"

/*
 * CadenceClassification (Procedure -> Analyzer implementation, built on
 * CadenceDetection, analysis/cadence_detection.h): classifies a chord
 * progression's cadence TYPE (PAC, IAC, HC, Plagal, Deceptive) from
 * real, already-computed chord root motion, and reports the result the
 * same way CadenceDetection does -- a single generic Interpretation
 * targeting an existing L5 Cadence shell Structure, backed by Evidence
 * and a candidate-set Uncertainty. This module does not invent a new
 * output type: it is a candidate-BUILDER that packages chord-motion
 * rules into the UncertaintyCandidate set CadenceDetection already
 * knows how to turn into an Interpretation (it calls
 * cadence_detection_analyze directly). Each candidate is a CadenceType
 * (semantics/cadence_type.h); its label, which becomes the Interpretation claim,
 * comes from cadence_type_label.
 *
 * Root-motion signal: ChordIdentification (analysis/
 * subset_chord_analyzer.h) reports a chord's Roman-numeral/quality LABEL
 * as an opaque string -- it does not expose the chord's scale-degree
 * root as a separate queryable field, and a seventh-chord's label
 * ("dominant7") carries no root information at all (seventh_
 * identification.h matches by root-relative interval content, not
 * scale degree). Recovering root motion by parsing chord label strings
 * would be fragile and would silently fail for seventh chords. Instead,
 * following the same caller-supplies-the-dimension pattern
 * ChordIdentification/KeyEstimation/VoiceLeadingAnalysis all use, this
 * analyzer takes each chord's root scale degree (1-7, relative to the
 * already-declared key) directly -- the same integer the caller already
 * had to supply to ChordIdentification's own scale_degrees parameter to
 * identify that chord in the first place. This is real, already-
 * computed data, not a guess.
 *
 * Classification rules (root motion from_degree -> to_degree, 1 = tonic,
 * 4 = subdominant, 5 = dominant, 6 = submediant):
 *   - to_degree == 1 && from_degree == 5: authentic-cadence candidate.
 *     PAC vs. IAC genuinely requires soprano-resolution data this
 *     codebase does not compute anywhere yet (no PitchClass/
 *     TuningSystem, per docs/modules/l8-notes.md) -- has_soprano_
 *     resolves_to_tonic is an OPTIONAL caller-supplied refinement, the
 *     same optional-refinement pattern VoiceLeadingAnalysis's bass_
 *     motion uses. When absent, the candidate is reported as
 *     CADENCE_TYPE_AUTHENTIC_UNSPECIFIED rather than guessing.
 *   - to_degree == 1 && from_degree == 4: plagal-cadence candidate.
 *   - to_degree == 5 (and the authentic rule above did not already
 *     match): half-cadence candidate.
 *   - from_degree == 5 && to_degree == 6: deceptive-cadence candidate.
 *   - anything else: a single CADENCE_TYPE_UNKNOWN candidate, weight 1/1 --
 *     no false-confidence guess.
 *
 * from_is_dominant_seventh and has_bass_motion/bass_motion are further
 * OPTIONAL caller-supplied refinements (never required) that only
 * adjust the top candidate's confidence weight within the authentic-
 * cadence rule, matching the branch's "bass-motion data may be used if
 * supplied, but must not be required" scope: a caller with no Voice
 * Leading data still gets a classification, just a lower-confidence
 * one.
 *
 * region, framework are borrowed (not owned), passed straight through
 * to cadence_detection_analyze -- see analysis/cadence_detection.h for
 * their contract.
 */

typedef struct {
    const Structure *region;
    const Framework *framework;

    int from_chord_root_degree; /* 1-7, required */
    int to_chord_root_degree;   /* 1-7, required */

    bool from_is_dominant_seventh; /* optional refinement, default false */

    bool has_soprano_resolves_to_tonic; /* optional refinement */
    bool soprano_resolves_to_tonic;     /* valid only when has_soprano_resolves_to_tonic */

    bool has_bass_motion;             /* optional refinement */
    VoiceLeadingBassMotion bass_motion; /* valid only when has_bass_motion */
} CadenceClassificationInput;

/*
 * Conforms to AnalyzerFn (analysis/analyzer.h): input MUST point to a
 * CadenceClassificationInput with a non-NULL region and from/to root
 * degrees each in [1, 7]; context and config are accepted per the
 * interface but unused. Delegates entirely to cadence_detection_analyze
 * for the actual Interpretation/Evidence/Uncertainty construction, so
 * its output shape matches CadenceDetection's exactly.
 */
AnalyzerResult cadence_classification_analyze(const void *input, const Context *context, const Value *config,
                                               AnalysisResult **out);

extern const Analyzer CADENCE_CLASSIFICATION_ANALYZER;

#endif
