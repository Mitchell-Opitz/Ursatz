#ifndef URSATZ_FRAMEWORKS_STANDARD_FORM_ANALYSIS_ANALYSIS_MOTIVE_H
#define URSATZ_FRAMEWORKS_STANDARD_FORM_ANALYSIS_ANALYSIS_MOTIVE_H

#include <stdbool.h>
#include <stddef.h>

#include "analysis_theme.h"
#include "identity/entity_id.h"
#include "relationship/relationship.h"
#include "semantics/contour.h"
#include "semantics/motif_window.h"
#include "semantics/rhythmic_pattern.h"
#include "structure/motif.h"
#include "structure/motif_occurrence.h"

/*
 * Motive analysis (the form adapter, L3.7): motives of the soprano (and, by config, the bass) found by interval and rhythm, and the
 * ways they recur. A motive is a short, regularly recurring snippet of music (motive_rules.h); a recurring motive commonly
 * undergoes the transformations that file lists. Motives of different voices are not matched with each other, and inner voices are
 * not read.
 *
 * DIMENSIONS. A window of notes has two interval sequences: the SEMITONES between successive notes, and, when every note of the
 * window has a scale degree, the GENERIC interval (scale steps, signed, 7 to an octave: the letter distance the degrees give plus the
 * octaves the semitones imply). The generic sequence is the pattern when known, so a diatonic transposition or sequence matches;
 * otherwise the semitone sequence is. A window with generic intervals never matches one without. Its RHYTHM is the sequence of
 * inter-onset intervals (onset to onset) as exact rationals; an n-note window has n - 1 of each. A window does not cross a rest, and
 * it starts on the first note of an idea or on a beat of the MeterMap (the bass: on a beat).
 *
 * DISCOVERY. Every window of config.motive_min_notes to config.motive_max_notes notes is a candidate. Windows with the same pattern
 * (interval sequence and rhythm) form a group; its occurrences are taken in order, an occurrence that overlaps the one before it
 * dropped. A group of at least TWO occurrences is a motive (a pattern seen once is none). MAXIMALITY: a group is suppressed when every
 * one of its occurrences lies inside an occurrence of a longer kept group. The first occurrence is the PROTOTYPE of the family.
 *
 * KERNEL. The kernel's Motif Window Matching (src/analysis/motif_window_matching.h: a window's Contour direction sequence and its
 * RhythmicPattern, both exactly equal) is the first test of a match. A framework may not include kernel analysis, so the caller INJECTS
 * it as a StandardFormWindowMatcher (tests/integration/form_kernel_bridge.h). Every window of a kept group is given to it, and each must
 * match another: a window the kernel does not match is an error (DETECTOR), since equal intervals and equal rhythm imply the kernel's
 * test. The kernel only compares directions; the adapter's interval comparison is the finer, and decides.
 *
 * KINDS of an occurrence (StandardFormMotiveKind), each relative to the prototype:
 *   EXACT     the same pitches (semitones, the first note included) and rhythm
 *   REAL      the same semitone intervals from another pitch: a real transposition
 *   TONAL     the same generic intervals but not the same semitones: a diatonic transposition or sequence
 *   (the others: analysis_motive_match.h and step 3)
 *
 * FAMILY OUTPUT. One Motif per family (the prototype's note ids), an Interpretation targeting it (claim StandardFormMotiveClaim);
 * a MotifOccurrence per occurrence (its note ids) and an Interpretation targeting it with the KIND (claim
 * StandardFormMotiveOccurrenceClaim), whose uncertainty is 1 - score (0 for a full match); a Relationship Motif -> MotifOccurrence,
 * predicate "occurrence-of" (the family); and a Relationship MotifOccurrence -> the innermost idea shell that holds all its notes,
 * else the phrase occurrence that does, predicate "contained-in" (none when neither).
 */

#define STANDARD_FORM_MOTIVE_CLAIM_TYPE "standard-form/motive"
#define STANDARD_FORM_MOTIVE_OCCURRENCE_CLAIM_TYPE "standard-form/motive-occurrence"

/* StandardFormMotiveKind is defined in analysis_input.h (the relation matcher of StandardFormConfig names it). */

