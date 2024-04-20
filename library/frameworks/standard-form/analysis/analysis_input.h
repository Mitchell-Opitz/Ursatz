#ifndef URSATZ_FRAMEWORKS_STANDARD_FORM_ANALYSIS_ANALYSIS_INPUT_H
#define URSATZ_FRAMEWORKS_STANDARD_FORM_ANALYSIS_ANALYSIS_INPUT_H

#include <stdbool.h>
#include <stddef.h>

#include "events/note_event.h"
#include "semantics/cadence_type.h"
#include "semantics/contour.h"
#include "time/meter_map.h"
#include "time/metrical_position.h"
#include "time/time_span.h"

/*
 * The standard-form form adapter (L3.6): phrases, ideas and subphrases, themes and phrase expansion from real cadences and
 * the soprano. standard-form includes no other framework and no kernel analysis (the architecture test enforces both): the
 * input is the neutral structs below, which the caller fills (tests/integration/ builds them from the standard-harmony
 * cadences, the standard-counterpoint melody line and the MeterMap), and the one kernel procedure the adapter reuses, Phrase
 * Boundary Detection, is INJECTED as a StandardFormBoundaryDetector (analysis_phrase.h).
 *
 * Lengths are in measures, counted from the MeterMap (time/meter_map.h). A pickup (a first region the MeterMap marks as measure
 * 0) belongs to the first idea and is NOT counted in any length.
 *
 * THE INPUT is built with the functions below: init with the MeterMap, add cadences, soprano notes and segments, finish. Ids
 * are copied; TimeSpans, NoteEvents, MetricalPositions and the MeterMap are BORROWED and must outlive the input and every
 * result computed from it.
 *
 *   cadence    a cadence of standard-harmony's analysis: id; the ARRIVAL chord's TimeSpan; CadenceType; strength_rank, the
 *              standard-harmony strength rank (larger is stronger; cadences are compared by it, nothing else); post_cadential,
 *              a plagal cadence directly after a PAC (it extends the preceding phrase and starts no phrase); uncertainty [0, 1]
 *   note       a SOPRANO note, in time order (start non-decreasing): id; its NoteEvent (the kernel's point target); its TimeSpan;
 *              optionally its MetricalPosition; `semitone`, its pitch in semitones (a whole number); and, filled by
 *              standard_form_input_finish, the interval and contour to the NEXT note (has_next is false at the end of the line
 *              and across a rest: a gap between one note's end and the next one's start)
 *   segment    a harmonic segment: id, TimeSpan, the harmonic function and the phrase-model label as the caller's integer
 *              codes (carried into claims; the only use here is that segment boundaries give the harmonic rhythm)
 */

typedef struct {
    char id[64];
    const TimeSpan *arrival;
    CadenceType type;
    int strength_rank;
    bool post_cadential;
    double uncertainty;
} StandardFormCadence;

typedef struct {
    char note_id[64];
    const NoteEvent *event;
    const TimeSpan *span;
    const MetricalPosition *position; /* may be NULL */
    long semitone;
    bool has_next;
    long semitones_to_next;
    int direction_to_next; /* ContourDirection; meaningful only when has_next */
} StandardFormNote;

typedef struct {
    char id[64];
    const TimeSpan *span;
    int function;
    int phrase_model_label;
} StandardFormSegment;

typedef struct {
    const MeterMap *map;
    StandardFormCadence *cadences;
    size_t cadence_count;
    size_t cadence_capacity;
    StandardFormNote *notes;
    size_t note_count;
    size_t note_capacity;
    StandardFormSegment *segments;
    size_t segment_count;
    size_t segment_capacity;
    bool finished;
} StandardFormInput;

typedef enum {
    STANDARD_FORM_ANALYSIS_OK = 0,
    STANDARD_FORM_ANALYSIS_ERR_NULL_ARGUMENT,
    STANDARD_FORM_ANALYSIS_ERR_BAD_INPUT,   /* a missing id, span or event; an invalid type; uncertainty outside [0, 1]; notes out of order */
    STANDARD_FORM_ANALYSIS_ERR_BAD_CONFIG,
    STANDARD_FORM_ANALYSIS_ERR_NO_DETECTOR, /* no boundary detector was injected */
    STANDARD_FORM_ANALYSIS_ERR_DETECTOR,    /* the injected detector failed */
    STANDARD_FORM_ANALYSIS_ERR_METER,       /* a time the MeterMap cannot place */
    STANDARD_FORM_ANALYSIS_ERR_ALLOC
} StandardFormAnalysisStatus;

void standard_form_input_init(StandardFormInput *input, const MeterMap *map);
StandardFormAnalysisStatus standard_form_input_add_cadence(StandardFormInput *input, const char *id,
                                                           const TimeSpan *arrival, CadenceType type, int strength_rank,
                                                           bool post_cadential, double uncertainty);
StandardFormAnalysisStatus standard_form_input_add_note(StandardFormInput *input, const char *note_id,
                                                        const NoteEvent *event, const TimeSpan *span,
                                                        const MetricalPosition *position, long semitone);
StandardFormAnalysisStatus standard_form_input_add_segment(StandardFormInput *input, const char *id,
                                                           const TimeSpan *span, int function, int phrase_model_label);
/* Fills the interval and contour to the next note and sorts the cadences by arrival. Call once, after the last add. */
StandardFormAnalysisStatus standard_form_input_finish(StandardFormInput *input);
void standard_form_input_destroy(StandardFormInput *input);

/*
 * THE CONFIGURATION. Defaults are stated, not learned.
 *
 *   max_length_without_cadence   measures a phrase may run before the secondary signal can end it (0: the kernel's default, 8)
 *   discontinuity_leap_semitones the secondary signal: a rest at a measure's start, or a leap of at least this many semitones
 *                                into its first note, is a break before it (default 7, a fifth)
 *   cadence_lookahead_measures   a break within this many measures before a goal cadence is not reported as a discontinuity: the
 *                                cadence will close the phrase, so the break is inside it (default 4; 0 reports every break)
 *   max_absorbed_tail            measures after the last cadence that extend the last phrase instead of forming one (default 8)
 *   similarity_min               idea similarity: the least share of matching intervals for a match (default 0.75)
 *   no_signal_uncertainty        the uncertainty of a phrase that ends with no cadence and no discontinuity, only at the end of
 *                                the input (default 0.5)
 */
typedef struct {
    size_t max_length_without_cadence;
    long discontinuity_leap_semitones;
    size_t cadence_lookahead_measures;
    size_t max_absorbed_tail;
    double similarity_min;
    double no_signal_uncertainty;
} StandardFormConfig;

StandardFormConfig standard_form_config_default(void);
/* True when every field is in range (similarity_min in (0, 1], the uncertainty in [0, 1], the leap size positive). */
bool standard_form_config_valid(const StandardFormConfig *config);

#endif
