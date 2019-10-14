#ifndef MUSIC_TRANSFORM_TRANSFORM_RESULT_H
#define MUSIC_TRANSFORM_TRANSFORM_RESULT_H

#include <stddef.h>

#include "events/note_event.h"

/*
 * TransformResult (Category / base type): the shared output shape every
 * concrete Transformation subtype's apply function (Transposition,
 * Inversion, Retrograde, ...) returns -- established here once, per
 * transformation.h's apply-dispatch contract, rather than each subtype
 * inventing its own output shape.
 *
 * events is a required, owned array-of-NoteEvent-pointers-plus-count
 * (count may be 0: a transformation that legitimately produces no
 * output events -- e.g. Contraction removing an entire declared
 * sub-range -- still returns a valid, empty TransformResult, the same
 * "found nothing is not an error" treatment CandidateSet gives its own
 * candidates array, generation/candidate.h).
 *
 * Every NoteEvent in events already carries its own required
 * ProvenanceReference (events/note_event.h) pointing back to whatever
 * it was derived from -- TransformResult does not duplicate that at the
 * wrapper level. This is what satisfies the base Transformation
 * contract's "output is new objects with correct Provenance" (TDD
 * §140): the requirement is enforced per-event, by each subtype
 * building its output NoteEvents with real ProvenanceReferences, not by
 * this wrapper.
 *
 * A one-to-one transformation (Transposition, Inversion, Augmentation,
 * Diminution, Displacement) returns a TransformResult with exactly one
 * event. A one-to-many transformation (Ornamentation) returns one with
 * several. A sequence-level transformation (Retrograde, Fragmentation,
 * Expansion, Contraction) returns the resulting NoteEvent sequence in
 * full, not just the changed subset -- the caller does not need to
 * reconcile a partial result against the original sequence.
 *
 * TransformResult never mutates the NoteEvents given to it, and no
 * concrete subtype's apply function may mutate its input NoteEvent(s)
 * either (TDD §140 referential transparency, the same rule Analyzer and
 * CandidateEvaluator follow) -- every event in a TransformResult is a
 * newly constructed NoteEvent, never a pointer aliasing an input event.
 */

typedef struct TransformResult TransformResult;

typedef enum {
    TRANSFORM_RESULT_OK = 0,
    TRANSFORM_RESULT_ERR_NULL_EVENT,
    TRANSFORM_RESULT_ERR_ALLOC
} TransformResultResult;

/*
 * Takes ownership of each NoteEvent in events (if any) on success (all
 * destroyed by transform_result_destroy). events/count are optional --
 * pass NULL with count == 0 for a transformation that produced no
 * output events. No entry of a non-empty array may be NULL.
 */
TransformResultResult transform_result_create(NoteEvent **events, size_t count, TransformResult **out);
void transform_result_destroy(TransformResult *result);

size_t transform_result_event_count(const TransformResult *result);
const NoteEvent *transform_result_event_at(const TransformResult *result, size_t index);

#endif