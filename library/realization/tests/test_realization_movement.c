#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "realization/piano_reduction.h"
#include "realization/work.h"
#include "ursatz_test_knowledge.h"

/*
 * Movements own and free their maps; key signatures, markers, played spans and compound parts are kept in order with their refusals;
 * a work's ensemble is the default a movement overrides; the piano reduction assigns every line to one part by the staff rule.
 */

static MusicalTime at(long long n) {
    MusicalTime t;
    assert(musical_time_make(n, 1, &t) == MUSICAL_TIME_OK);
    return t;
}

static Duration beats(long long n) {
    Duration d;
    assert(duration_make(n, 1, &d) == DURATION_OK);
    return d;
}

static EntityID *entity(const char *value) {
    EntityID *id = NULL;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

static TimeSpan span(long long from, long long to) {
    TimeSpan s;
    s.start = at(from);
    s.end = at(to);
    return s;
}

static Movement *movement(const char *id, const char *title) {
    Movement *m = NULL;
    assert(movement_create(id, title, &m) == REALIZATION_OK);
    return m;
}

static void test_maps_are_owned(void) {
    Movement *m = movement("m1", "Allegro");
    assert(strcmp(movement_id(m), "m1") == 0 && strcmp(movement_title(m), "Allegro") == 0);
    assert(movement_meter_map(m) == NULL && movement_tempo_map(m) == NULL);
    MeterMap *meter = NULL;
    assert(meter_map_create_defaulted(entity("meter-1"), &meter) == METER_MAP_OK);
    assert(movement_set_meter_map(m, meter) == REALIZATION_OK && movement_meter_map(m) == meter);
    assert(movement_set_meter_map(m, meter) == REALIZATION_OK && movement_meter_map(m) == meter); /* the same map: not freed */
    MeterMap *second = NULL;
    assert(meter_map_create_defaulted(entity("meter-2"), &second) == METER_MAP_OK);
    assert(movement_set_meter_map(m, second) == REALIZATION_OK && movement_meter_map(m) == second); /* the first is freed */
    TempoMap *tempo = NULL;
    assert(tempo_map_create_defaulted(entity("tempo-1"), &tempo) == TEMPO_MAP_OK);
    assert(movement_set_tempo_map(m, tempo) == REALIZATION_OK && movement_tempo_map(m) == tempo);
    assert(movement_set_tempo_map(m, NULL) == REALIZATION_OK && movement_tempo_map(m) == NULL); /* cleared and freed */
    TempoMap *again = NULL;
    assert(tempo_map_create_defaulted(entity("tempo-2"), &again) == TEMPO_MAP_OK);
    assert(movement_set_tempo_map(m, again) == REALIZATION_OK);
    assert(movement_set_meter_map(NULL, NULL) == REALIZATION_ERR_NULL);
    movement_destroy(m); /* frees the meter map and the tempo map it holds (valgrind checks) */

    Movement *untitled = movement("m2", NULL);
    assert(movement_title(untitled) == NULL);
    movement_destroy(untitled);
    Movement *bad = NULL;
    assert(movement_create("2m", NULL, &bad) == REALIZATION_ERR_BAD_ID && bad == NULL);
    movement_destroy(NULL);
}

static void test_ordered_lists(void) {
    Movement *m = movement("m1", NULL);
    MovementKeySignature key = {at(0), 2, MOVEMENT_KEY_MAJOR};
    assert(movement_add_key_signature(m, &key) == REALIZATION_OK);
    key.position = at(8);
    key.sharps_flats = -3;
    key.mode = MOVEMENT_KEY_MINOR;
    assert(movement_add_key_signature(m, &key) == REALIZATION_OK);
    key.position = at(8);
    assert(movement_add_key_signature(m, &key) == REALIZATION_OK); /* equal positions are not decreasing */
    key.position = at(4);
    assert(movement_add_key_signature(m, &key) == REALIZATION_ERR_BAD_VALUE);
    key.position = at(9);
    key.sharps_flats = 8;
    assert(movement_add_key_signature(m, &key) == REALIZATION_ERR_BAD_VALUE);
    key.sharps_flats = -7;
    key.mode = (MovementKeyMode)5;
    assert(movement_add_key_signature(m, &key) == REALIZATION_ERR_BAD_VALUE);
    key.mode = MOVEMENT_KEY_UNKNOWN;
    key.position = at(-1);
    assert(movement_add_key_signature(m, &key) == REALIZATION_ERR_BAD_VALUE);
    assert(movement_key_signature_count(m) == 3 && movement_key_signature_at(m, 1)->sharps_flats == -3 &&
           movement_key_signature_at(m, 0)->mode == MOVEMENT_KEY_MAJOR && movement_key_signature_at(m, 3) == NULL);

    MovementMarker marker;
    memset(&marker, 0, sizeof(marker));
    marker.position = at(0);
    strcpy(marker.text, "A");
    assert(movement_add_marker(m, &marker) == REALIZATION_OK);
    marker.position = at(16);
    strcpy(marker.text, "B");
    assert(movement_add_marker(m, &marker) == REALIZATION_OK);
    marker.text[0] = '\0';
    assert(movement_add_marker(m, &marker) == REALIZATION_ERR_BAD_VALUE);
    memset(marker.text, 'x', sizeof(marker.text));
    assert(movement_add_marker(m, &marker) == REALIZATION_ERR_BAD_VALUE);
    memset(marker.text, 'x', 63);
    marker.text[63] = '\0';
    assert(movement_add_marker(m, &marker) == REALIZATION_OK);
    marker.position = at(-2);
    assert(movement_add_marker(m, &marker) == REALIZATION_ERR_BAD_VALUE);
    assert(movement_marker_count(m) == 3 && strcmp(movement_marker_at(m, 1)->text, "B") == 0);

    assert(movement_add_played_span(m, span(0, 8)) == REALIZATION_OK && movement_add_played_span(m, span(0, 8)) == REALIZATION_OK);
    assert(movement_add_played_span(m, span(8, 8)) == REALIZATION_ERR_BAD_VALUE);
    assert(movement_add_played_span(m, span(8, 4)) == REALIZATION_ERR_BAD_VALUE);
    assert(movement_played_span_count(m) == 2 && musical_time_same(movement_played_span_at(m, 1)->end, at(8)));

    MovementPart minuet;
    memset(&minuet, 0, sizeof(minuet));
    strcpy(minuet.id, "minuet");
    strcpy(minuet.name, "Minuet");
    minuet.span = span(0, 16);
    assert(movement_add_compound_part(m, &minuet) == REALIZATION_OK);
    assert(movement_add_compound_part(m, &minuet) == REALIZATION_ERR_DUPLICATE_ID);
    strcpy(minuet.id, "trio");
    strcpy(minuet.name, "Trio");
    minuet.span = span(16, 32);
    assert(movement_add_compound_part(m, &minuet) == REALIZATION_OK);
    strcpy(minuet.id, "3rd");
    assert(movement_add_compound_part(m, &minuet) == REALIZATION_ERR_BAD_ID);
    strcpy(minuet.id, "empty");
    minuet.span = span(5, 5);
    assert(movement_add_compound_part(m, &minuet) == REALIZATION_ERR_BAD_VALUE);
    assert(movement_compound_part_count(m) == 2 && strcmp(movement_compound_part_at(m, 1)->id, "trio") == 0);

    Region home = region_tonal(0, knowledge_snapshot_material(test_knowledge(), "omt.material.major"));
    Region read;
    assert(!movement_home(m, &read));
    assert(movement_set_home(m, home) == REALIZATION_OK && movement_home(m, &read) && region_equals(&home, &read));
    assert(movement_ensemble(m) == NULL);
    assert(movement_set_ensemble(m, "omt.instrument.solo-piano") == REALIZATION_OK &&
           strcmp(movement_ensemble(m), "omt.instrument.solo-piano") == 0);
    assert(movement_set_ensemble(m, "9") == REALIZATION_ERR_BAD_ID);
    assert(movement_set_ensemble(m, NULL) == REALIZATION_OK && movement_ensemble(m) == NULL);
    movement_destroy(m);
}

static void test_containers(void) {
    Movement *m = movement("m1", NULL);
    Line *line = NULL;
    assert(line_create("melody", LINE_ROLE_MELODY, NULL, &line) == REALIZATION_OK);
    assert(movement_add_line(m, line) == REALIZATION_OK && movement_line_named(m, "melody") == line && movement_line_at(m, 0) == line);
    Line *twin = NULL;
    assert(line_create("melody", LINE_ROLE_BASS, NULL, &twin) == REALIZATION_OK);
    assert(movement_add_line(m, twin) == REALIZATION_ERR_DUPLICATE_ID);
    line_destroy(twin); /* the caller keeps what was refused */
    PerformerPart *part = NULL;
    assert(performer_part_create("flute", "omt.instrument.piano", "Flute", 1, 1, 1, 73, &part) == REALIZATION_OK);
    assert(movement_add_part(m, part) == REALIZATION_OK && movement_part_named(m, "flute") == part);
    PerformerPart *same = NULL;
    assert(performer_part_create("flute", "omt.instrument.piano", "Flute", 1, 1, 1, 73, &same) == REALIZATION_OK);
    assert(movement_add_part(m, same) == REALIZATION_ERR_DUPLICATE_ID);
    performer_part_destroy(same);
    assert(movement_part_named(m, "oboe") == NULL && movement_line_named(m, NULL) == NULL);

    ScoringAssignment a;
    memset(&a, 0, sizeof(a));
    strcpy(a.id, "a1");
    strcpy(a.line, "melody");
    strcpy(a.part, "flute");
    a.whole = true;
    a.staff = 1;
    a.voice = 1;
    assert(movement_add_assignment(m, &a) == REALIZATION_OK && movement_assignment_count(m) == 1);
    assert(movement_add_assignment(m, &a) == REALIZATION_ERR_DUPLICATE_ID);
    strcpy(a.id, "a2");
    a.voice = 0;
    assert(movement_add_assignment(m, &a) == REALIZATION_ERR_BAD_VALUE);
    LineNote note;
    memset(&note, 0, sizeof(note));
    strcpy(note.id, "n1");
    note.onset = at(0);
    note.duration = beats(1);
    note.midi = 72;
    assert(line_add_note(line, &note) == REALIZATION_OK);
    assert(movement_score(m, "r") == REALIZATION_OK && performer_part_note_count(part) == 1);
    assert(strcmp(movement_assignment_at(m, 0)->part, "flute") == 0 && movement_assignment_at(m, 1) == NULL);
    movement_destroy(m);
}

/* A part's pedal markings: none to begin with, kept in start order, an overlap refused, an index out of range has none. */
static void test_part_pedals(void) {
    PerformerPart *part = NULL;
    assert(performer_part_create("piano", "omt.instrument.piano", "Piano", 1, 1, 2, 0, &part) == REALIZATION_OK);
    assert(performer_part_pedal_count(part) == 0 && performer_part_pedal_at(part, 0) == NULL);
    assert(performer_part_pedal_count(NULL) == 0 && performer_part_pedal_at(NULL, 0) == NULL);

    PedalMarking later = {PEDAL_SUSTAIN, span(4, 6)};
    PedalMarking earlier = {PEDAL_SUSTAIN, span(0, 2)};
    assert(performer_part_add_pedal(part, &later) == REALIZATION_OK);
    assert(performer_part_add_pedal(part, &earlier) == REALIZATION_OK); /* added out of order */
    assert(performer_part_pedal_count(part) == 2);
    assert(rational_equals(performer_part_pedal_at(part, 0)->span.start.value, at(0).value));
    assert(rational_equals(performer_part_pedal_at(part, 1)->span.start.value, at(4).value));
    PedalMarking overlapping = {PEDAL_SUSTAIN, span(1, 5)};
    assert(performer_part_add_pedal(part, &overlapping) == REALIZATION_ERR_BAD_VALUE);
    assert(performer_part_pedal_count(part) == 2);
    assert(performer_part_pedal_at(part, 2) == NULL);
    performer_part_destroy(part);
}

static void test_work_defaults(void) {
    Work *work = NULL;
    assert(work_create("w1", "Sonata", "Anon", &work) == REALIZATION_OK);
    assert(strcmp(work_title(work), "Sonata") == 0 && strcmp(work_composer(work), "Anon") == 0 && work_ensemble(work) == NULL);
    Movement *first = movement("m1", "I");
    Movement *second = movement("m2", "II");
    Movement *third = movement("m1", "dup");
    assert(work_add_movement(work, first) == REALIZATION_OK && work_add_movement(work, second) == REALIZATION_OK);
    assert(work_add_movement(work, third) == REALIZATION_ERR_DUPLICATE_ID);
    movement_destroy(third);
    assert(work_movement_count(work) == 2 && work_movement_at(work, 0) == first && work_movement_at(work, 1) == second &&
           work_movement_at(work, 2) == NULL);
    assert(work_movement_ensemble(work, first) == NULL);
    assert(work_set_ensemble(work, "omt.instrument.solo-piano") == REALIZATION_OK);
    assert(strcmp(work_movement_ensemble(work, first), "omt.instrument.solo-piano") == 0);
    assert(movement_set_ensemble(second, "user.instrument.duo") == REALIZATION_OK);
    assert(strcmp(work_movement_ensemble(work, second), "user.instrument.duo") == 0);
    assert(strcmp(work_movement_ensemble(work, first), "omt.instrument.solo-piano") == 0);
    assert(work_set_ensemble(work, "1") == REALIZATION_ERR_BAD_ID);
    assert(work_set_ensemble(work, NULL) == REALIZATION_OK && work_movement_ensemble(work, first) == NULL);

    assert(work_add_material(work, "relation:supertonic") == REALIZATION_OK && work_add_material(work, "motif:theme-1.a") == REALIZATION_OK);
    assert(work_add_material(work, "Relation:x") == REALIZATION_ERR_BAD_VALUE && work_add_material(work, "relation") == REALIZATION_ERR_BAD_VALUE);
    assert(work_add_material(work, "relation:") == REALIZATION_ERR_BAD_VALUE && work_add_material(work, ":x") == REALIZATION_ERR_BAD_VALUE);
    assert(work_add_material(work, "a--b:x") == REALIZATION_ERR_BAD_VALUE && work_add_material(work, "a:b c") == REALIZATION_ERR_BAD_VALUE);
    assert(work_material_count(work) == 2 && strcmp(work_material_at(work, 1), "motif:theme-1.a") == 0 && work_material_at(work, 2) == NULL);

    Work *bare = NULL;
    assert(work_create("w2", NULL, NULL, &bare) == REALIZATION_OK && work_title(bare) == NULL && work_composer(bare) == NULL);
    work_destroy(bare);
    work_destroy(work); /* frees both movements */
    assert(work_create("9", NULL, NULL, &bare) == REALIZATION_ERR_BAD_ID);
}

static Line *line_of(const char *id, LineRole role, const char *note_id, int midi, int count) {
    Line *line = NULL;
    assert(line_create(id, role, NULL, &line) == REALIZATION_OK);
    for (int i = 0; i < count; i++) {
        LineNote note;
        memset(&note, 0, sizeof(note));
        snprintf(note.id, sizeof(note.id), "%s%d", note_id, i + 1);
        note.onset = at(i);
        note.duration = beats(1);
        note.midi = midi;
        assert(line_add_note(line, &note) == REALIZATION_OK);
    }
    return line;
}

static void test_piano_reduction(void) {
    const Instrument *piano = knowledge_snapshot_instrument(test_knowledge(), "omt.instrument.piano");
    assert(piano != NULL);
    Movement *m = movement("m1", NULL);
    assert(movement_add_line(m, line_of("melody", LINE_ROLE_MELODY, "m", 50, 2)) == REALIZATION_OK); /* a melody is staff 1 at any pitch */
    assert(movement_add_line(m, line_of("bass", LINE_ROLE_BASS, "b", 80, 2)) == REALIZATION_OK);     /* a bass is staff 2 at any pitch */
    assert(movement_add_line(m, line_of("low", LINE_ROLE_HARMONY_LINE, "l", 59, 2)) == REALIZATION_OK);
    assert(movement_add_line(m, line_of("mid", LINE_ROLE_ACCOMPANIMENT, "i", 60, 2)) == REALIZATION_OK);
    assert(movement_add_line(m, line_of("none", LINE_ROLE_PAD, "p", 0, 0)) == REALIZATION_OK); /* no notes: staff 1 */
    assert(movement_reduce_to_piano(m, piano, "r1") == REALIZATION_OK);

    PerformerPart *part = movement_part_named(m, "piano");
    assert(part != NULL && movement_part_count(m) == 1 && strcmp(performer_part_name(part), "Piano") == 0);
    assert(strcmp(performer_part_instrument(part), "omt.instrument.piano") == 0 && performer_part_number(part) == 1 &&
           performer_part_players(part) == 1 && performer_part_staff_count(part) == 2 && performer_part_midi_program(part) == 0);
    assert(movement_assignment_count(m) == 5);
    static const char *const ids[5] = {"a-melody", "a-bass", "a-low", "a-mid", "a-none"};
    static const int staffs[5] = {1, 2, 2, 1, 1};
    for (size_t i = 0; i < 5; i++) {
        const ScoringAssignment *a = movement_assignment_at(m, i);
        assert(strcmp(a->id, ids[i]) == 0 && a->whole && a->staff == staffs[i] && a->voice == (int)i + 1 &&
               strcmp(a->part, "piano") == 0);
    }
    assert(performer_part_note_count(part) == 8);
    /* by onset; at each beat the lines in assignment order */
    static const char *const note_ids[8] = {"piano-m1", "piano-b1", "piano-l1", "piano-i1", "piano-m2", "piano-b2", "piano-l2", "piano-i2"};
    for (size_t i = 0; i < 8; i++) {
        const PerformerNote *n = performer_part_note_at(part, i);
        assert(strcmp(n->id, note_ids[i]) == 0);
        assert(n->derivation.source_count == 1 && strcmp(n->derivation.operation, "score") == 0 && strcmp(n->derivation.run, "r1") == 0);
        assert(n->staff == staffs[i % 4] && n->voice == (int)(i % 4) + 1);
    }
    assert(strcmp(performer_part_note_at(part, 0)->derivation.sources[0], "m1") == 0);
    /* a second reduction is refused: the movement already has a part */
    assert(movement_reduce_to_piano(m, piano, "r2") == REALIZATION_ERR_BAD_VALUE && movement_part_count(m) == 1 &&
           movement_assignment_count(m) == 5);
    movement_destroy(m);

    /* a motif stays on one staff */
    Movement *motif = movement("m2", NULL);
    assert(movement_add_line(motif, line_of("melody", LINE_ROLE_MELODY, "m", 72, 3)) == REALIZATION_OK);
    assert(movement_reduce_to_piano(motif, piano, "") == REALIZATION_OK);
    assert(performer_part_staff_count(movement_part_named(motif, "piano")) == 1 &&
           performer_part_note_count(movement_part_named(motif, "piano")) == 3);
    movement_destroy(motif);

    /* a movement that has assignments but no part is refused too; so is an instrument with one staff */
    Movement *busy = movement("m3", NULL);
    ScoringAssignment a;
    memset(&a, 0, sizeof(a));
    strcpy(a.id, "x");
    strcpy(a.line, "y");
    strcpy(a.part, "z");
    a.whole = true;
    a.staff = 1;
    a.voice = 1;
    assert(movement_add_assignment(busy, &a) == REALIZATION_OK);
    assert(movement_reduce_to_piano(busy, piano, "") == REALIZATION_ERR_BAD_VALUE && movement_part_count(busy) == 0);
    movement_destroy(busy);
    Instrument flute = *piano;
    flute.staff_count = 1;
    Movement *single = movement("m4", NULL);
    assert(movement_reduce_to_piano(single, &flute, "") == REALIZATION_ERR_BAD_VALUE && movement_part_count(single) == 0);
    assert(movement_reduce_to_piano(NULL, piano, "") == REALIZATION_ERR_NULL && movement_reduce_to_piano(single, NULL, "") == REALIZATION_ERR_NULL);
    /* an empty movement: a part with no notes */
    assert(movement_reduce_to_piano(single, piano, "") == REALIZATION_OK && performer_part_note_count(movement_part_named(single, "piano")) == 0);
    movement_destroy(single);
}

int main(void) {
    test_maps_are_owned();
    test_ordered_lists();
    test_containers();
    test_part_pedals();
    test_work_defaults();
    test_piano_reduction();
    puts("realization_movement: all tests passed");
    return 0;
}
