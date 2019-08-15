#include "events/note_event.h"

#include <assert.h>
#include <stddef.h>

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

static NoteEvent *make_minimal_note_event(const char *id_value) {
    NoteEvent *note_event = NULL;
    note_event_create(make_id(id_value), make_temporal_anchor(1, 4), make_duration(1, 4), make_pitch("pitch-a"),
                       NULL, NULL, make_absent_provenance(), &note_event);
    return note_event;
}

/* Every failure path below returns before note_event_create takes
   ownership of anything (src/events/note_event.c validates id,
   temporal_anchor, duration, pitch, provenance in that order before any
   assignment), so every argument that WAS successfully constructed for
   a given call -- even ones that are not the null one under test -- is
   still owned by the caller and must be destroyed here. */

static void test_create_rejects_null_id(void) {
    TemporalAnchor *temporal_anchor = make_temporal_anchor(1, 4);
    Duration *duration = make_duration(1, 4);
    PitchIdentity *pitch = make_pitch("pitch-a");
    ProvenanceReference *provenance = make_absent_provenance();

    NoteEvent *note_event = NULL;
    NoteEventResult r =
        note_event_create(NULL, temporal_anchor, duration, pitch, NULL, NULL, provenance, &note_event);
    assert(r == NOTE_EVENT_ERR_NULL_ID);
    assert(note_event == NULL);

    temporal_anchor_destroy(temporal_anchor);
    duration_destroy(duration);
    pitch_identity_destroy(pitch);
    provenance_reference_destroy(provenance);
}

static void test_create_rejects_null_temporal_anchor(void) {
    EntityID *id = make_id("note-1");
    Duration *duration = make_duration(1, 4);
    PitchIdentity *pitch = make_pitch("pitch-a");
    ProvenanceReference *provenance = make_absent_provenance();

    NoteEvent *note_event = NULL;
    NoteEventResult r = note_event_create(id, NULL, duration, pitch, NULL, NULL, provenance, &note_event);
    assert(r == NOTE_EVENT_ERR_NULL_TEMPORAL_ANCHOR);
    assert(note_event == NULL);

    entity_id_destroy(id);
    duration_destroy(duration);
    pitch_identity_destroy(pitch);
    provenance_reference_destroy(provenance);
}

static void test_create_rejects_null_duration(void) {
    EntityID *id = make_id("note-1");
    TemporalAnchor *temporal_anchor = make_temporal_anchor(1, 4);
    PitchIdentity *pitch = make_pitch("pitch-a");
    ProvenanceReference *provenance = make_absent_provenance();

    NoteEvent *note_event = NULL;
    NoteEventResult r = note_event_create(id, temporal_anchor, NULL, pitch, NULL, NULL, provenance, &note_event);
    assert(r == NOTE_EVENT_ERR_NULL_DURATION);
    assert(note_event == NULL);

    entity_id_destroy(id);
    temporal_anchor_destroy(temporal_anchor);
    pitch_identity_destroy(pitch);
    provenance_reference_destroy(provenance);
}

static void test_create_rejects_null_pitch(void) {
    EntityID *id = make_id("note-1");
    TemporalAnchor *temporal_anchor = make_temporal_anchor(1, 4);
    Duration *duration = make_duration(1, 4);
    ProvenanceReference *provenance = make_absent_provenance();

    NoteEvent *note_event = NULL;
    NoteEventResult r = note_event_create(id, temporal_anchor, duration, NULL, NULL, NULL, provenance, &note_event);
    assert(r == NOTE_EVENT_ERR_NULL_PITCH);
    assert(note_event == NULL);

    entity_id_destroy(id);
    temporal_anchor_destroy(temporal_anchor);
    duration_destroy(duration);
    provenance_reference_destroy(provenance);
}

/* Provenance state must be explicitly present or absent -- never silently
   omitted (C6). A hand-authored NoteEvent may use state=absent, but the
   provenance argument itself is never optional. */
static void test_create_rejects_null_provenance(void) {
    EntityID *id = make_id("note-1");
    TemporalAnchor *temporal_anchor = make_temporal_anchor(1, 4);
    Duration *duration = make_duration(1, 4);
    PitchIdentity *pitch = make_pitch("pitch-a");

    NoteEvent *note_event = NULL;
    NoteEventResult r = note_event_create(id, temporal_anchor, duration, pitch, NULL, NULL, NULL, &note_event);
    assert(r == NOTE_EVENT_ERR_NULL_PROVENANCE);
    assert(note_event == NULL);

    entity_id_destroy(id);
    temporal_anchor_destroy(temporal_anchor);
    duration_destroy(duration);
    pitch_identity_destroy(pitch);
}

static void test_create_accepts_minimal_valid(void) {
    NoteEvent *note_event = make_minimal_note_event("note-1");
    assert(note_event != NULL);
    assert(note_event_optional_pitch_spelling(note_event) == NULL);
    assert(note_event_optional_voice(note_event) == NULL);
    assert(provenance_reference_state(note_event_provenance(note_event)) == PROVENANCE_REFERENCE_ABSENT);
    note_event_destroy(note_event);
}

