#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "model/identity/entity_id.h"
#include "model/time/note_value.h"
#include "realization/realization_json.h"
#include "test_knowledge.h"

/*
 * The realization codec (R-627): a work with two movements' worth of what the group holds (lines with relations, a doubling and a span
 * assignment, a piano part of two staves with an edited and a detached note, a pedal, a local meter map, a compound part, a played
 * span, markers, key signatures, a home region, ensembles and work material) round-trips to equal text; the digest is stable and
 * follows the notes; the refusals name their paths.
 */

static MusicalTime at(long long n, long long d) {
    MusicalTime t;
    assert(musical_time_make(n, d, &t) == MUSICAL_TIME_OK);
    return t;
}

static Duration length(long long n, long long d) {
    Duration value;
    assert(duration_make(n, d, &value) == DURATION_OK);
    return value;
}

static TimeSpan span(long long from, long long to) {
    TimeSpan s;
    s.start = at(from, 1);
    s.end = at(to, 1);
    return s;
}

static EntityID *entity(const char *value) {
    EntityID *id = NULL;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

static TimeSignature *signature(unsigned top, int exponent) {
    NoteValue *unit = NULL;
    TimeSignature *made = NULL;
    assert(note_value_create(exponent, &unit) == NOTE_VALUE_OK);
    assert(time_signature_create(top, unit, &made) == TIME_SIGNATURE_OK);
    note_value_destroy(unit);
    return made;
}

static TimeSignature *grouped_signature(void) {
    NoteValue *unit = NULL;
    TimeSignature *made = NULL;
    unsigned groups[2] = {3, 2};
    assert(note_value_create(3, &unit) == NOTE_VALUE_OK);
    assert(time_signature_create_grouped(groups, 2, unit, &made) == TIME_SIGNATURE_OK);
    note_value_destroy(unit);
    return made;
}

static MeterMap *meter_map(const char *id, bool with_grouped) {
    TimeSignature *three = signature(3, 2);
    TimeSignature *six = signature(6, 3);
    TimeSignature *five = grouped_signature();
    MeterMapChangePoint points[3] = {{three, at(0, 1)}, {six, at(12, 1)}, {five, at(24, 1)}};
    MeterMap *map = NULL;
    assert(meter_map_create(entity(id), points, with_grouped ? 3 : 2, &map) == METER_MAP_OK);
    time_signature_destroy(three);
    time_signature_destroy(six);
    time_signature_destroy(five);
    return map;
}

static TempoMap *tempo_map(void) {
    TempoMapChangePoint points[2];
    memset(points, 0, sizeof(points));
    points[0].start = at(0, 1);
    points[0].microseconds_per_quarter = 500000UL;
    points[1].start = at(8, 1);
    points[1].microseconds_per_quarter = 600000UL;
    TempoMap *map = NULL;
    assert(tempo_map_create(entity("tempo-1"), points, 2, &map) == TEMPO_MAP_OK);
    return map;
}

static LineNote line_note(const char *id, long long onset_n, long long onset_d, long long duration_n, long long duration_d, int midi) {
    LineNote note;
    memset(&note, 0, sizeof(note));
    snprintf(note.id, sizeof(note.id), "%s", id);
    note.onset = at(onset_n, onset_d);
    note.duration = length(duration_n, duration_d);
    note.midi = midi;
    return note;
}

static PerformerNote performer_note(const char *id, long long onset, int midi, int staff) {
    PerformerNote note;
    memset(&note, 0, sizeof(note));
    snprintf(note.id, sizeof(note.id), "%s", id);
    note.onset = at(onset, 1);
    note.duration = length(1, 2);
    note.midi = midi;
    note.staff = staff;
    note.voice = 1;
    return note;
}

/* The work the test writes. `first_duration` is the duration in sixteenths of the first soprano note: the digest has to follow it. */
static Work *build_work(long long first_duration) {
    const KnowledgeSnapshot *knowledge = test_knowledge();
    Work *work = NULL;
    assert(work_create("w1", "Sonata", "Mozart", &work) == REALIZATION_OK);
    assert(work_set_ensemble(work, "omt.ensemble.piano-solo") == REALIZATION_OK);
    assert(work_add_material(work, "form:sonata") == REALIZATION_OK && work_add_material(work, "style:galant") == REALIZATION_OK);

    Movement *movement = NULL;
    assert(movement_create("m1", "Allegro", &movement) == REALIZATION_OK);
    assert(movement_set_meter_map(movement, meter_map("meter-1", true)) == REALIZATION_OK);
    assert(movement_set_tempo_map(movement, tempo_map()) == REALIZATION_OK);
    assert(movement_set_home(movement, region_tonal(0, knowledge_snapshot_material(knowledge, "omt.material.major"))) == REALIZATION_OK);
    assert(movement_set_ensemble(movement, "omt.ensemble.string-quartet") == REALIZATION_OK);
    MovementKeySignature key = {at(0, 1), 0, MOVEMENT_KEY_MAJOR};
    assert(movement_add_key_signature(movement, &key) == REALIZATION_OK);
    key.position = at(8, 1);
    key.sharps_flats = -3;
    key.mode = MOVEMENT_KEY_MINOR;
    assert(movement_add_key_signature(movement, &key) == REALIZATION_OK);
    MovementMarker marker = {at(0, 1), "Allegro"};
    assert(movement_add_marker(movement, &marker) == REALIZATION_OK);
    snprintf(marker.text, sizeof(marker.text), "%s", "Coda");
    marker.position = at(12, 1);
    assert(movement_add_marker(movement, &marker) == REALIZATION_OK);
    assert(movement_add_played_span(movement, span(0, 8)) == REALIZATION_OK && movement_add_played_span(movement, span(0, 16)) == REALIZATION_OK);
    MovementPart compound;
    memset(&compound, 0, sizeof(compound));
    snprintf(compound.id, sizeof(compound.id), "%s", "minuet");
    snprintf(compound.name, sizeof(compound.name), "%s", "Minuet");
    compound.span = span(0, 8);
    compound.has_home = true;
    compound.home = region_tonal(9, knowledge_snapshot_material(knowledge, "omt.material.natural-minor"));
    assert(movement_add_compound_part(movement, &compound) == REALIZATION_OK);
    snprintf(compound.id, sizeof(compound.id), "%s", "trio");
    snprintf(compound.name, sizeof(compound.name), "%s", "Trio");
    compound.span = span(8, 16);
    compound.has_home = false;
    assert(movement_add_compound_part(movement, &compound) == REALIZATION_OK);

    /* Two lines, a relation each; the first note is spelled and made from nothing, the second from a source. */
    Line *soprano = NULL;
    assert(line_create("soprano", LINE_ROLE_MELODY, "Soprano", &soprano) == REALIZATION_OK);
    LineNote note = line_note("n1", 0, 1, first_duration, 16, 66);
    note.spelled = true;
    note.letter = PITCH_LETTER_F;
    note.accidental = 1;
    assert(line_add_note(soprano, &note) == REALIZATION_OK);
    note = line_note("n2", 1, 3, 1, 3, 69);
    assert(note_derivation_add_source(&note.derivation, "n1") == REALIZATION_OK);
    assert(note_derivation_set_operation(&note.derivation, "copy") == REALIZATION_OK);
    assert(note_derivation_set_run(&note.derivation, "run 1") == REALIZATION_OK);
    assert(line_add_note(soprano, &note) == REALIZATION_OK);
    LineRelation relation;
    memset(&relation, 0, sizeof(relation));
    relation.kind = LINE_RELATION_CALL_RESPONSE;
    snprintf(relation.target, sizeof(relation.target), "%s", "bass");
    relation.interval = 7;
    relation.delay = length(1, 4);
    assert(line_add_relation(soprano, &relation) == REALIZATION_OK);
    assert(movement_add_line(movement, soprano) == REALIZATION_OK);
    Line *bass = NULL;
    assert(line_create("bass", LINE_ROLE_BASS, NULL, &bass) == REALIZATION_OK);
    assert(line_add_note(bass, &(LineNote){.id = "b1", .onset = at(0, 1), .duration = length(2, 1), .midi = 40}) == REALIZATION_OK);
    memset(&relation, 0, sizeof(relation));
    relation.kind = LINE_RELATION_PARALLEL;
    snprintf(relation.target, sizeof(relation.target), "%s", "soprano");
    relation.interval = -12;
    relation.delay = length(0, 1);
    relation.has_span = true;
    relation.span = span(0, 4);
    assert(line_add_relation(bass, &relation) == REALIZATION_OK);
    assert(movement_add_line(movement, bass) == REALIZATION_OK);

    /* A piano part of two staves, a violin part, a local meter map and a pedal. */
    PerformerPart *piano = NULL;
    assert(performer_part_create("piano", "omt.instrument.piano", "Piano", 1, 1, 2, 0, &piano) == REALIZATION_OK);
    PerformerNote played = performer_note("piano-n1", 0, 66, 1);
    played.spelled = true;
    played.letter = PITCH_LETTER_G;
    played.accidental = -1;
    assert(note_derivation_add_source(&played.derivation, "n1") == REALIZATION_OK);
    assert(note_derivation_set_operation(&played.derivation, "score") == REALIZATION_OK);
    assert(performer_part_add_note(piano, &played) == REALIZATION_OK);
    PerformerNote edited = performer_note("e1", 1, 60, 2);
    edited.edited = true;
    assert(performer_part_add_note(piano, &edited) == REALIZATION_OK);
    PerformerNote detached = performer_note("d1", 2, 62, 2);
    detached.detached = true;
    detached.review = true;
    assert(performer_part_add_note(piano, &detached) == REALIZATION_OK);
    PedalMarking pedal = {PEDAL_SUSTAIN, span(0, 2)};
    assert(performer_part_add_pedal(piano, &pedal) == REALIZATION_OK);
    TimeSignature *two = signature(2, 2);
    MeterMapChangePoint local_point = {two, at(0, 1)};
    MeterMap *local = NULL;
    assert(meter_map_create(entity("local-1"), &local_point, 1, &local) == METER_MAP_OK);
    time_signature_destroy(two);
    assert(performer_part_set_local_meter_map(piano, local) == REALIZATION_OK);
    assert(movement_add_part(movement, piano) == REALIZATION_OK);
    PerformerPart *violin = NULL;
    assert(performer_part_create("violin", "omt.instrument.violin", "Violin", 1, 2, 1, 40, &violin) == REALIZATION_OK);
    assert(movement_add_part(movement, violin) == REALIZATION_OK);

    ScoringAssignment assignment;
    memset(&assignment, 0, sizeof(assignment));
    snprintf(assignment.id, sizeof(assignment.id), "%s", "a1");
    snprintf(assignment.line, sizeof(assignment.line), "%s", "soprano");
    snprintf(assignment.part, sizeof(assignment.part), "%s", "piano");
    assignment.whole = true;
    assignment.staff = 1;
    assignment.voice = 1;
    assert(movement_add_assignment(movement, &assignment) == REALIZATION_OK);
    snprintf(assignment.id, sizeof(assignment.id), "%s", "a2");
    snprintf(assignment.line, sizeof(assignment.line), "%s", "bass");
    assignment.whole = false;
    assignment.span = span(0, 4);
    assignment.staff = 2;
    assignment.transform.kind = SCORING_TRANSFORM_OCTAVE;
    assignment.transform.octaves = -1;
    assert(movement_add_assignment(movement, &assignment) == REALIZATION_OK);
    snprintf(assignment.id, sizeof(assignment.id), "%s", "a3"); /* a doubling: the soprano goes to the violin too */
    snprintf(assignment.line, sizeof(assignment.line), "%s", "soprano");
    snprintf(assignment.part, sizeof(assignment.part), "%s", "violin");
    assignment.whole = true;
    assignment.staff = 1;
    memset(&assignment.transform, 0, sizeof(assignment.transform));
    assignment.divisi = true;
    assert(movement_add_assignment(movement, &assignment) == REALIZATION_OK);
    assert(work_add_movement(work, movement) == REALIZATION_OK);

    /* A second movement with a smaller shape: no tempo, no home, a defaulted meter. */
    Movement *second = NULL;
    assert(movement_create("m2", NULL, &second) == REALIZATION_OK);
    MeterMap *defaulted = NULL;
    assert(meter_map_create_defaulted(entity("meter-2"), &defaulted) == METER_MAP_OK);
    assert(movement_set_meter_map(second, defaulted) == REALIZATION_OK);
    assert(work_add_movement(work, second) == REALIZATION_OK);
    return work;
}

static char *replace_once(const char *text, const char *from, const char *to) {
    const char *found = strstr(text, from);
    assert(found != NULL);
    size_t head = (size_t)(found - text);
    char *out = malloc(strlen(text) + strlen(to) + 1);
    assert(out != NULL);
    memcpy(out, text, head);
    strcpy(out + head, to);
    strcat(out, found + strlen(from));
    return out;
}

/* The schema test validates what the codec wrote: the documents go to files in the working directory. */
static void write_document(const char *name, const char *text) {
    char path[128];
    snprintf(path, sizeof(path), "realization_written_%s.json", name);
    FILE *file = fopen(path, "wb");
    assert(file != NULL && fwrite(text, 1, strlen(text), file) == strlen(text));
    fclose(file);
}

static void test_round_trip(void) {
    Work *work = build_work(1);
    RealizationJsonError error;
    char *text = realization_work_to_json(work, &error);
    assert(text != NULL);
    write_document("work", text);
    assert(strncmp(text, "{\n  \"format\": \"ursatz-realization\",\n  \"format_version\": 1,\n  \"work\": {", 62) == 0);
    assert(text[strlen(text) - 1] == '\n');
    /* The shape of the members the requirement names. */
    assert(strstr(text, "\"time_signature\": \"3/4\"") != NULL && strstr(text, "\"groups\": [") != NULL);
    assert(strstr(text, "\"quarters_per_minute\": {") != NULL && strstr(text, "\"num\": 120") != NULL && strstr(text, "\"num\": 100") != NULL);
    assert(strstr(text, "\"spelling\": \"F#\"") != NULL && strstr(text, "\"spelling\": \"Gb\"") != NULL && strstr(text, "\"spelling\": null") != NULL);
    assert(strstr(text, "\"home\": null") != NULL && strstr(text, "\"kind\": \"tonal\"") != NULL);
    assert(strstr(text, "\"derivation\": null") != NULL && strstr(text, "\"operation\": \"copy\"") != NULL);
    assert(strstr(text, "\"local_meter\": []") != NULL && strstr(text, "\"meter\": []") != NULL && strstr(text, "\"tempo\": []") != NULL);

    Work *read = NULL;
    assert(realization_work_from_json(test_knowledge(), text, &read, &error) == REALIZATION_OK);
    assert(read != NULL && work_movement_count(read) == 2 && work_material_count(read) == 2);
    char *again = realization_work_to_json(read, &error);
    assert(again != NULL && strcmp(text, again) == 0);
    /* What was read is the work: ids, notes, flags, maps. */
    const Movement *first = work_movement_at(read, 0);
    assert(movement_line_count(first) == 2 && movement_part_count(first) == 2 && movement_assignment_count(first) == 3);
    assert(performer_part_local_meter_map(movement_part_named(first, "piano")) != NULL && meter_map_region_count(movement_meter_map(first)) == 3);
    assert(movement_part_named(first, "piano") && performer_part_note_at(movement_part_named(first, "piano"), 1)->edited);
    assert(performer_part_note_at(movement_part_named(first, "piano"), 2)->detached && performer_part_note_at(movement_part_named(first, "piano"), 2)->review);
    assert(movement_tempo_map(first) != NULL && tempo_map_region_count(movement_tempo_map(first)) == 2);
    assert(movement_tempo_map(work_movement_at(read, 1)) == NULL);
    free(again);
    work_destroy(read);
    free(text);
    work_destroy(work);

    /* The smallest work: no title, no movement. */
    Work *minimal = NULL;
    assert(work_create("w2", NULL, NULL, &minimal) == REALIZATION_OK);
    text = realization_work_to_json(minimal, &error);
    assert(text != NULL && strstr(text, "\"movements\": []") != NULL && strstr(text, "\"title\": null") != NULL);
    write_document("minimal", text);
    free(text);
    work_destroy(minimal);
}

static void test_digest(void) {
    Work *work = build_work(1);
    char first[17];
    char second[17];
    assert(realization_digest(work, first) && realization_digest(work, second) && strcmp(first, second) == 0 && strlen(first) == 16);
    /* The digest of the compact text, and of what was read. */
    RealizationJsonError error;
    char *compact = realization_work_to_compact_json(work, &error);
    assert(compact != NULL && strchr(compact, '\n') == NULL && strstr(compact, "  ") == NULL);
    Work *read = NULL;
    assert(realization_work_from_json(test_knowledge(), compact, &read, &error) == REALIZATION_OK);
    char third[17];
    assert(realization_digest(read, third) && strcmp(first, third) == 0);
    work_destroy(read);
    free(compact);
    /* One note's duration changes the digest. */
    Work *longer = build_work(2);
    char other[17];
    assert(realization_digest(longer, other) && strcmp(first, other) != 0);
    assert(!realization_digest(NULL, other));
    work_destroy(longer);
    work_destroy(work);
}

static void expect_refused(const char *text, RealizationResult result, const char *path, const char *message_part) {
    Work *read = (Work *)1;
    RealizationJsonError error;
    memset(&error, 0, sizeof(error));
    RealizationResult got = realization_work_from_json(test_knowledge(), text, &read, &error);
    if (got != result) {
        fprintf(stderr, "expected result %d for '%s', got %d: %s at '%s'\n", (int)result, path, (int)got, error.message, error.path);
    }
    assert(got == result);
    assert(read == NULL);
    if (strcmp(error.path, path) != 0 || (message_part && strstr(error.message, message_part) == NULL)) {
        fprintf(stderr, "expected '%s' (%s), got '%s': %s\n", path, message_part ? message_part : "", error.path, error.message);
    }
    assert(strcmp(error.path, path) == 0);
    assert(message_part == NULL || strstr(error.message, message_part) != NULL);
}

static void test_refusals(void) {
    Work *work = build_work(1);
    char *text = realization_work_to_json(work, NULL);
    assert(text != NULL);
    work_destroy(work);
    char *edited = replace_once(text, "\"ursatz-realization\"", "\"ursatz-plan\"");
    expect_refused(edited, REALIZATION_ERR_BAD_VALUE, "format", "expected");
    free(edited);
    edited = replace_once(text, "\"format_version\": 1", "\"format_version\": 2");
    expect_refused(edited, REALIZATION_ERR_BAD_VALUE, "format_version", "version 1");
    free(edited);
    edited = replace_once(text, "\"id\": \"soprano\"", "\"id\": \"1soprano\"");
    expect_refused(edited, REALIZATION_ERR_BAD_ID, "work.movements[0].lines[0].id", "not a valid id");
    free(edited);
    edited = replace_once(text, "\"id\": \"violin\"", "\"id\": \"piano\"");
    expect_refused(edited, REALIZATION_ERR_DUPLICATE_ID, "work.movements[0].parts[1].id", "twice");
    free(edited);
    edited = replace_once(text, "\"id\": \"n2\"", "\"id\": \"n1\"");
    expect_refused(edited, REALIZATION_ERR_DUPLICATE_ID, "work.movements[0].lines[0].notes[1].id", "twice");
    free(edited);
    edited = replace_once(text, "\"line\": \"soprano\"", "\"line\": \"tenor\"");
    expect_refused(edited, REALIZATION_ERR_UNKNOWN_REFERENCE, "work.movements[0].assignments[0].line", "tenor");
    free(edited);
    edited = replace_once(text, "\"part\": \"piano\"", "\"part\": \"harp\"");
    expect_refused(edited, REALIZATION_ERR_UNKNOWN_REFERENCE, "work.movements[0].assignments[0].part", "harp");
    free(edited);
    edited = replace_once(text, "\"material\": \"omt.material.major\"", "\"material\": \"omt.material.nope\"");
    expect_refused(edited, REALIZATION_ERR_UNKNOWN_REFERENCE, "work.movements[0].home.material", "no material");
    free(edited);
    edited = replace_once(text, "\"kind\": \"tonal\"", "\"kind\": \"pitch-field\"");
    expect_refused(edited, REALIZATION_ERR_BAD_VALUE, "work.movements[0].home.kind", "not built");
    free(edited);
    edited = replace_once(text, "\"midi\": 66,\n                \"spelling\": \"F#\"", "\"midi\": 200,\n                \"spelling\": \"F#\"");
    expect_refused(edited, REALIZATION_ERR_BAD_VALUE, "work.movements[0].lines[0].notes[0].midi", "from 0 to 127");
    free(edited);
    edited = replace_once(text, "\"time_signature\": \"3/4\"", "\"time_signature\": \"3/5\"");
    expect_refused(edited, REALIZATION_ERR_BAD_VALUE, "work.movements[0].meter[0].time_signature", "time signature");
    free(edited);
    expect_refused("{\"format\": \"ursatz-realization\", \"format_version\": 1}", REALIZATION_ERR_BAD_VALUE, "work", "missing");
    expect_refused("[1]", REALIZATION_ERR_BAD_VALUE, "", "object");
    expect_refused("not json", REALIZATION_ERR_BAD_VALUE, "", "JSON");
    Work *none = NULL;
    assert(realization_work_from_json(NULL, text, &none, NULL) == REALIZATION_ERR_NULL && realization_work_from_json(test_knowledge(), NULL, &none, NULL) == REALIZATION_ERR_NULL);
    free(text);
}

int main(void) {
    test_round_trip();
    test_digest();
    test_refusals();
    printf("test_realization_json: ok\n");
    return 0;
}
