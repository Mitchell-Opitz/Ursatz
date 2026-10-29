#ifndef MUSIC_SEMANTICS_SONORITY_BUILDER_H
#define MUSIC_SEMANTICS_SONORITY_BUILDER_H

/*
 * sonority_builder_build_from_notes is not the sonority segmentation owner
 * (sonority_builder_build_adaptive). Kept as the fixed-grid primitive: a caller-given grid is a
 * different job (docs/ARCHITECTURE.md, Analytic job owners).
 */

#include <stdbool.h>
#include <stddef.h>

#include "events/note_event.h"
#include "semantics/sonority.h"
#include "time/musical_time.h"

/*
 * SonorityBuilder: derives Sonority objects (semantics/sonority.h) from
 * raw NoteEvents by onset/release activity tracking, so chord
 * identification (ursatz_chain/subset_chord_analyzer.h) has a real Sonority
 * to consume instead of requiring the caller to hand-assemble one.
 *
 * SCOPE (System Registry, Problem A only): each NoteEvent is modeled as
 * active over the interval [onset, onset + duration) -- left-closed,
 * right-open, the same boundary treatment TIME_SPAN_BOUNDARY_LEFT_CLOSED
 * already uses elsewhere in this codebase (queryable via
 * sonority_builder_note_active_at below) -- and the grid this builder
 * samples groups notes by ATTACK: each beat's Sonority is formed from
 * every note whose onset falls within that beat's half-open span [beat,
 * beat + 1/4) (fixed to a quarter note, MusicalTime 1/4, whole-note-
 * relative, matching this codebase's existing whole-note-per-measure
 * MusicalTime convention). A note still ringing (per its own onset-
 * release interval) into a LATER beat is not carried forward into that
 * later beat's Sonority: in practice a note's duration commonly exceeds
 * one beat (e.g. a held bass tone under faster-moving upper voices), and
 * doing so would accumulate it into every later beat for as long as it
 * rings, contaminating each with a stale tone instead of that beat's own
 * attack content -- attack-grouping is what lets chord identification
 * read off each beat's own harmony. This is a stated v1 default, not a
 * general/adaptive harmonic-rhythm detector: it does not infer
 * chord-change points, does not vary its grid by texture or tempo, and
 * the beat unit is not a parameter -- see docs/modules/semantics.md. A
 * different beat unit is new scope, to be added when a real use case
 * demands it, not now.
 *
 * PARTIALLY HANDLED (Problem B, was deferred): non-chord-tone tolerance.
 * This builder itself still does no filtering -- every note active at
 * the sample point is included as-is, passing/neighbor tones included.
 * The tolerance instead lives one layer up, in
 * frameworks/common-practice-minimal/triad_identification.c and
 * seventh_identification.c: those now match by subset containment
 * rather than exact degree/interval-set equality, so a single extra
 * non-chord-tone degree in a beat's Sonority no longer blocks
 * identification of the underlying triad/seventh. This is a stopgap,
 * not real non-chord-tone filtering (see Known_Gaps.md): it only
 * tolerates one simultaneous non-chord degree, ties are resolved by
 * naive scan order and only logged (not surfaced) when a beat's degree
 * set satisfies more than one candidate, and dense multi-voice textures
 * (melody + accompaniment together) still frequently produce no match
 * at all -- see tests/analysis/test_framework_v1_corpus_subset_match_report.c
 * for real per-beat results against the wider corpus. Built and
 * validated against solo/monophonic-texture writing
 * (tests/fixtures/arpeggiation-investigation/Bach_Prelude_C_Major.mid);
 * the subset-match stopgap was added for the denser Field 1 fixture
 * (tests/fixtures/framework-v1-corpus/Field 1.mid).
 *
 * A sample point with zero active notes produces no Sonority (Sonority
 * requires event_id_count > 0, semantics/sonority.h) -- silence between
 * notes is a legitimate gap, not an error.
 */

typedef enum {
    SONORITY_BUILDER_OK = 0,
    SONORITY_BUILDER_ERR_NULL_NOTES,
    SONORITY_BUILDER_ERR_NULL_NOTE,
    SONORITY_BUILDER_ERR_ALLOC
} SonorityBuilderResult;

/*
 * True iff note is active (onset <= at < onset + duration) at the given
 * MusicalTime position. note and at must be non-NULL.
 */
bool sonority_builder_note_active_at(const NoteEvent *note, const MusicalTime *at);

/*
 * Builds one Sonority per non-empty fixed-beat-grid sample point covering
 * notes[0..note_count), each carrying (as event_ids) a fresh EntityID
 * clone of every NoteEvent onsetting within that beat, an unrestricted
 * voice set (voice_id_count == 0 -- this builder does not consume
 * NoteEvent's optional_voice), and SONORITY_EVENT_DOMAIN_SOUNDING (notes
 * are treated as already-realized/sounding activity, matching a
 * MIDI-import source).
 *
 * notes must be non-NULL with note_count > 0; no entry may be NULL.
 * Writes a newly allocated Sonority* array to *out_sonorities with
 * *out_sonority_count entries. Caller owns the array and every Sonority
 * in it (destroy each with sonority_destroy, then free the array).
 */
SonorityBuilderResult sonority_builder_build_from_notes(NoteEvent *const *notes, size_t note_count,
                                                          Sonority ***out_sonorities, size_t *out_sonority_count);

#endif
