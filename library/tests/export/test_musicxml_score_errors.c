#include "score_test_util.h"

/* What musicxml_export_write_score refuses: each failing call writes nothing and leaks nothing. */

#define SENTINEL ((char *)(uintptr_t)0x1)

static MusicXmlExportResult write_failing(const MusicXmlExportScore *score) {
    char *xml = SENTINEL;
    MusicXmlExportResult result = musicxml_export_write_score(score, &xml);
    if (result != MUSICXML_EXPORT_OK) {
        assert(xml == SENTINEL); /* nothing written */
    } else {
        free(xml);
    }
    return result;
}

static NoteEvent *unspelled_note(void) {
    TemporalAnchor *anchor = NULL;
    assert(temporal_anchor_create(tu_time(0, 1), &anchor) == TEMPORAL_ANCHOR_OK);
    Duration *duration = NULL;
    assert(duration_create(1, 4, &duration) == DURATION_OK);
    PitchIdentity *pitch = NULL;
    unsigned char token[1] = {60};
    assert(pitch_identity_create(token, sizeof(token), &pitch) == PITCH_IDENTITY_OK);
    ProvenanceReference *provenance = NULL;
    assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);
    NoteEvent *event = NULL;
    assert(note_event_create(tu_id("bare"), anchor, duration, pitch, NULL, NULL, provenance, &event) == NOTE_EVENT_OK);
    return event;
}

static MusicXmlExportScore score_of(const TestBag *bag) {
    MusicXmlExportScore score;
    memset(&score, 0, sizeof(score));
    score.events = bag->events;
    score.count = bag->count;
    score.part_index = bag->part;
    score.staff = bag->staff;
    return score;
}

static void test_null_arguments(void) {
    char *xml = SENTINEL;
    MusicXmlExportScore empty;
    memset(&empty, 0, sizeof(empty));
    assert(musicxml_export_write_score(NULL, &xml) == MUSICXML_EXPORT_ERR_NULL_ARG && xml == SENTINEL);
    assert(musicxml_export_write_score(&empty, NULL) == MUSICXML_EXPORT_ERR_NULL_ARG);
}

static void test_missing_spelling(void) {
    NoteEvent *events[1] = {unspelled_note()};
    MusicXmlExportScore score;
    memset(&score, 0, sizeof(score));
    score.events = events;
    score.count = 1;
    assert(write_failing(&score) == MUSICXML_EXPORT_ERR_MISSING_SPELLING);
    note_event_destroy(events[0]);
}

/* Overlapping notes are written, not refused (test_musicxml_score_overlaps); in different voices, staves or parts
   they need no extra voice at all. */
static void test_overlap_is_per_voice(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    bag_add(&bag, tu_note("a", 0, 1, 1, 2, PITCH_LETTER_C, 0, 4, "v"), 0, 1);
    bag_add(&bag, tu_note("b", 1, 4, 1, 2, PITCH_LETTER_D, 0, 4, "w"), 0, 1);
    MusicXmlExportScore score = score_of(&bag);
    assert(write_failing(&score) == MUSICXML_EXPORT_OK);
    bag_free(&bag);

    memset(&bag, 0, sizeof(bag));
    bag_add(&bag, tu_note("a", 0, 1, 1, 2, PITCH_LETTER_C, 0, 4, NULL), 0, 1);
    bag_add(&bag, tu_note("b", 1, 4, 1, 2, PITCH_LETTER_D, 0, 4, NULL), 0, 2);
    score = score_of(&bag);
    assert(write_failing(&score) == MUSICXML_EXPORT_OK);
    bag_free(&bag);

    memset(&bag, 0, sizeof(bag));
    bag_add(&bag, tu_note("a", 0, 1, 1, 2, PITCH_LETTER_C, 0, 4, NULL), 0, 1);
    bag_add(&bag, tu_note("b", 1, 4, 1, 2, PITCH_LETTER_D, 0, 4, NULL), 1, 1);
    const MusicXmlExportPart parts[] = {{"P1", "One", 1}, {"P2", "Two", 1}};
    score = score_of(&bag);
    score.parts = parts;
    score.part_count = 2;
    assert(write_failing(&score) == MUSICXML_EXPORT_OK);
    bag_free(&bag);
}

