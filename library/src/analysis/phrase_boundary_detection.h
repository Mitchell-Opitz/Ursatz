#ifndef MUSIC_ANALYSIS_PHRASE_BOUNDARY_DETECTION_H
#define MUSIC_ANALYSIS_PHRASE_BOUNDARY_DETECTION_H

#include <stdbool.h>
#include <stddef.h>

#include "analysis/analyzer.h"
#include "analysis/phrase_segmentation.h"
#include "events/note_event.h"
#include "structure/phrase_occurrence.h"

/*
 * PhraseBoundaryDetection (Procedure -> Analyzer implementation, built
 * on PhraseSegmentation, analysis/phrase_segmentation.h): derives REAL
 * is_boundary values for a NoteEvent sequence from Cadence
 * Interpretations (primary signal) and Contour/RhythmicPattern
 * discontinuity (secondary signal), the same delegation relationship
 * CadenceClassification has to CadenceDetection (analysis/
 * cadence_classification.h): this module computes the signal,
 * PhraseSegmentation still owns packaging is_boundary into an
 * Interpretation/Evidence/Uncertainty. PhraseSegmentation itself is
 * unchanged.
 *
 * Signal 1 (primary) -- cadence: a detected Cadence at a point is
 * always a boundary, whatever its type, including "unclassified" (the
 * same legitimate-value treatment CadenceClassification gives
 * "unclassified" -- not a gap to work around). ChordIdentification/
 * CadenceClassification's candidate-set label strings are not
 * re-parsed here; the caller (who already ran CadenceDetection/
 * CadenceClassification) supplies has_cadence and, when true, the
 * winning candidate's label directly as cadence_type.
 *
 * Signal 2 (secondary) -- contour/rhythm discontinuity: PitchIdentity
 * and Duration carry no numeric semantics this codebase computes from
 * scratch (the same scope boundary IntervalAnalysis/VoiceLeadingAnalysis/
 * MotifWindowMatching already hit), so "a rest, a clear directional or
 * rhythmic break" is supplied already observed by the caller as
 * has_discontinuity, per point -- the same caller-supplies-the-
 * dimension pattern MotifWindowMatching's Contour/RhythmicPattern
 * equality check uses.
 *
 * Design default (flagged, not a Registry-settled rule, same flagged-
 * default treatment MotifWindowMatching's window-matching gives its own
 * "two occurrences minimum" rule): a discontinuity only counts as a
 * boundary once at least max_length_without_cadence points have
 * elapsed since the last boundary (cadence or discontinuity) with no
 * cadence among them -- "a reasonable length" is this many points
 * without a cadence. max_length_without_cadence == 0 selects
 * PHRASE_BOUNDARY_DETECTION_DEFAULT_MAX_LENGTH.
 *
 * event is a borrowed reference (not owned) into the caller's NoteEvent
 * sequence, the same as PhraseSegmentationPoint's event.
 */

#define PHRASE_BOUNDARY_DETECTION_DEFAULT_MAX_LENGTH 8u

typedef struct {
    const NoteEvent *event;

    bool has_cadence;
    /* Borrowed, non-NULL when has_cadence is true; the CadenceClassification (or
       CadenceDetection) winning candidate label, e.g. "authentic (PAC)" or
       "unclassified". Ignored when has_cadence is false. */
    const char *cadence_type;

    /* Contour/RhythmicPattern discontinuity observed at this point (a rest, a clear
       directional/rhythmic break), supplied already determined by the caller. */
    bool has_discontinuity;
} PhraseBoundaryDetectionPoint;

typedef struct {
    const PhraseBoundaryDetectionPoint *points;
    size_t point_count;
    /* 0 selects PHRASE_BOUNDARY_DETECTION_DEFAULT_MAX_LENGTH. */
    size_t max_length_without_cadence;
} PhraseBoundaryDetectionInput;

typedef enum { PHRASE_BOUNDARY_SIGNAL_CADENCE, PHRASE_BOUNDARY_SIGNAL_CONTOUR_RHYTHM } PhraseBoundarySignal;

/*
 * The second, signal-recording claim (Requirement 4's "which signal(s)
 * triggered ... and which cadence type if applicable") packaged into
 * its own Interpretation, alongside (not instead of) the plain
 * PhraseSegmentationClaim delegated to PhraseSegmentation -- both
 * target the same point's event, the same competing/complementary-
 * Interpretations principle Chord/Key already coexist under. Only
 * produced for points PhraseBoundaryDetection marked is_boundary;
 * non-boundary points have no signal to record.
 *
 * cadence_type is a plain copy of the input's cadence_type string when
 * signal == PHRASE_BOUNDARY_SIGNAL_CADENCE, empty otherwise.
 */
#define PHRASE_BOUNDARY_DETECTION_CADENCE_TYPE_BUFFER_SIZE 64

typedef struct {
    PhraseBoundarySignal signal;
    char cadence_type[PHRASE_BOUNDARY_DETECTION_CADENCE_TYPE_BUFFER_SIZE];
} PhraseBoundaryDetectionSignalClaim;

