#ifndef URSATZ_FRAMEWORKS_STANDARD_FORM_ANALYSIS_ANALYSIS_INPUT_H
#define URSATZ_FRAMEWORKS_STANDARD_FORM_ANALYSIS_ANALYSIS_INPUT_H

#include <stdbool.h>
#include <stddef.h>

#include "model/events/note_event.h"
#include "analysis/notes/analysis_note.h"
#include "theory/vocabulary/cadence_type.h"
#include "analysis/notes/contour.h"
#include "theory/vocabulary/motive_occurrence_kind.h"
#include "model/time/meter_map.h"
#include "model/time/metrical_position.h"
#include "model/time/time_span.h"

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
 * THE INPUT is built with the functions below: init with the MeterMap and the soprano AnalysisNotes, add cadences, finish. Cadence
 * ids are copied; the notes (with their ids, NoteEvents and TimeSpans), the degrees and the MeterMap are BORROWED and must outlive
 * the input and every result computed from it.
 *
 *   cadence    a cadence of standard-harmony's analysis: id; the ARRIVAL chord's TimeSpan; CadenceType; strength_rank, the
 *              standard-harmony strength rank (larger is stronger; cadences are compared by it, nothing else); post_cadential,
 *              a plagal cadence directly after a PAC (it extends the preceding phrase and starts no phrase); uncertainty [0, 1]
 *   note       an AnalysisNote of the SOPRANO line, in time order (start non-decreasing): note_id; its NoteEvent (the kernel's point
 *              target); its TimeSpan; and `midi`, its pitch (a whole number of semitones, known). The scale degree of each note
 *              (1..7, 0 unknown) is an optional parallel array, `degrees`
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
    const MeterMap *map;
    StandardFormCadence *cadences; /* owned */
    size_t cadence_count;
    size_t cadence_capacity;
    const AnalysisNote *notes;      /* borrowed: the soprano, in time order (start non-decreasing) */
    size_t note_count;
    const AnalysisNote *bass_notes; /* borrowed, optional (set after init, before finish): the bass line, the same shape */
    size_t bass_count;
    const int *degrees;             /* borrowed, optional (set after init, before finish): one per soprano note, the scale
                                       degree 1..7 when known, 0 when not; NULL when no degree is known */
    bool finished;
} StandardFormInput;

typedef enum {
    STANDARD_FORM_ANALYSIS_OK = 0,
    STANDARD_FORM_ANALYSIS_ERR_NULL_ARGUMENT,
    STANDARD_FORM_ANALYSIS_ERR_BAD_INPUT,   /* a missing id, span or event; an invalid type; uncertainty outside [0, 1]; notes out of order */
    STANDARD_FORM_ANALYSIS_ERR_BAD_CONFIG,
    STANDARD_FORM_ANALYSIS_ERR_NO_DETECTOR, /* no boundary detector, window matcher or relation matcher was injected */
    STANDARD_FORM_ANALYSIS_ERR_DETECTOR,    /* the injected detector failed */
    STANDARD_FORM_ANALYSIS_ERR_METER,       /* a time the MeterMap cannot place */
    STANDARD_FORM_ANALYSIS_ERR_ALLOC
} StandardFormAnalysisStatus;

/* An input over the soprano notes, which are borrowed (with their events, spans and the map they must outlive the input and every
   result). Cadences are added after and owned; bass_notes and degrees are set by the caller before finish. */
void standard_form_input_init(StandardFormInput *input, const MeterMap *map, const AnalysisNote *notes, size_t note_count);
StandardFormAnalysisStatus standard_form_input_add_cadence(StandardFormInput *input, const char *id,
                                                           const TimeSpan *arrival, CadenceType type, int strength_rank,
                                                           bool post_cadential, double uncertainty);
/* Checks the notes (each with an event, a valid span, a non-empty id under 64 characters and a known pitch; starts not decreasing;
   the bass line the same) and the degrees (0 to 7), and sorts the cadences by arrival. BAD_INPUT for what it cannot read. Call once,
   after the last cadence. */
StandardFormAnalysisStatus standard_form_input_finish(StandardFormInput *input);
void standard_form_input_destroy(StandardFormInput *input);

/* Kept for the apps until their next branch: the occurrence kind is semantics/motive_occurrence_kind.h. */
typedef MotiveOccurrenceKind StandardFormMotiveKind;

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
 *   relation_match, relation_context   NULL (default), and required by the motive analysis (NO_DETECTOR without it): the owner of every
 *                                kind but varied (pattern, inversion, retrograde, augmentation, diminution and fragment matching); varied stays the
 *                                framework's
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
    void *relation_context;
    StandardFormRelationMatchFn relation_match; /* injected owner of every kind but varied (NULL by default, required by the motive analysis) */
} StandardFormConfig;

StandardFormConfig standard_form_config_default(void);
/* True when every field is in range (similarity_min in (0, 1], the uncertainty in [0, 1], the leap size positive). */
bool standard_form_config_valid(const StandardFormConfig *config);

#endif
