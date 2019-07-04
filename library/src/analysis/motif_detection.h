#ifndef MUSIC_ANALYSIS_MOTIF_DETECTION_H
#define MUSIC_ANALYSIS_MOTIF_DETECTION_H

#include <stddef.h>

#include "analysis/analyzer.h"
#include "structure/motif.h"

/*
 * MotifDetection (Procedure -> Analyzer implementation, Registry #20):
 * recognizes occurrences of a Motif (structure/motif.h) within a region
 * and reports each as an Interpretation backed by Evidence and
 * Uncertainty (TDD Sec242), per the Analyzer interface (analysis/
 * analyzer.h).
 *
 * MUST NOT duplicate matched events (TDD Sec152): a detected occurrence
 * is represented purely as EntityID references to the matched
 * NoteEvents, exactly what MotifOccurrence (structure/motif_occurrence.h,
 * L5) itself carries. MotifDetection exercises MotifOccurrence's real
 * constructor per occurrence (occurrence_create + motif_occurrence_create)
 * to validate the reference list before reporting it -- non-empty,
 * every event id present, no NoteEvent object ever copied -- the same
 * validation MotifOccurrence would perform for any other caller.
 *
 * The Interpretation this analyzer emits targets the MotifOccurrence it
 * just validated (by that MotifOccurrence's own id, a fresh EntityID
 * copy) rather than embedding the matched event list a second time: the
 * event references already live on the (transient, immediately
 * destroyed after its id is copied out) MotifOccurrence, and duplicating
 * them again into the Interpretation's claim would be exactly the
 * duplication TDD Sec152 forbids. A caller wanting the persisted
 * MotifOccurrence reconstructs it from matched_event_ids the same way
 * MotifDetection did -- the analyzer's job is detection and reporting,
 * not owning the resulting Structure object (the same division
 * CadenceDetection uses for Cadence, analysis/cadence_detection.h).
 *
 * PitchIdentity carries no numeric semantics yet (no tuning system, no
 * PitchClass): MotifDetection cannot derive pattern similarity from raw
 * PitchIdentity tokens. Each occurrence's matched event set and match
 * confidence are supplied already determined (the same
 * caller-supplies-the-dimension pattern IntervalAnalysis and
 * VoiceLeadingAnalysis use) -- MotifDetection's job is to validate and
 * package that finding, not to search for it.
 *
 * motif and each occurrence's matched_event_ids entries are borrowed
 * (not owned) references.
 */

typedef struct {
    const EntityID *const *matched_event_ids;
    size_t matched_event_id_count;
    unsigned long confidence_numerator;
    unsigned long confidence_denominator;
} MotifDetectionOccurrence;

typedef struct {
    const Motif *motif;
    const MotifDetectionOccurrence *occurrences;
    size_t occurrence_count;
} MotifDetectionInput;

/* The claim packaged into each Interpretation -- how many events this occurrence matched. */
typedef struct {
    size_t matched_event_id_count;
} MotifDetectionClaim;

/*
 * Conforms to AnalyzerFn (analysis/analyzer.h): input MUST point to a
 * MotifDetectionInput with a non-NULL motif; context and config are
 * accepted per the interface but unused (motif matching needs neither
 * yet). Every occurrence MUST have matched_event_id_count > 0, a
 * non-NULL matched_event_ids with no NULL entry, and a positive
 * confidence_denominator with confidence_numerator <= confidence_
 * denominator -- the same validation motif_occurrence_create and
 * uncertainty_create_scalar_confidence already enforce, surfaced here
 * as ANALYZER_ERR_NULL_INPUT/ANALYZER_ERR_ANALYSIS_FAILED so a caller
 * does not need to duplicate those checks itself.
 *
 * Produces exactly one Interpretation and one backing Evidence per
 * occurrence, in input order: interpretation N's target is a fresh
 * EntityID copy of the MotifOccurrence validated from occurrence N (its
 * own id, distinct from motif's id and from any matched event's id),
 * its claim is a MotifDetectionClaim recording matched_event_id_count,
 * and its uncertainty is occurrence N's confidence as a scalar
 * confidence. The backing Evidence's source is motif's own id value.
 * occurrence_count == 0 is valid and yields an empty AnalysisResult.
 */
AnalyzerResult motif_detection_analyze(const void *input, const Context *context, const Value *config,
                                        AnalysisResult **out);

/* The Analyzer value pluggable wherever an Analyzer is expected. */
extern const Analyzer MOTIF_DETECTION_ANALYZER;

#endif