#ifndef MUSIC_ANALYSIS_SENTENCE_CONTINUATION_MATCHING_H
#define MUSIC_ANALYSIS_SENTENCE_CONTINUATION_MATCHING_H

#include <stdbool.h>

#include "analysis/motif_window_matching.h"

/*
 * SentenceContinuationMatching (Procedure, feeds SentenceDetection):
 * determines whether a candidate window is a shortened/fragmented
 * continuation of a statement window -- the third part of period/
 * sentence theory's Sentence structure (statement, repetition,
 * continuation), following the statement/repetition pair
 * MotifWindowMatching (analysis/motif_window_matching.h) already
 * detects via exact full-window equality.
 *
 * Not a reuse of the Fragmentation transform (transform/fragmentation.h):
 * Fragmentation only clones a caller-supplied set of extracted_event_ids
 * out of a source Structure's events -- it has no comparison/matching
 * logic of its own, so it cannot itself decide whether a given tail IS a
 * fragment of the statement. This module supplies that missing
 * comparison, extending MotifWindowMatching's exact-equality rule to a
 * variable-length prefix match instead.
 *
 * Match rule (v1, flagged design default per requirement 3, confirmed
 * against Contour's actual semantics -- Contour (semantics/contour.h)
 * carries direction only (up/down/same), never pitch, so equal
 * direction sequences are transposition-invariant by construction the
 * same way MotifWindowMatching's own doc comment already establishes;
 * a transposed continuation is matched by this rule with no special
 * case needed): a candidate window is a continuation of a statement
 * window iff:
 *   - candidate->event_id_count >= SENTENCE_CONTINUATION_MIN_EVENT_COUNT
 *     (2 -- avoids a single-note match counting as "continuation");
 *   - candidate->event_id_count < statement->event_id_count (strictly
 *     shorter -- a continuation is a shortened statement, not an equal
 *     or longer one, which is MotifWindowMatching's job instead);
 *   - candidate's Contour direction sequence equals, element-for-
 *     element, the statement's Contour direction sequence over the same
 *     (shorter) length -- a direction-sequence prefix match; and
 *   - candidate's RhythmicPattern duration sequence equals, element-
 *     for-element, the statement's RhythmicPattern duration sequence
 *     over that same length -- a duration-sequence prefix match, the
 *     same both-dimensions-required discipline MotifWindowMatching uses
 *     for its own exact match.
 */

#define SENTENCE_CONTINUATION_MIN_EVENT_COUNT 2

/*
 * statement and candidate MUST be non-NULL with non-NULL contour/rhythm/
 * event_ids and a positive event_id_count (the same shape
 * MotifWindowMatching's windows require) -- returns false for any
 * window failing that shape, never guessed at.
 */
bool sentence_continuation_matching_is_continuation(const MotifWindow *statement, const MotifWindow *candidate);

#endif
