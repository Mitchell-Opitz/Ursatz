#ifndef URSATZ_FRAMEWORKS_STANDARD_FORM_ANALYSIS_ANALYSIS_INPUT_H
#define URSATZ_FRAMEWORKS_STANDARD_FORM_ANALYSIS_ANALYSIS_INPUT_H

#include <stdbool.h>
#include <stddef.h>

#include "events/note_event.h"
#include "semantics/analysis_note.h"
#include "semantics/analysis_segment.h"
#include "semantics/cadence_type.h"
#include "semantics/contour.h"
#include "semantics/motive_occurrence_kind.h"
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
    int degree;            /* the scale degree 1..7 when known (0: unknown), set by standard_form_input_set_degree */
    int alteration;        /* -1, 0 or +1: a flat or sharp degree */
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
    StandardFormNote *bass_notes; /* the optional bass line, same shape as the soprano's */
    size_t bass_count;
    size_t bass_capacity;
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
/* The optional BASS line (used only when config.include_bass is set): the same as the soprano's notes, in time order. */
StandardFormAnalysisStatus standard_form_input_add_bass_note(StandardFormInput *input, const char *note_id,
                                                             const NoteEvent *event, const TimeSpan *span,
                                                             const MetricalPosition *position, long semitone);
/* Adds an AnalysisNote to the soprano line (bass false) or the bass line (bass true): `semitone` is midi, position is NULL, the next-note
   fields are set by standard_form_input_finish. BAD_INPUT for an unknown pitch, no event, or what the line's add rejects. */
StandardFormAnalysisStatus standard_form_input_add_analysis_note(StandardFormInput *input, const AnalysisNote *note, bool bass);
/* The scale degree (1..7) and alteration (-1, 0, +1) of note `index` of the soprano (bass false) or the bass line. Degrees give the
   motive analysis a generic (scale-step) interval, so a diatonic sequence matches. BAD_INPUT for an index out of range. */
StandardFormAnalysisStatus standard_form_input_set_degree(StandardFormInput *input, bool bass, size_t index, int degree,
                                                          int alteration);
StandardFormAnalysisStatus standard_form_input_add_segment(StandardFormInput *input, const char *id,
                                                           const TimeSpan *span, int function, int phrase_model_label);
/* Adds an AnalysisSegment (id and span) with the function and phrase model label the caller reads for it. */
StandardFormAnalysisStatus standard_form_input_add_analysis_segment(StandardFormInput *input, const AnalysisSegment *segment,
                                                                    int function, int phrase_model_label);
/* Fills the interval and contour to the next note and sorts the cadences by arrival. Call once, after the last add. */
StandardFormAnalysisStatus standard_form_input_finish(StandardFormInput *input);
void standard_form_input_destroy(StandardFormInput *input);

/* Kept for the apps until B12 of P28: the occurrence kind is semantics/motive_occurrence_kind.h. */
typedef MotiveOccurrenceKind StandardFormMotiveKind;
#define STANDARD_FORM_MOTIVE_EXACT MOTIVE_OCCURRENCE_EXACT
#define STANDARD_FORM_MOTIVE_REAL MOTIVE_OCCURRENCE_REAL
#define STANDARD_FORM_MOTIVE_TONAL MOTIVE_OCCURRENCE_TONAL
#define STANDARD_FORM_MOTIVE_INVERTED MOTIVE_OCCURRENCE_INVERTED
#define STANDARD_FORM_MOTIVE_RETROGRADE MOTIVE_OCCURRENCE_RETROGRADE
#define STANDARD_FORM_MOTIVE_AUGMENTED MOTIVE_OCCURRENCE_AUGMENTED
#define STANDARD_FORM_MOTIVE_DIMINISHED MOTIVE_OCCURRENCE_DIMINISHED
#define STANDARD_FORM_MOTIVE_FRAGMENTED MOTIVE_OCCURRENCE_FRAGMENTED
#define STANDARD_FORM_MOTIVE_VARIED MOTIVE_OCCURRENCE_VARIED
#define STANDARD_FORM_MOTIVE_KIND_COUNT MOTIVE_OCCURRENCE_KIND_COUNT

typedef struct StandardFormMotiveShape StandardFormMotiveShape;

/* What an injected matcher says about a candidate window: how it reads against the prototype. */
typedef struct {
    MotiveOccurrenceKind kind;
    double score;               /* in (0, 1] */
    int fragment_part;          /* FRAGMENTED: 0 prefix, 1 suffix; else -1 */
    size_t fragment_intervals;  /* FRAGMENTED: the intervals of the fragment; else 0 */
} StandardFormRelationMatch;

/*
 * An injected matcher for every occurrence kind the kernel owns (motive_variant_recognize): EXACT, REAL, TONAL, INVERTED,
 * RETROGRADE, AUGMENTED, DIMINISHED and FRAGMENTED. True when the candidate is the prototype under one relation, with *out filled;
 * false: no relation. The candidate may be shorter than the prototype (a fragment search). context is the config's relation_context.
 * VARIED is not matched by it; it stays the framework's.
 */
typedef bool (*StandardFormRelationMatchFn)(const StandardFormMotiveShape *prototype, const StandardFormMotiveShape *candidate,
                                            StandardFormRelationMatch *out, void *context);

/*
 * An injected matcher for the transformed occurrences the kernel owns (motive_relation_apply: transposition, inversion,
 * retrograde). True when the candidate is the prototype under one of them: *kind is EXACT, REAL, TONAL, INVERTED or RETROGRADE and
 * *score in (0, 1]. False: no relation. context is the config's relation_context.
 */
typedef bool (*StandardFormRelationMatcher)(const StandardFormMotiveShape *prototype, const StandardFormMotiveShape *candidate,
                                            StandardFormMotiveKind *kind, double *score, void *context);

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
 *   motive_min_notes, motive_max_notes, varied_max_mismatch, include_bass   the motive analysis (analysis_motive.h): 3, 8, 1, false
 *   relation_match, relation_context   NULL (default): the matchers below. Set: it replaces the pattern, inversion, retrograde,
 *                                augmentation, diminution and fragment matchers, in their places in the match order; varied stays the framework's
 *   relation_matcher, relation_context   (kept until B12; used only when relation_match is NULL) NULL (default): standard-form's own pattern, inversion and retrograde matchers. Set: the
 *                                matcher replaces those three, in their place in the match order (analysis_motive_match.h); the
 *                                other kinds (augmentation, diminution, varied, fragment) keep the framework's matchers
 */
typedef struct {
    size_t max_length_without_cadence;
    long discontinuity_leap_semitones;
    size_t cadence_lookahead_measures;
    size_t max_absorbed_tail;
    double similarity_min;
    double no_signal_uncertainty;
    size_t motive_min_notes;      /* the fewest notes of a motive (default 3) */
    size_t motive_max_notes;      /* the most (default 8) */
    size_t varied_max_mismatch;   /* interval mismatches a varied motive allows (default 1) */
    bool include_bass;            /* also look for motives in the bass line, apart from the soprano's (default false) */
    StandardFormRelationMatcher relation_matcher; /* kept until B12 of P28: used only when relation_match is NULL (default NULL) */
    void *relation_context;
    StandardFormRelationMatchFn relation_match; /* injected owner of every kind but varied (default NULL) */
} StandardFormConfig;

StandardFormConfig standard_form_config_default(void);
/* True when every field is in range (similarity_min in (0, 1], the uncertainty in [0, 1], the leap size positive). */
bool standard_form_config_valid(const StandardFormConfig *config);

#endif
