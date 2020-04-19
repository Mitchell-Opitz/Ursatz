#ifndef MUSIC_SEMANTICS_SONORITY_BUILDER_H
#define MUSIC_SEMANTICS_SONORITY_BUILDER_H

#include <stdbool.h>
#include <stddef.h>

#include "events/note_event.h"
#include "semantics/sonority.h"
#include "time/musical_time.h"

/*
 * SonorityBuilder: derives Sonority objects (semantics/sonority.h) from
 * raw NoteEvents by onset/release activity tracking, so chord
 * identification (analysis/chord_identification.h) has a real Sonority
 * to consume instead of requiring the caller to hand-assemble one.
 *
 * SCOPE (System Registry, Problem A only): each NoteEvent is modeled as
 * active over the interval [onset, onset + duration) -- left-closed,
 * right-open, the same boundary treatment TIME_SPAN_BOUNDARY_LEFT_CLOSED
 * already uses elsewhere in this codebase -- and that activity is
 * sampled at a FIXED one-sample-per-beat grid, beat hardcoded to a
 * quarter note (MusicalTime 1/4, whole-note-relative, matching this
 * codebase's existing whole-note-per-measure MusicalTime convention).
 * This is a stated v1 default, not a general/adaptive harmonic-rhythm
 * detector: it does not infer chord-change points, does not vary its
 * grid by texture or tempo, and the beat unit is not a parameter -- see
 * docs/modules/semantics.md. A different beat unit is new scope, to be
 * added when a real use case demands it, not now.
 *
 * OUT OF SCOPE (Problem B, deferred): non-chord-tone tolerance and
 * multi-voice/dense-texture handling. A note active at a sample point
 * that is actually a passing/neighbor tone is not filtered out -- every
 * note active at the sample point is included as-is. Built and
 * validated against solo/monophonic-texture writing only
 * (tests/fixtures/arpeggiation-investigation/Bach_Prelude_C_Major.mid).
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
 * clone of every NoteEvent active at that sample point, an unrestricted
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