static void test_inconsistent_arrays(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    bag_add_note(&bag, "a", 0, 1, 1, 4, PITCH_LETTER_C, NULL);

    MusicXmlExportScore score = score_of(&bag);
    score.events = NULL;
    assert(write_failing(&score) == MUSICXML_EXPORT_ERR_INCONSISTENT_INPUT);

    NoteEvent *with_null[2] = {bag.events[0], NULL};
    score = score_of(&bag);
    score.events = with_null;
    score.count = 2;
    assert(write_failing(&score) == MUSICXML_EXPORT_ERR_INCONSISTENT_INPUT);

    PitchNumericValue *null_pitch[1] = {NULL};
    score = score_of(&bag);
    score.pitch_values = null_pitch;
    assert(write_failing(&score) == MUSICXML_EXPORT_ERR_INCONSISTENT_INPUT);

    score = score_of(&bag);
    score.part_count = 1; /* a count with no parts */
    assert(write_failing(&score) == MUSICXML_EXPORT_ERR_INCONSISTENT_INPUT);

    const MusicXmlExportPart no_staves[] = {{"P1", "One", 0}};
    score = score_of(&bag);
    score.parts = no_staves;
    score.part_count = 1;
    assert(write_failing(&score) == MUSICXML_EXPORT_ERR_INCONSISTENT_INPUT);

    score = score_of(&bag);
    score.tempo_marking_count = 1; /* a count with no array */
    assert(write_failing(&score) == MUSICXML_EXPORT_ERR_INCONSISTENT_INPUT);
    score = score_of(&bag);
    score.dynamics_count = 1;
    assert(write_failing(&score) == MUSICXML_EXPORT_ERR_INCONSISTENT_INPUT);
    score = score_of(&bag);
    score.key_signature_count = 1;
    assert(write_failing(&score) == MUSICXML_EXPORT_ERR_INCONSISTENT_INPUT);
    bag_free(&bag);
}

static void test_unknown_part_and_staff(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    bag_add(&bag, tu_note("a", 0, 1, 1, 4, PITCH_LETTER_C, 0, 4, NULL), 1, 1); /* part 1 does not exist */
    MusicXmlExportScore score = score_of(&bag);
    assert(write_failing(&score) == MUSICXML_EXPORT_ERR_UNKNOWN_PART);
    bag_free(&bag);

    memset(&bag, 0, sizeof(bag));
    bag_add(&bag, tu_note("a", 0, 1, 1, 4, PITCH_LETTER_C, 0, 4, NULL), 0, 2); /* staff 2 of a one-staff part */
    const MusicXmlExportPart one_staff[] = {{"P1", "One", 1}};
    score = score_of(&bag);
    score.parts = one_staff;
    score.part_count = 1;
    assert(write_failing(&score) == MUSICXML_EXPORT_ERR_BAD_STAFF);
    bag_free(&bag);

    memset(&bag, 0, sizeof(bag));
    bag_add(&bag, tu_note("a", 0, 1, 1, 4, PITCH_LETTER_C, 0, 4, NULL), 0, 0); /* staff 0 */
    score = score_of(&bag);
    assert(write_failing(&score) == MUSICXML_EXPORT_ERR_BAD_STAFF);
    bag_free(&bag);

    /* markings name a part and a staff too */
    memset(&bag, 0, sizeof(bag));
    bag_add_note(&bag, "a", 0, 1, 1, 4, PITCH_LETTER_C, NULL);
    Dynamics *dynamics[1] = {tu_dynamics("p", 0, 1, 1, 4)};
    size_t bad_part[1] = {3};
    score = score_of(&bag);
    score.dynamics = dynamics;
    score.dynamics_count = 1;
    score.dynamics_part = bad_part;
    assert(write_failing(&score) == MUSICXML_EXPORT_ERR_UNKNOWN_PART);
    int bad_staff[1] = {2};
    score.dynamics_part = NULL;
    score.dynamics_staff = bad_staff;
    assert(write_failing(&score) == MUSICXML_EXPORT_ERR_BAD_STAFF);
    dynamics_destroy(dynamics[0]);

    MusicXmlExportKeySignature key = {0, MUSICXML_EXPORT_MODE_MAJOR, NULL, 5};
    MusicalTime *at = tu_time(0, 1);
    key.position = at;
    score = score_of(&bag);
    score.key_signatures = &key;
    score.key_signature_count = 1;
    assert(write_failing(&score) == MUSICXML_EXPORT_ERR_UNKNOWN_PART);
    musical_time_destroy(at);
    bag_free(&bag);
}

static void test_dynamics_for_a_missing_event(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    bag_add_note(&bag, "a", 0, 1, 1, 4, PITCH_LETTER_C, NULL);
    Dynamics *dynamics = NULL;
    assert(dynamics_create_for_event("p", tu_id("not-a-note"), &dynamics) == DYNAMICS_OK);
    Dynamics *list[1] = {dynamics};
    MusicXmlExportScore score = score_of(&bag);
    score.dynamics = list;
    score.dynamics_count = 1;
    assert(write_failing(&score) == MUSICXML_EXPORT_ERR_UNKNOWN_EVENT);
    dynamics_destroy(dynamics);

    /* an event the score does have is written at that event */
    assert(dynamics_create_for_event("p", tu_id("a"), &dynamics) == DYNAMICS_OK);
    list[0] = dynamics;
    char *xml = NULL;
    assert(musicxml_export_write_score(&score, &xml) == MUSICXML_EXPORT_OK);
    assert(TU_HAS(xml, "<dynamics><p/></dynamics>"));
    free(xml);
    dynamics_destroy(dynamics);
    bag_free(&bag);
}