static void test_create_accepts_all_optional_fields_present(void) {
    PitchSpelling *spelling = NULL;
    pitch_spelling_create(PITCH_LETTER_C, 0, 4, &spelling);

    VoiceReference *voice = NULL;
    voice_reference_create(make_id("voice-1"), &voice);

    ProvenanceReference *provenance = NULL;
    provenance_reference_create_present(make_id("derivation-1"), &provenance);

    NoteEvent *note_event = NULL;
    NoteEventResult r = note_event_create(make_id("note-1"), make_temporal_anchor(1, 4), make_duration(1, 4),
                                           make_pitch("pitch-a"), spelling, voice, provenance, &note_event);
    assert(r == NOTE_EVENT_OK);
    assert(note_event_optional_pitch_spelling(note_event) != NULL);
    assert(note_event_optional_voice(note_event) != NULL);
    assert(provenance_reference_state(note_event_provenance(note_event)) == PROVENANCE_REFERENCE_PRESENT);

    note_event_destroy(note_event);
}

/* duration >= 0 is Duration's own invariant (time/duration.h rejects a
   negative numerator at duration_create); a caller cannot construct a
   negative Duration to pass into note_event_create, so the invariant
   holds transitively without NoteEvent re-checking it. */
static void test_duration_invariant_enforced_by_duration_type(void) {
    Duration *negative = NULL;
    DurationResult r = duration_create(-1, 4, &negative);
    assert(r == DURATION_ERR_NEGATIVE);
    assert(negative == NULL);
}

/* optional_voice, when present, is typed as VoiceReference* -- the same
   structural-gap enforcement PitchIdentity uses for its token
   (tests/pitch/test_pitch_identity.c): there is no C type representing a
   resolved Voice (L4) in this codebase to pass instead, so only the
   opaque referenced id is ever retrievable, never Voice fields. */
static void test_optional_voice_exposes_only_opaque_reference(void) {
    VoiceReference *voice = NULL;
    voice_reference_create(make_id("voice-1"), &voice);

    NoteEvent *note_event = NULL;
    note_event_create(make_id("note-1"), make_temporal_anchor(1, 4), make_duration(1, 4), make_pitch("pitch-a"),
                       NULL, voice, make_absent_provenance(), &note_event);

    const VoiceReference *stored_voice = note_event_optional_voice(note_event);
    EntityID *expected_target = make_id("voice-1");
    assert(entity_id_equals(voice_reference_target_id(stored_voice), expected_target));
    entity_id_destroy(expected_target);

    note_event_destroy(note_event);
}

/* id stability: extracting id's raw value and reconstructing an EntityID
   from it (the same "round trip" pattern as
   tests/identity/test_entity_id.c) yields an id equal to the original --
   id is never derived from position/array index anywhere in this
   module. */
static void test_id_stable_under_round_trip(void) {
    NoteEvent *note_event = make_minimal_note_event("note-3f9a2b");

    const char *serialized = entity_id_value(note_event_id(note_event));
    EntityID *restored = make_id(serialized);

    assert(entity_id_equals(note_event_id(note_event), restored));

    entity_id_destroy(restored);
    note_event_destroy(note_event);
}

static void test_equals_by_id_ignores_other_fields(void) {
    NoteEvent *a = NULL;
    note_event_create(make_id("note-1"), make_temporal_anchor(1, 4), make_duration(1, 4), make_pitch("pitch-a"),
                       NULL, NULL, make_absent_provenance(), &a);
    NoteEvent *b = NULL;
    note_event_create(make_id("note-1"), make_temporal_anchor(3, 4), make_duration(1, 2), make_pitch("pitch-b"),
                       NULL, NULL, make_absent_provenance(), &b);

    assert(note_event_equals_by_id(a, b));
    assert(!note_event_equals_by_value(a, b));

    note_event_destroy(a);
    note_event_destroy(b);
}

static void test_equals_by_value_ignores_id(void) {
    NoteEvent *a = NULL;
    note_event_create(make_id("note-1"), make_temporal_anchor(1, 4), make_duration(1, 4), make_pitch("pitch-a"),
                       NULL, NULL, make_absent_provenance(), &a);
    NoteEvent *b = NULL;
    note_event_create(make_id("note-2"), make_temporal_anchor(1, 4), make_duration(1, 4), make_pitch("pitch-a"),
                       NULL, NULL, make_absent_provenance(), &b);

    assert(!note_event_equals_by_id(a, b));
    assert(note_event_equals_by_value(a, b));

    note_event_destroy(a);
    note_event_destroy(b);
}

int main(void) {
    test_create_rejects_null_id();
    test_create_rejects_null_temporal_anchor();
    test_create_rejects_null_duration();
    test_create_rejects_null_pitch();
    test_create_rejects_null_provenance();
    test_create_accepts_minimal_valid();
    test_create_accepts_all_optional_fields_present();
    test_duration_invariant_enforced_by_duration_type();
    test_optional_voice_exposes_only_opaque_reference();
    test_id_stable_under_round_trip();
    test_equals_by_id_ignores_other_fields();
    test_equals_by_value_ignores_id();
    return 0;
}