/*
 * Conforms to AnalyzerFn (analysis/analyzer.h): input MUST point to a
 * PhraseBoundaryDetectionInput; context and config are accepted per the
 * interface but unused. Every point's event MUST be non-NULL; a point
 * with has_cadence true MUST have a non-NULL cadence_type whose length
 * (including the terminator) fits PHRASE_BOUNDARY_DETECTION_CADENCE_TYPE_BUFFER_SIZE.
 *
 * Computes is_boundary per point per the primary/secondary signal rule
 * above, packages the resulting PhraseSegmentationPoint array, and
 * delegates entirely to phrase_segmentation_analyze for the plain
 * boundary Interpretation/Evidence/Uncertainty -- its output shape
 * matches PhraseSegmentation's exactly (one Interpretation/Evidence per
 * point, confidence as a scalar). This call produces ONLY that
 * delegated result; the second, signal-recording Interpretation set is
 * obtained separately via phrase_boundary_detection_signal_analyze
 * below (kept as its own AnalyzerFn-conformant call rather than folded
 * in, since it produces a different claim type and this one's output
 * must match PhraseSegmentation's exactly).
 */
AnalyzerResult phrase_boundary_detection_analyze(const void *input, const Context *context, const Value *config,
                                                  AnalysisResult **out);

extern const Analyzer PHRASE_BOUNDARY_DETECTION_ANALYZER;

#define PHRASE_BOUNDARY_DETECTION_DISCONTINUITY_CONFIDENCE_NUM 3u
#define PHRASE_BOUNDARY_DETECTION_DISCONTINUITY_CONFIDENCE_DEN 4u

/*
 * Conforms to AnalyzerFn: same input/validation as
 * phrase_boundary_detection_analyze, same is_boundary computation.
 * Produces one Interpretation and one backing Evidence per point that
 * computed is_boundary == true (skipping non-boundary points -- they
 * have no signal to record), in input order, each targeting that
 * point's event (by id reference) with a PhraseBoundaryDetectionSignalClaim
 * claim and a scalar-confidence Uncertainty (1/1 for a cadence signal,
 * PHRASE_BOUNDARY_DETECTION_DISCONTINUITY_CONFIDENCE_NUM/DEN for a
 * contour/rhythm signal -- a secondary signal without a cadence's
 * certainty). point_count == 0, or a point_count with no boundary
 * points, is valid and yields an empty AnalysisResult.
 */
AnalyzerResult phrase_boundary_detection_signal_analyze(const void *input, const Context *context,
                                                         const Value *config, AnalysisResult **out);

extern const Analyzer PHRASE_BOUNDARY_DETECTION_SIGNAL_ANALYZER;

typedef enum {
    PHRASE_BOUNDARY_DETECTION_OCC_OK = 0,
    PHRASE_BOUNDARY_DETECTION_OCC_ERR_NULL_POINTS,
    PHRASE_BOUNDARY_DETECTION_OCC_ERR_NULL_POINT,
    PHRASE_BOUNDARY_DETECTION_OCC_ERR_ALLOC
} PhraseBoundaryDetectionOccurrenceResult;

/*
 * Groups points into phrases using the same is_boundary computation
 * phrase_boundary_detection_analyze uses (a boundary point closes the
 * phrase it ends, inclusive), and constructs one PhraseOccurrence
 * (structure/phrase_occurrence.h, L5) per phrase spanning that phrase's
 * event range. A fresh Phrase (containers/phrase.h, L4) is constructed
 * per phrase purely to obtain the id PhraseOccurrence's Occurrence
 * targets -- the same transient-construct-then-capture-id pattern
 * MotifDetection uses for MotifOccurrence (analysis/motif_detection.c)
 * -- then destroyed; PhraseOccurrence's own region_event_ids are
 * separate owned copies, not shared with the transient Phrase.
 *
 * Design default (flagged, not Registry-settled): a trailing run of
 * points after the last boundary, with no boundary signal to close it,
 * still becomes a final PhraseOccurrence (the piece's material must
 * end up in some phrase). Same treatment as MotifWindowMatching
 * flagging its own two-occurrences-minimum default.
 *
 * points/point_count validation matches phrase_boundary_detection_analyze.
 * point_count == 0 is valid and yields *out_occurrences = NULL,
 * *out_occurrence_count = 0.
 *
 * On success, *out_occurrences is a freshly allocated array (caller
 * frees with phrase_boundary_detection_destroy_occurrences) of
 * PhraseOccurrence pointers, one per phrase, in input order.
 */
PhraseBoundaryDetectionOccurrenceResult phrase_boundary_detection_build_occurrences(
    const PhraseBoundaryDetectionPoint *points, size_t point_count, size_t max_length_without_cadence,
    PhraseOccurrence ***out_occurrences, size_t *out_occurrence_count);

/* Destroys each PhraseOccurrence in occurrences and the array itself. */
void phrase_boundary_detection_destroy_occurrences(PhraseOccurrence **occurrences, size_t occurrence_count);

#endif
