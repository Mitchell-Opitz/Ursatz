#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_ANALYSIS_ANALYSIS_MELODY_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_ANALYSIS_ANALYSIS_MELODY_H

#include <stdbool.h>
#include <stddef.h>

#include "standard-counterpoint/counterpoint_interval.h"
#include "interpretation/interpretation.h"
#include "pitch/pitch_identity.h"
#include "pitch/pitch_numeric_value.h"
#include "pitch/pitch_spelling.h"
#include "semantics/beat_strength.h"
#include "semantics/contour.h"
#include "semantics/interval.h"
#include "semantics/non_chord_tone.h"
#include "structure/structure.h"
#include "time/metrical_position.h"
#include "time/time_span.h"

/*
 * The melody adapter of standard-counterpoint (L3.5, step 1): melodic intervals and contour of ONE line. There is no
 * melody framework; the melodic rules (steps and leaps, leap recovery, a single climax) are this framework's, so the
 * adapter is here. standard-counterpoint includes no other framework: the input is the neutral line below, which the caller
 * fills (tests/integration/ builds it from the outer voices and the other frameworks' results).
 *
 * THE LINE. One entry per melody position, in time order. An entry is a NOTE or a REST (present = false). A rest, or the
 * end of the line, breaks the chain: no interval is formed across it. Each entry is a distinct note: a note held across
 * several segments of the caller's segmentation is ONE entry (the caller merges them), and a repeated pitch is a second
 * entry. A note carries:
 *
 *   note_id       the note's id (non-empty). Copied. The id the Interpretations refer to
 *   span          its TimeSpan, or NULL. Borrowed
 *   position      its MetricalPosition, or NULL. Borrowed
 *   has_strength  strength is meaningful; strength is its BeatStrength
 *   pitch         its PitchNumericValue (required; a whole number of semitones, denominator 1). Borrowed
 *   spelling      its PitchSpelling when known (MusicXML), or NULL. Borrowed
 *   degree        its scale degree 1..7 when known (0: unknown), and alteration -1, 0 or +1 (a flat or sharp degree)
 *   nct, nct_type, nct_ref   the non-chord-tone result for the note (analysis_melody_nct.h). Copied
 *   confidence    [0, 1], the line extraction's confidence in the note, used in uncertainty
 *
 * Borrowed pointers must outlive the line and every result computed from it.
 *
 * INTERVALS. For each pair of consecutive entries that are both notes:
 *
 *   semitones    the signed difference of the two PitchNumericValues
 *   direction    ContourDirection UP, DOWN or SAME. The kernel Contour and Interval do not compute direction or size from
 *                PitchIdentity (an opaque token with no tuning), so the adapter, which has the numeric pitches, computes
 *                it and SUPPLIES it to them
 *   generic size the size in letter names (a third is 3; compound included), taken from the SPELLINGS when both are given
 *                (the existing interval, counterpoint_interval.h), else from the SCALE DEGREES when both are known (the
 *                letter distance in the direction of motion plus the octaves the semitones imply), else ABSENT (0)
 *   leap / step  decided on the generic size by the existing definitions (counterpoint_interval.h: a step is a second, a
 *                leap is a third or larger). With no generic size they are read from the semitones (a leap is 3 or more,
 *                a step 1 or 2), the interval is marked APPROXIMATE, and its uncertainty is raised by the penalty
 *                (an augmented second is then read as a leap: a known limit)
 *
 * Each interval is a Structure shell (the two note ids), kernel Interval and Contour values, and an Interpretation
 * targeting the shell's id: FrameworkReference standard-counterpoint, claim = a StandardCounterpointMelodicIntervalClaim,
 *
 *   uncertainty = 1 - (1 - u)(1 - penalty), u = 1 - the lesser note confidence, penalty = config.approximate_penalty when
 *                 the interval is approximate and 0 otherwise.
 *
 * SPANS. A span is a run of entries [first, last] (inclusive indices into the line), supplied by the caller (phrases do not
 * exist yet); with none given the whole line is one span. A contour summary per span counts the intervals inside it (both
 * notes in the span) as up, down and same, and gives the net direction and net semitones (last note minus first), the range
 * in semitones, and the lowest and highest notes (the FIRST entry at each pitch).
 */

#define STANDARD_COUNTERPOINT_MELODIC_INTERVAL_CLAIM_TYPE "standard-counterpoint/melodic-interval"
#define STANDARD_COUNTERPOINT_CONTOUR_SUMMARY_CLAIM_TYPE "standard-counterpoint/contour-summary"

typedef enum {
    STANDARD_COUNTERPOINT_NCT_UNCLASSIFIED = 0, /* no result for the note (the default) */
    STANDARD_COUNTERPOINT_NCT_CHORD_TONE,
    STANDARD_COUNTERPOINT_NCT_NON_CHORD_TONE /* nct_type says which */
} StandardCounterpointNctStatus;

typedef struct {
    bool present; /* false: a rest */
    const char *note_id;
    const TimeSpan *span;
    const MetricalPosition *position;
    bool has_strength;
    BeatStrength strength;
    const PitchNumericValue *pitch;
    const PitchSpelling *spelling;
    int degree;
    int alteration;
    StandardCounterpointNctStatus nct;
    NonChordTone nct_type;
    const char *nct_ref;
    double confidence;
    unsigned chord_pitch_classes; /* 12-bit mask (bit pc set) of the note's chord; 0 (the default): none known. Read only
                                     by standard_counterpoint_melody_nct_labels_with. */
} StandardCounterpointMelodyNote;

typedef struct {
    StandardCounterpointMelodyNote *notes; /* owned; count entries */
    size_t count;
    size_t capacity;
} StandardCounterpointMelodyLine;

