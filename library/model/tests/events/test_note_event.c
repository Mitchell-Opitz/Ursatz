#include "model/events/note_event.h"

#include <assert.h>
#include <stddef.h>

static EntityID *make_id(const char *value) {
    EntityID *id = NULL;
    entity_id_create(value, &id);
    return id;
}

static MusicalTime make_onset(long long numerator, long long denominator) {
    MusicalTime onset;
    assert(musical_time_make(numerator, denominator, &onset) == MUSICAL_TIME_OK);
    return onset;
}

static Duration make_duration(long long numerator, long long denominator) {
    Duration duration;
    assert(duration_make(numerator, denominator, &duration) == DURATION_OK);
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
    note_event_create(make_id(id_value), make_onset(1, 4), make_duration(1, 4), make_pitch("pitch-a"),
                       NULL, NULL, make_absent_provenance(), &note_event);
    return note_event;
}

/* Every failure path below returns before note_event_create takes
   ownership of anything (src/events/note_event.c validates id, pitch and
   provenance before any assignment), so every pointer argument that WAS
   successfully constructed for a given call -- even ones that are not the
   null one under test -- is still owned by the caller and must be
   destroyed here. The onset and the duration are values: nothing to free. */

static void test_create_rejects_null_id(void) {
    PitchIdentity *pitch = make_pitch("pitch-a");
    ProvenanceReference *provenance = make_absent_provenance();

    NoteEvent *note_event = NULL;
    NoteEventResult r =
        note_event_create(NULL, make_onset(1, 4), make_duration(1, 4), pitch, NULL, NULL, provenance, &note_event);
    assert(r == NOTE_EVENT_ERR_NULL_ID);
    assert(note_event == NULL);

    pitch_identity_destroy(pitch);
    provenance_reference_destroy(provenance);
}

static void test_create_rejects_null_pitch(void) {
    EntityID *id = make_id("note-1");
    ProvenanceReference *provenance = make_absent_provenance();

    NoteEvent *note_event = NULL;
    NoteEventResult r =
        note_event_create(id, make_onset(1, 4), make_duration(1, 4), NULL, NULL, NULL, provenance, &note_event);
    assert(r == NOTE_EVENT_ERR_NULL_PITCH);
    assert(note_event == NULL);

    entity_id_destroy(id);
    provenance_reference_destroy(provenance);
}

/* Provenance state must be explicitly present or absent -- never silently
   omitted (C6). A hand-authored NoteEvent may use state=absent, but the
   provenance argument itself is never optional. */
static void test_create_rejects_null_provenance(void) {
    EntityID *id = make_id("note-1");
    PitchIdentity *pitch = make_pitch("pitch-a");

    NoteEvent *note_event = NULL;
    NoteEventResult r =
        note_event_create(id, make_onset(1, 4), make_duration(1, 4), pitch, NULL, NULL, NULL, &note_event);
    assert(r == NOTE_EVENT_ERR_NULL_PROVENANCE);
    assert(note_event == NULL);

    entity_id_destroy(id);
    pitch_identity_destroy(pitch);
}

static void test_create_accepts_minimal_valid(void) {
    NoteEvent *note_event = make_minimal_note_event("note-1");
    assert(note_event != NULL);
    /* The event builds its own anchor at the onset and holds the duration as given. */
    assert(musical_time_same(*temporal_anchor_position(note_event_temporal_anchor(note_event)), make_onset(1, 4)));
    assert(duration_same(*note_event_duration(note_event), make_duration(1, 4)));
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
    NoteEventResult r = note_event_create(make_id("note-1"), make_onset(1, 4), make_duration(1, 4),
                                           make_pitch("pitch-a"), spelling, voice, provenance, &note_event);
    assert(r == NOTE_EVENT_OK);
    assert(note_event_optional_pitch_spelling(note_event) != NULL);
    assert(note_event_optional_voice(note_event) != NULL);
    assert(provenance_reference_state(note_event_provenance(note_event)) == PROVENANCE_REFERENCE_PRESENT);

    note_event_destroy(note_event);
}

/* duration >= 0 is Duration's own invariant (time/duration.h rejects a
   negative value at duration_make); a caller cannot construct a
   negative Duration to pass into note_event_create, so the invariant
   holds transitively without NoteEvent re-checking it. */
static void test_duration_invariant_enforced_by_duration_type(void) {
    Duration negative;
    DurationResult r = duration_make(-1, 4, &negative);
    assert(r == DURATION_ERR_NEGATIVE);
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
    note_event_create(make_id("note-1"), make_onset(1, 4), make_duration(1, 4), make_pitch("pitch-a"),
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

int main(void) {
    test_create_rejects_null_id();
    test_create_rejects_null_pitch();
    test_create_rejects_null_provenance();
    test_create_accepts_minimal_valid();
    test_create_accepts_all_optional_fields_present();
    test_duration_invariant_enforced_by_duration_type();
    test_optional_voice_exposes_only_opaque_reference();
    test_id_stable_under_round_trip();
    return 0;
}