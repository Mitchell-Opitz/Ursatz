#ifndef MUSIC_ANALYSIS_INTERVAL_ANALYSIS_H
#define MUSIC_ANALYSIS_INTERVAL_ANALYSIS_H

#include <stddef.h>

#include "analysis/analyzer.h"
#include "events/note_event.h"

/*
 * IntervalAnalysis (Procedure -> Analyzer implementation, Registry #14):
 * measures the interval between consecutive NoteEvents in a monophonic
 * line and reports each as an Interpretation backed by Evidence and
 * Uncertainty (TDD Sec242), per the Analyzer interface (analysis/
 * analyzer.h).
 *
 * PitchIdentity carries no numeric semantics yet (pitch/pitch_identity.h
 * -- no tuning system, no PitchClass): neither IntervalAnalysis nor
 * semantics/interval.h's Interval can derive a semitone displacement
 * from two PitchIdentity tokens by itself. Each IntervalAnalysisPair
 * therefore carries its own already-measured directed_semitones
 * (supplied by whoever has the tuning-system knowledge to measure it --
 * the same caller-supplies-the-dimension pattern Transposition uses for
 * its semitones parameter, transform/transposition.h). IntervalAnalysis
 * builds a real Interval (semantics/interval.h) from pitch_a/pitch_b
 * plus that displacement -- both to reuse the established interval
 * representation and for its NULL-pitch/consistency checks -- then
 * packages the result into the Interpretation/Evidence/Uncertainty
 * architecture; it does not compute the displacement itself.
 *
 * from/to are borrowed references (not owned) into the caller's
 * NoteEvent sequence.
 */

typedef struct {
    const NoteEvent *from;
    const NoteEvent *to;
    long directed_semitones;
} IntervalAnalysisPair;

typedef struct {
    const IntervalAnalysisPair *pairs;
    size_t pair_count;
} IntervalAnalysisInput;

/*
 * Conforms to AnalyzerFn (analysis/analyzer.h): input MUST point to an
 * IntervalAnalysisInput; context and config are accepted per the
 * interface but unused (interval measurement needs neither yet). Every
 * pair's from/to MUST be non-NULL.
 *
 * Produces exactly one Interpretation and one backing Evidence per
 * pair, in input order: interpretation N's target is pair N's "to"
 * NoteEvent (by id reference), its claim is a long directed-semitones
 * Value equal to pair N's directed_semitones, and its uncertainty is a
 * scalar confidence of 1/1 (the displacement was supplied already
 * measured, not estimated). pair_count == 0 is valid and yields an
 * empty AnalysisResult.
 */
AnalyzerResult interval_analysis_analyze(const void *input, const Context *context, const Value *config,
                                          AnalysisResult **out);

/* The Analyzer value pluggable wherever an Analyzer is expected. */
extern const Analyzer INTERVAL_ANALYSIS_ANALYZER;

#endif