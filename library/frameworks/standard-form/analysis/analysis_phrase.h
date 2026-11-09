#ifndef URSATZ_FRAMEWORKS_STANDARD_FORM_ANALYSIS_ANALYSIS_PHRASE_H
#define URSATZ_FRAMEWORKS_STANDARD_FORM_ANALYSIS_ANALYSIS_PHRASE_H

/*
 * Owner of phrase segmentation (standard_form_segment_phrases) (docs/analytic-job-owners.md, Analytic job
 * owners).
 */

#include <stdbool.h>
#include <stddef.h>

#include "analysis_input.h"
#include "analysis_phrase_grid.h"
#include "interpretation/interpretation.h"
#include "structure/phrase_occurrence.h"
#include "time/time_span.h"

/*
 * Phrase segmentation (the form adapter, step 1). A phrase ends at the arrival of a cadence that is a phrase goal
 * (standard_form_cadence_is_phrase_goal: PAC, IAC, HC; also an authentic cadence with no soprano, AUTHENTIC_UNSPECIFIED).
 * A subverted, deceptive or unclassified cadence is not a goal and ends nothing (it is evidence of an expansion, step 4).
 *
 * REUSE OF THE KERNEL. The segmenter is the kernel's Phrase Boundary Detection (analysis/phrase_boundary_detection.h), not a
 * second one. A framework may not include kernel analysis, so the caller INJECTS it as a StandardFormBoundaryDetector: a
 * function that takes the points below and answers, for each, whether it is a boundary and by which signal. The bridge to the
 * kernel (the points as PhraseBoundaryDetectionPoints, the cadence label from cadence_type_label, the answer read from the
 * PhraseSegmentation Interpretations) is tests/integration/form_kernel_bridge.h.
 *
 * THE POINTS. One per MEASURE that has a sounding soprano note, in order, so the kernel's default of 8 points without a
 * cadence is 8 measures (max_length_without_cadence). A point's event is the first soprano note sounding in the measure. A
 * measure with no note has no point.
 *   cadence signal    a goal cadence whose arrival starts in measure m is on the latest point at or before m (the stronger one
 *                     when two share a point). It ends the phrase at that point's measure
 *   discontinuity     the secondary signal, a break between measure m and the next point's measure: a measure with no note
 *                     between them, a rest at the barline before the next point's first note, or a leap of at least
 *                     config.discontinuity_leap_semitones into it. It is counted by the kernel only after
 *                     max_length_without_cadence points without a boundary. A break within cadence_lookahead_measures points
 *                     before a goal cadence is not reported: the cadence will close the phrase and the break is inside it
 *
 * THE BOUNDARIES. A phrase runs from the measure after the previous phrase to the end of the arrival chord of its cadence: the
 * arrival chord's end snapped to the next beat of the MeterMap (never past the measure's end). The measure of that time is the
 * phrase's last measure. The length of a phrase is last - first + 1 measures, a pickup not counted (it belongs to the first
 * idea). Partial measures count whole.
 *
 * POST-CADENTIAL ABSORPTION. A cadence marked post_cadential (a plagal after a PAC) starts no phrase and is not given to the
 * kernel: the preceding phrase is extended to the end of its arrival chord (an EXTENSION, standard_form external expansion
 * kind POST_CADENTIAL_EXTENSION). After the last goal cadence, a tail of up to config.max_absorbed_tail measures also extends
 * the last phrase: of 1-2 measures a POST_CADENTIAL_EXTENSION, of 3 or more a CODETTA (the suffix lengths of
 * external_expansion_rules.h). A longer tail, or any tail after a boundary the secondary signal made, is a phrase of its own with
 * no cadence.
 *
 * PHRASES ON THE GRID (R-391, standard_form_segment_phrases_on_grid). The points are the same (one per measure with a sounding
 * soprano note, the goal cadences on them, the stronger when two share a point), and the detector is not used: the boundaries
 * come from the phrase grid (analysis_phrase_grid.h). A point at measure m is a boundary when
 *
 *   (a) m is a unit's last measure or the last measure of the first half of a unit of 8 or 16 (half_measure), and a goal cadence
 *       is on it: signal CADENCE, the phrase's end_measure is m (not the end of the arrival chord, which the claimed cadences
 *       of a grid input may hold past it), its span ends with m
 *   (b) m is a unit's last measure with no goal cadence, and the phrase from the measure after the previous boundary to the
 *       next unit's last measure would pass 16 measures, or no unit follows: signal DISCONTINUITY. On the grid this signal
 *       is the 16-measure cap, not a break in the melody (the Analyzer's document schema is unchanged)
 *
 * A cadence anywhere else ends nothing. A phrase therefore ends at a claimed cadence on the grid, and a unit end without one is
 * inside the phrase unless the cap applies. Post-cadential absorption, the tail after the last boundary and the output are as
 * above.
 *
 * OUTPUT. A kernel PhraseOccurrence per phrase (its region: the ids of its soprano notes; target phrase id
 * "standard-form-phrase-<n>") and an Interpretation targeting the occurrence's id: FrameworkReference standard-form, claim = a
 * StandardFormPhraseClaim, and
 *
 *   uncertainty = the cadence's uncertainty; 0.25 for a boundary by the secondary signal (the kernel's 3/4 confidence for it);
 *                 config.no_signal_uncertainty for a final phrase with neither.
 */

