#include "semantics/sonority_builder.h"

#include <assert.h>
#include <stddef.h>

#include "core/provenance_reference.h"
#include "events/note_event.h"
#include "identity/entity_id.h"
#include "pitch/pitch_identity.h"
#include "time/duration.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"

static EntityID *make_id(const char *value) {
    EntityID *id = NULL;
    entity_id_create(value, &id);
    return id;
}

static TemporalAnchor *make_temporal_anchor(long numerator, unsigned long denominator) {
    MusicalTime *position = NULL;
    musical_time_create(numerator, denominator, &position);
    TemporalAnchor *anchor = NULL;
    temporal_anchor_create(position, &anchor);
    return anchor;
}

static Duration *make_duration(long numerator, unsigned long denominator) {
    Duration *duration = NULL;
    duration_create(numerator, denominator, &duration);
    return duration;
}

static PitchIdentity *make_pitch(const char *token) {
    PitchIdentity *pitch = NULL;
    pitch_identity_create(token, 8, &pitch);
    return pitch;
}

static ProvenanceReference *make_absent_provenance(void) {
    ProvenanceReference *provenance = NULL;
    provenance_reference_create_absent(&provenance);
    return provenance;
}

/* onset = onset_num/4, duration = duration_num/4 (quarter-note units, matching the builder's fixed beat grid). */
static NoteEvent *make_note(const char *id_value, long onset_num, long duration_num) {
    NoteEvent *note_event = NULL;
    assert(note_event_create(make_id(id_value), make_temporal_anchor(onset_num, 4), make_duration(duration_num, 4),
                              make_pitch("pitch-a"), NULL, NULL, make_absent_provenance(),
                              &note_event) == NOTE_EVENT_OK);
    return note_event;
}

static MusicalTime *make_at(long numerator, unsigned long denominator) {
    MusicalTime *at = NULL;
    musical_time_create(numerator, denominator, &at);
    return at;
}

/*
 * Golden test: a single note spanning [1/4, 3/4) (onset at beat 1,
 * duration of two beats) is active exactly at its onset, mid-sustain,
 * and the instant before release -- and inactive before onset and at/
 * after release. Boundary is left-closed/right-open, matching
 * TIME_SPAN_BOUNDARY_LEFT_CLOSED's treatment elsewhere in this codebase.
 */
static void test_note_active_at_boundaries(void) {
    NoteEvent *note = make_note("note-a", 1, 2);

    MusicalTime *before_onset = make_at(0, 4);
    assert(!sonority_builder_note_active_at(note, before_onset));
    musical_time_destroy(before_onset);

    MusicalTime *at_onset = make_at(1, 4);
    assert(sonority_builder_note_active_at(note, at_onset));
    musical_time_destroy(at_onset);

    MusicalTime *mid_sustain = make_at(2, 4);
    assert(sonority_builder_note_active_at(note, mid_sustain));
    musical_time_destroy(mid_sustain);

    MusicalTime *just_before_release = make_at(11, 16);
    assert(sonority_builder_note_active_at(note, just_before_release));
    musical_time_destroy(just_before_release);

    MusicalTime *at_release = make_at(3, 4);
    assert(!sonority_builder_note_active_at(note, at_release));
    musical_time_destroy(at_release);

    MusicalTime *after_release = make_at(1, 1);
    assert(!sonority_builder_note_active_at(note, after_release));
    musical_time_destroy(after_release);

    note_event_destroy(note);
}

/*
 * Two overlapping notes: note-a spans [0, 2/4), note-b spans [1/4, 3/4).
 * At beat 1/4, both are active; before and after the overlap, only one
 * (or neither) is.
 */
static void test_overlapping_notes(void) {
    NoteEvent *note_a = make_note("note-a", 0, 2);
    NoteEvent *note_b = make_note("note-b", 1, 2);

    MusicalTime *before_overlap = make_at(0, 4);
    assert(sonority_builder_note_active_at(note_a, before_overlap));
    assert(!sonority_builder_note_active_at(note_b, before_overlap));
    musical_time_destroy(before_overlap);

    MusicalTime *during_overlap = make_at(1, 4);
    assert(sonority_builder_note_active_at(note_a, during_overlap));
    assert(sonority_builder_note_active_at(note_b, during_overlap));
    musical_time_destroy(during_overlap);

    MusicalTime *after_a_releases = make_at(2, 4);
    assert(!sonority_builder_note_active_at(note_a, after_a_releases));
    assert(sonority_builder_note_active_at(note_b, after_a_releases));
    musical_time_destroy(after_a_releases);

    note_event_destroy(note_a);
    note_event_destroy(note_b);
}

int main(void) {
    test_note_active_at_boundaries();
    test_overlapping_notes();
    return 0;
}
