#include "semantics/sonority_builder.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

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

static void assert_sonority_ids(const Sonority *sonority, const char *const *expected_ids, size_t expected_count) {
    assert(sonority_event_id_count(sonority) == expected_count);
    for (size_t i = 0; i < expected_count; i++) {
        assert(strcmp(entity_id_value(sonority_event_id_at(sonority, i)), expected_ids[i]) == 0);
    }
}

/*
 * Arpeggiated single-line input (the target texture for this branch,
 * matching tests/fixtures/arpeggiation-investigation/Bach_Prelude_C_Major.mid's
 * shape): three notes with staggered, overlapping onset/release
 * intervals produce one Sonority per beat-grid sample point where at
 * least one note is active, each carrying exactly the notes active at
 * that beat --
 *
 *   note-a: [0/4, 2/4)   note-b: [1/4, 3/4)   note-c: [2/4, 3/4)
 *
 *   beat 0/4: {note-a}
 *   beat 1/4: {note-a, note-b}
 *   beat 2/4: {note-b, note-c}
 */
static void test_build_from_notes_arpeggiated(void) {
    NoteEvent *note_a = make_note("note-a", 0, 2);
    NoteEvent *note_b = make_note("note-b", 1, 2);
    NoteEvent *note_c = make_note("note-c", 2, 1);
    NoteEvent *notes[3] = {note_a, note_b, note_c};

    Sonority **sonorities;
    size_t sonority_count;
    assert(sonority_builder_build_from_notes(notes, 3, &sonorities, &sonority_count) == SONORITY_BUILDER_OK);
    assert(sonority_count == 3);

    const char *beat0[] = {"note-a"};
    assert_sonority_ids(sonorities[0], beat0, 1);

    const char *beat1[] = {"note-a", "note-b"};
    assert_sonority_ids(sonorities[1], beat1, 2);

    const char *beat2[] = {"note-b", "note-c"};
    assert_sonority_ids(sonorities[2], beat2, 2);

    for (size_t i = 0; i < sonority_count; i++) {
        sonority_destroy(sonorities[i]);
    }
    free(sonorities);
    note_event_destroy(note_a);
    note_event_destroy(note_b);
    note_event_destroy(note_c);
}

/*
 * A note active only from beat 2/4 onward produces exactly one Sonority
 * (for beat 2/4) -- the earlier, silent grid points (0/4, 1/4) are
 * skipped entirely, not emitted as empty Sonorities (Sonority requires
 * event_id_count > 0, semantics/sonority.h).
 */
static void test_empty_sample_points_produce_no_sonority(void) {
    NoteEvent *note = make_note("note-only", 2, 1);
    NoteEvent *notes[1] = {note};

    Sonority **sonorities;
    size_t sonority_count;
    assert(sonority_builder_build_from_notes(notes, 1, &sonorities, &sonority_count) == SONORITY_BUILDER_OK);
    assert(sonority_count == 1);

    const char *beat2[] = {"note-only"};
    assert_sonority_ids(sonorities[0], beat2, 1);

    sonority_destroy(sonorities[0]);
    free(sonorities);
    note_event_destroy(note);
}

int main(void) {
    test_note_active_at_boundaries();
    test_overlapping_notes();
    test_build_from_notes_arpeggiated();
    test_empty_sample_points_produce_no_sonority();
    return 0;
}