static void test_not_representable(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    bag_add(&bag, tu_note("a", -1, 4, 1, 4, PITCH_LETTER_C, 0, 4, NULL), 0, 1); /* before the start */
    MusicXmlExportScore score = score_of(&bag);
    assert(write_failing(&score) == MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE);
    bag_free(&bag);

    memset(&bag, 0, sizeof(bag));
    bag_add_note(&bag, "a", 0, 1, 1, 4, PITCH_LETTER_C, NULL);
    MusicalTime *at = tu_time(0, 1);
    MusicXmlExportKeySignature key = {8, MUSICXML_EXPORT_MODE_MAJOR, at, 0}; /* eight sharps */
    score = score_of(&bag);
    score.key_signatures = &key;
    score.key_signature_count = 1;
    assert(write_failing(&score) == MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE);
    key.sharps_flats = -8;
    assert(write_failing(&score) == MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE);
    key.sharps_flats = 7;
    assert(write_failing(&score) == MUSICXML_EXPORT_OK);
    musical_time_destroy(at);

    /* a beat unit MusicXML has no name for (a third of a whole note), and a tempo that is not a terminating decimal */
    TempoMarking *markings[1] = {tu_tempo_marking(0, 1, NULL, 1, 3, 100, 1)};
    score = score_of(&bag);
    score.tempo_markings = markings;
    score.tempo_marking_count = 1;
    assert(write_failing(&score) == MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE);
    tempo_marking_destroy(markings[0]);
    markings[0] = tu_tempo_marking(0, 1, NULL, 1, 4, 1, 3);
    assert(write_failing(&score) == MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE);
    tempo_marking_destroy(markings[0]);
    markings[0] = tu_tempo_marking(0, 1, NULL, 3, 8, 1005, 10); /* dotted quarter = 100.5 */
    char *xml = NULL;
    assert(musicxml_export_write_score(&score, &xml) == MUSICXML_EXPORT_OK);
    assert(TU_HAS(xml, "<beat-unit-dot/>") && TU_HAS(xml, "<per-minute>100.5</per-minute>"));
    free(xml);
    tempo_marking_destroy(markings[0]);
    bag_free(&bag);
}

static void test_divisions_overflow(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    /* 1 / 2147483659: no <divisions> up to INT32_MAX makes this position a whole number */
    bag_add(&bag, tu_note("a", 1, 2147483659UL, 1, 4, PITCH_LETTER_C, 0, 4, NULL), 0, 1);
    MusicXmlExportScore score = score_of(&bag);
    assert(write_failing(&score) == MUSICXML_EXPORT_ERR_DIVISIONS_OVERFLOW);
    bag_free(&bag);

    /* two denominators whose least common multiple is too large, though neither is */
    memset(&bag, 0, sizeof(bag));
    bag_add(&bag, tu_note("a", 1, 65537, 1, 4, PITCH_LETTER_C, 0, 4, NULL), 0, 1);
    bag_add(&bag, tu_note("b", 1, 65539, 1, 4, PITCH_LETTER_D, 0, 4, NULL), 0, 1);
    score = score_of(&bag);
    assert(write_failing(&score) == MUSICXML_EXPORT_ERR_DIVISIONS_OVERFLOW);
    bag_free(&bag);
}

static void test_output_is_deterministic(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    bag_add_note(&bag, "a", 0, 1, 1, 4, PITCH_LETTER_C, "v");
    bag_add_note(&bag, "b", 0, 1, 1, 2, PITCH_LETTER_E, "w");
    MusicXmlExportScore score = score_of(&bag);
    char *first = NULL;
    char *second = NULL;
    assert(musicxml_export_write_score(&score, &first) == MUSICXML_EXPORT_OK);
    assert(musicxml_export_write_score(&score, &second) == MUSICXML_EXPORT_OK);
    assert(strcmp(first, second) == 0);
    free(first);
    free(second);
    bag_free(&bag);
}

int main(void) {
    test_null_arguments();
    test_missing_spelling();
    test_overlap_is_per_voice();
    test_inconsistent_arrays();
    test_unknown_part_and_staff();
    test_dynamics_for_a_missing_event();
    test_not_representable();
    test_divisions_overflow();
    test_output_is_deterministic();
    printf("test_musicxml_score_errors: all tests passed\n");
    return 0;
}
