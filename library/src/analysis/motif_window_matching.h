#ifndef MUSIC_ANALYSIS_MOTIF_WINDOW_MATCHING_H
#define MUSIC_ANALYSIS_MOTIF_WINDOW_MATCHING_H

/*
 * Not the motive matching owner (motive_variant_recognize with the motive_relation_apply relations).
 * Kept as the exact contour and rhythm primitive, injected into standard-form through the chain bridge
 * in P22 (docs/ARCHITECTURE.md, Analytic job owners).
 */

#include <stddef.h>

#include "identity/entity_id.h"
#include "semantics/contour.h"
#include "semantics/motif_window.h"
#include "semantics/rhythmic_pattern.h"

/* A window that matched at least one other: its event ids (borrowed) and the confidence of the match. */
typedef struct {
    const EntityID *const *matched_event_ids;
    size_t matched_event_id_count;
    unsigned long confidence_numerator;
    unsigned long confidence_denominator;
} MotifWindowOccurrence;

/*
 * MotifWindowMatching (Procedure, Registry #20): finds candidate-motif windows
 * that repeat elsewhere in a piece by comparing each window's Contour
 * direction sequence (semantics/contour.h) and RhythmicPattern
 * duration sequence (semantics/rhythmic_pattern.h) for exact equality.
 *
 * Scope boundary (same one IntervalAnalysis/VoiceLeadingAnalysis/
 * CadenceClassification already hit): PitchIdentity carries no numeric
 * semantics, so this module cannot compute pitch direction or interval
 * distance itself. It takes windows whose Contour/RhythmicPattern have
 * already been built by the caller (from real pitch/duration data
 * under whatever tuning/notation the caller has) and compares those
 * already-observed sequences. Cutting a NoteEvent stream into
 * candidate windows is likewise the caller's job -- this module has no
 * opinion on window size or boundary placement, only on whether two
 * given windows match.
 *
 * Match rule (v1, deliberately narrow -- exact match only, no
 * inversion/retrograde/fragmentation, no pitch-interval math):
 * two windows match iff their Contour direction sequences are equal
 * element-for-element (contour_direction_at) AND their RhythmicPattern
 * duration sequences are equal element-for-element (duration_equals).
 * Equal Contour directions is transposition-invariant by construction
 * (a transposition preserves relative up/down/same), so this single
 * rule covers both exact repetition and transposition without any
 * numeric pitch math -- direction and rhythm equality are both
 * required, so same-rhythm-different-shape or same-shape-different-
 * rhythm windows are correctly rejected.
 *
 * Design default (flagged, not a Registry-settled rule): a motif
 * requires at least two occurrences by definition, so a window with no
 * match anywhere else in the input produces no occurrence at all. This
 * module does not attempt to distinguish "coincidental" repeats from
 * intentional ones beyond that -- any two-or-more-window exact match
 * is reported, at confidence 1/1 (an exact match under this rule has no
 * partial-credit dimension to score).
 */

/* MotifWindow is defined in semantics/motif_window.h (one window value, shared with standard-form). */

typedef enum {
    MOTIF_WINDOW_MATCHING_OK = 0,
    MOTIF_WINDOW_MATCHING_ERR_NULL_WINDOWS,
    MOTIF_WINDOW_MATCHING_ERR_NULL_WINDOW,
    MOTIF_WINDOW_MATCHING_ERR_ALLOC
} MotifWindowMatchingResult;

/*
 * Partial comparison of an aligned stretch of two windows, for callers that
 * look for a motif inside longer windows or allow variation (ThematicReturn-
 * Detection). Compares `length` consecutive events of `a` starting at
 * a_offset with `length` of `b` starting at b_offset: contour_score is the
 * fraction of the length - 1 contour directions that are equal,
 * rhythm_score the fraction of the length durations that are equal, each
 * 0 to 1. Both are 1 exactly when the stretches match under the exact rule
 * above. length must be at least 2 and both stretches must lie inside their
 * windows' contours and rhythms; otherwise
 * MOTIF_WINDOW_MATCHING_ERR_NULL_WINDOW is returned and *out is untouched.
 */
typedef struct {
    double contour_score;
    double rhythm_score;
} MotifWindowSimilarity;

MotifWindowMatchingResult motif_window_matching_compare(const MotifWindow *a, size_t a_offset, const MotifWindow *b,
                                                          size_t b_offset, size_t length,
                                                          MotifWindowSimilarity *out);

/*
 * windows/window_count MUST be non-NULL/non-zero if window_count > 0;
 * window_count == 0 is valid and yields *out_occurrences = NULL,
 * *out_occurrence_count = 0. No entry of windows may have a NULL
 * contour, rhythm, or event_ids, or a zero event_id_count.
 *
 * On success, *out_occurrences is a freshly allocated array (caller
 * frees with motif_window_matching_destroy_occurrences) of one
 * MotifWindowOccurrence per window that matched at least one other
 * window, in input order; each occurrence's matched_event_ids/
 * matched_event_id_count borrow directly from the corresponding
 * window's event_ids (no copy), and confidence is fixed at 1/1.
 * *out_occurrence_count may be 0 (no window matched any other) with
 * *out_occurrences left NULL.
 */
MotifWindowMatchingResult motif_window_matching_find(const MotifWindow *windows, size_t window_count,
                                                       MotifWindowOccurrence **out_occurrences,
                                                       size_t *out_occurrence_count);

/* Frees the array returned by motif_window_matching_find. Does not touch the
   borrowed event_ids each occurrence points at -- those remain owned by
   whatever owns the original MotifWindow array. */
void motif_window_matching_destroy_occurrences(MotifWindowOccurrence *occurrences);

#endif