typedef enum {
    STANDARD_COUNTERPOINT_MELODY_OK = 0,
    STANDARD_COUNTERPOINT_MELODY_ERR_NULL_ARGUMENT,
    STANDARD_COUNTERPOINT_MELODY_ERR_BAD_NOTE, /* no id or pitch, a pitch that is not a whole semitone, a degree outside
                                                  0..7, an alteration outside -1..1, a confidence outside [0, 1], an
                                                  invalid strength or NCT value, an nct_ref of 128 characters or more */
    STANDARD_COUNTERPOINT_MELODY_ERR_BAD_SPAN,
    STANDARD_COUNTERPOINT_MELODY_ERR_ALLOC
} StandardCounterpointMelodyStatus;

/* The builder: init an empty line, add entries in time order, destroy. add_note copies the note's strings and validates it
   (a `present` note is required; for a rest use add_rest). */
void standard_counterpoint_melody_line_init(StandardCounterpointMelodyLine *line);
StandardCounterpointMelodyStatus standard_counterpoint_melody_line_add_note(StandardCounterpointMelodyLine *line,
                                                                            const StandardCounterpointMelodyNote *note);
StandardCounterpointMelodyStatus standard_counterpoint_melody_line_add_rest(StandardCounterpointMelodyLine *line);
void standard_counterpoint_melody_line_destroy(StandardCounterpointMelodyLine *line);

typedef struct {
    double approximate_penalty; /* default 0.25 */
} StandardCounterpointMelodyConfig;

StandardCounterpointMelodyConfig standard_counterpoint_melody_config_default(void);

typedef enum {
    STANDARD_COUNTERPOINT_GENERIC_NONE = 0,
    STANDARD_COUNTERPOINT_GENERIC_SPELLING,
    STANDARD_COUNTERPOINT_GENERIC_DEGREE
} StandardCounterpointGenericSource;

typedef struct {
    int from;      /* entry index of the first note */
    int to;        /* entry index of the second note (from + 1) */
    long semitones; /* signed */
    int direction; /* ContourDirection */
    int generic_size;    /* 0 when absent */
    int generic_source;  /* StandardCounterpointGenericSource */
    int approximate;     /* leap/step read from the semitones */
    int leap;
    int step;
} StandardCounterpointMelodicIntervalClaim;

typedef struct {
    StandardCounterpointMelodicIntervalClaim claim;
    StandardCounterpointInterval effective; /* the interval the leap/step decision used; quality is meaningful only
                                               when claim.generic_source is SPELLING */
    double uncertainty;
    PitchIdentity *pitch_a;         /* owned: the Interval's references */
    PitchIdentity *pitch_b;
    Interval *interval;             /* owned: kernel value, directed and undirected semitones */
    Contour *contour;               /* owned: the two pitches and the direction */
    Structure *shell;               /* owned: the two note ids */
    Interpretation *interpretation; /* owned; targets the shell's id */
} StandardCounterpointMelodicInterval;

typedef struct {
    StandardCounterpointMelodicInterval *items; /* owned; in line order; NULL when none */
    size_t count;
} StandardCounterpointMelodicIntervals;

/* Every interval of the line. On OK, *out is owned by the caller; otherwise untouched. config may be NULL (the defaults). */
StandardCounterpointMelodyStatus standard_counterpoint_melody_intervals(const StandardCounterpointMelodyLine *line,
                                                                        const StandardCounterpointMelodyConfig *config,
                                                                        StandardCounterpointMelodicIntervals *out);
void standard_counterpoint_melodic_intervals_destroy(StandardCounterpointMelodicIntervals *intervals);

/* Reads the claim back from an interval Interpretation. False if it is not one. */
bool standard_counterpoint_melodic_interval_claim_read(const Interpretation *interpretation,
                                                       StandardCounterpointMelodicIntervalClaim *out);

typedef struct {
    const char *id; /* the span's name (used in claim ids); borrowed */
    size_t first;   /* inclusive entry indices */
    size_t last;
} StandardCounterpointMelodySpan;

typedef struct {
    int span_first;
    int span_last;
    int note_count;
    int up;
    int down;
    int same;
    int step_count;
    int leap_count;
    int net_direction;     /* ContourDirection of the last note against the first */
    long net_semitones;
    long range_semitones;  /* highest minus lowest */
    int lowest_index;      /* first entry at the lowest pitch */
    int highest_index;     /* first entry at the highest pitch */
} StandardCounterpointContourSummaryClaim;

typedef struct {
    char span_id[64];
    StandardCounterpointContourSummaryClaim claim;
    double uncertainty; /* 1 - the least note confidence in the span */
    Structure *shell;               /* owned: every note id in the span */
    Interpretation *interpretation; /* owned */
} StandardCounterpointContourSummary;

typedef struct {
    StandardCounterpointContourSummary *items; /* owned; in span order; NULL when none */
    size_t count;
} StandardCounterpointContourSummaries;

/* One summary per span that holds at least one note (a span of rests only has none). spans may be NULL with span_count 0
   for the whole line as one span "whole-line". A span with first > last or past the line is BAD_SPAN. */
StandardCounterpointMelodyStatus standard_counterpoint_melody_contour_summaries(
    const StandardCounterpointMelodyLine *line, const StandardCounterpointMelodicIntervals *intervals,
    const StandardCounterpointMelodySpan *spans, size_t span_count, StandardCounterpointContourSummaries *out);
void standard_counterpoint_contour_summaries_destroy(StandardCounterpointContourSummaries *summaries);

bool standard_counterpoint_contour_summary_claim_read(const Interpretation *interpretation,
                                                      StandardCounterpointContourSummaryClaim *out);

#endif