#define STANDARD_FORM_PHRASE_CLAIM_TYPE "standard-form/phrase"

typedef enum {
    STANDARD_FORM_SIGNAL_CADENCE = 0,
    STANDARD_FORM_SIGNAL_DISCONTINUITY,
    STANDARD_FORM_SIGNAL_NONE /* the end of the input */
} StandardFormSignal;

typedef struct {
    const NoteEvent *event;
    bool has_cadence; /* a goal cadence arrives in this point's measure (or in a measure with no point after it) */
    CadenceType cadence_type;
    bool has_discontinuity;
} StandardFormBoundaryPoint;

typedef struct {
    bool is_boundary;
    bool by_cadence; /* meaningful only for a boundary: the primary signal triggered it */
} StandardFormBoundary;

/* Answers out[0..point_count) for the points. max_length_without_cadence is already resolved (never 0). */
typedef StandardFormAnalysisStatus (*StandardFormBoundaryDetector)(const StandardFormBoundaryPoint *points,
                                                                   size_t point_count,
                                                                   size_t max_length_without_cadence,
                                                                   StandardFormBoundary *out, void *context);

/* The kernel's default maximum phrase length without a cadence. */
#define STANDARD_FORM_DEFAULT_MAX_LENGTH 8u

typedef struct {
    int index;
    int first_measure;
    int last_measure;       /* includes the extension */
    int end_measure;        /* the phrase proper: the measure its cadence's arrival chord ends in */
    int cadence_measure;    /* the measure the cadence arrives in; -1 with no cadence */
    int counted_first;      /* first_measure, or 1 when the phrase starts with the pickup */
    int length_measures;    /* end_measure - counted_first + 1 */
    int extension_measures; /* last_measure - end_measure */
    int extension_kind;     /* ExternalExpansionKind of the extension, -1 with none */
    int signal;             /* StandardFormSignal */
    int cadence_index;      /* into the input's cadences, -1 with none */
    int cadence_type;       /* CadenceType, -1 with none */
    int strength_rank;      /* of the cadence, 0 with none */
    int note_first;         /* the soprano notes of the phrase: [note_first, note_end) in the input */
    int note_end;
} StandardFormPhraseClaim;

typedef struct {
    StandardFormPhraseClaim claim;
    double uncertainty;
    TimeSpan *span;                 /* owned: the phrase's time span, the extension included */
    PhraseOccurrence *occurrence;   /* owned: the kernel's PhraseOccurrence */
    Interpretation *interpretation; /* owned; targets the occurrence's id */
} StandardFormPhrase;

typedef struct {
    StandardFormPhrase *items; /* owned; in time order; NULL when none */
    size_t count;
} StandardFormPhrases;

/* Segments the input (finished) into phrases. config may be NULL for the defaults. NO_DETECTOR when `detector` is NULL. On OK,
   *out is owned by the caller (standard_form_phrases_destroy); otherwise untouched. No notes gives no phrases. */
StandardFormAnalysisStatus standard_form_segment_phrases(const StandardFormInput *input, const StandardFormConfig *config,
                                                         StandardFormBoundaryDetector detector, void *detector_context,
                                                         StandardFormPhrases *out);
/* The longest phrase the grid lets a unit end without a cadence run on to, in measures. */
#define STANDARD_FORM_GRID_MAX_PHRASE 16

/* Segments the input (finished) into phrases on `grid`, as described above (the detector is not used, so there is none to pass).
   config may be NULL for the defaults. NULL_ARGUMENT for a NULL grid. On OK, *out is owned by the caller
   (standard_form_phrases_destroy); otherwise untouched. No notes gives no phrases. */
StandardFormAnalysisStatus standard_form_segment_phrases_on_grid(const StandardFormInput *input, const StandardFormConfig *config,
                                                                 const StandardFormPhraseGrid *grid, StandardFormPhrases *out);
void standard_form_phrases_destroy(StandardFormPhrases *phrases);

/* Reads the claim back out of a phrase Interpretation. False if it is not one. */
bool standard_form_phrase_claim_read(const Interpretation *interpretation, StandardFormPhraseClaim *out);

/* True for a phrase-goal cadence in this adapter's reading: the framework's goals, and AUTHENTIC_UNSPECIFIED. */
bool standard_form_cadence_ends_phrase(CadenceType type);

#endif