/* "exact" "real-transposed" "tonal-transposed" "inverted" "retrograde" "augmented" "diminished" "fragmented" "varied"; NULL otherwise. */
const char *standard_form_motive_kind_name(StandardFormMotiveKind kind);
/* The precedence rank of a kind when several apply (lower wins): exact 0, real and tonal 1, inverted and retrograde 2, augmented and
   diminished 3, fragmented 4, varied 5. -1 for an invalid kind. */
int standard_form_motive_kind_rank(StandardFormMotiveKind kind);

/* One window handed to the injected matcher: the kernel's MotifWindow (semantics/motif_window.h). Borrowed for the call. */
typedef MotifWindow StandardFormMotifWindow;

/* Sets matched[i] for each window: true when it matches at least one other window (the kernel's rule). */
typedef StandardFormAnalysisStatus (*StandardFormWindowMatcher)(const StandardFormMotifWindow *windows, size_t count,
                                                                bool *matched, void *context);

typedef struct {
    int family;
    int line;             /* 0 soprano, 1 bass */
    int note_count;       /* the prototype's */
    int occurrence_count;
    int generic;          /* the pattern is the generic interval sequence (else the semitones) */
} StandardFormMotiveClaim;

typedef struct {
    int family;
    int occurrence;       /* within the family; 0 is the prototype */
    int line;
    int note_first;       /* the occurrence's notes: [note_first, note_first + note_count) of the line */
    int note_count;
    int kind;             /* StandardFormMotiveKind */
    int dimension_generic;
    int mismatches;       /* interval mismatches of a varied occurrence, else 0 */
    int intervals;        /* the intervals compared */
    double rhythm_factor; /* 1, or 0.8 for a pitch-only match of a varied motive */
    double score;         /* 1 - mismatches / intervals, times the rhythm factor */
    int fragment_part;    /* a fragment: 0 prefix, 1 suffix; else -1 */
    int fragment_intervals;
    int in_continuation;  /* a fragment found inside a continuation span */
    int first_measure;
    double first_beat;    /* 1-based, in beats of the measure */
} StandardFormMotiveOccurrenceClaim;

typedef struct {
    StandardFormMotiveOccurrenceClaim claim;
    double uncertainty;                  /* 1 - score */
    MotifOccurrence *occurrence;         /* owned */
    Interpretation *interpretation;      /* owned; targets the occurrence's id */
    Relationship *family_relationship;   /* owned: Motif -> occurrence, "occurrence-of" */
    Relationship *contained_relationship; /* owned: occurrence -> idea or phrase, "contained-in"; NULL with none */
} StandardFormMotiveOccurrence;

typedef struct {
    StandardFormMotiveClaim claim;
    Motif *motif;                         /* owned */
    Interpretation *interpretation;       /* owned; targets the motif's id */
    StandardFormMotiveOccurrence *occurrences; /* owned; the prototype first, then in time order */
    size_t occurrence_count;
} StandardFormMotiveFamily;

typedef struct {
    StandardFormMotiveFamily *families; /* owned; in order of their prototype, longer first; NULL when none */
    size_t family_count;
} StandardFormMotives;

/* Finds the motives. phrases, segmentation (the ideas, for the start rule and the containment links) and themes (the continuation spans
   searched first for fragments) are the form adapter's outputs; themes may be NULL. config may be NULL for the defaults. `matcher` is
   required (NO_DETECTOR otherwise). On OK, *out is owned by the caller (standard_form_motives_destroy). */
StandardFormAnalysisStatus standard_form_find_motives(const StandardFormInput *input, const StandardFormPhrases *phrases,
                                                      const StandardFormSegmentation *segmentation,
                                                      const StandardFormThemes *themes, const StandardFormConfig *config,
                                                      StandardFormWindowMatcher matcher, void *matcher_context,
                                                      StandardFormMotives *out);
void standard_form_motives_destroy(StandardFormMotives *motives);

bool standard_form_motive_claim_read(const Interpretation *interpretation, StandardFormMotiveClaim *out);
bool standard_form_motive_occurrence_claim_read(const Interpretation *interpretation,
                                                StandardFormMotiveOccurrenceClaim *out);

#endif
