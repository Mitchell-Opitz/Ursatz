#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "realization/derivation.h"
#include "realization/line.h"
#include "realization/realization.h"

/*
 * Ids and their refusals, note derivations, line roles by name both ways, the checks of line_add_note and line_add_relation, and the
 * accessors.
 */

static MusicalTime at(long long n, long long d) {
    MusicalTime t;
    assert(musical_time_make(n, d, &t) == MUSICAL_TIME_OK);
    return t;
}

static Duration length(long long n, long long d) {
    Duration v;
    assert(duration_make(n, d, &v) == DURATION_OK);
    return v;
}

static LineNote note(const char *id, long long onset, int midi) {
    LineNote n;
    memset(&n, 0, sizeof(n));
    strcpy(n.id, id);
    n.onset = at(onset, 1);
    n.duration = length(1, 1);
    n.midi = midi;
    return n;
}

static void test_ids(void) {
    assert(realization_id_valid("n1") && realization_id_valid("a") && realization_id_valid("piano-n1-2") &&
           realization_id_valid("omt.instrument.piano") && realization_id_valid("A_b.c-d"));
    assert(!realization_id_valid(NULL) && !realization_id_valid("") && !realization_id_valid("1n") && !realization_id_valid("-n") &&
           !realization_id_valid("a b") && !realization_id_valid("a/b") && !realization_id_valid("\xc3\xa4"));
    char longest[REALIZATION_ID_MAX + 1];
    memset(longest, 'x', 63);
    longest[63] = '\0';
    assert(realization_id_valid(longest));
    longest[63] = 'x';
    longest[64] = '\0';
    assert(!realization_id_valid(longest));
}

static void test_derivation(void) {
    NoteDerivation d;
    memset(&d, 0, sizeof(d));
    assert(!note_derivation_present(&d) && !note_derivation_present(NULL));
    assert(note_derivation_add_source(&d, "n1") == REALIZATION_OK && d.source_count == 1 && strcmp(d.sources[0], "n1") == 0);
    assert(note_derivation_add_source(&d, "1n") == REALIZATION_ERR_BAD_ID && d.source_count == 1);
    assert(note_derivation_add_source(NULL, "n1") == REALIZATION_ERR_NULL && note_derivation_add_source(&d, NULL) == REALIZATION_ERR_NULL);
    for (int i = 1; i < NOTE_DERIVATION_SOURCES_MAX; i++) {
        assert(note_derivation_add_source(&d, "n2") == REALIZATION_OK);
    }
    assert(note_derivation_add_source(&d, "n3") == REALIZATION_ERR_BAD_VALUE && d.source_count == NOTE_DERIVATION_SOURCES_MAX);
    assert(note_derivation_set_operation(&d, "score") == REALIZATION_OK && strcmp(d.operation, "score") == 0);
    assert(note_derivation_set_operation(&d, "call-response") == REALIZATION_OK);
    assert(note_derivation_set_operation(&d, "Score") == REALIZATION_ERR_BAD_VALUE);
    assert(note_derivation_set_operation(&d, "a--b") == REALIZATION_ERR_BAD_VALUE);
    assert(note_derivation_set_operation(&d, "-a") == REALIZATION_ERR_BAD_VALUE);
    assert(note_derivation_set_operation(&d, "") == REALIZATION_ERR_BAD_VALUE);
    assert(strcmp(d.operation, "call-response") == 0);
    assert(note_derivation_set_run(&d, "run 1") == REALIZATION_OK && strcmp(d.run, "run 1") == 0);
    assert(note_derivation_set_run(&d, "") == REALIZATION_OK && d.run[0] == '\0');
    assert(note_derivation_set_run(&d, "tab\there") == REALIZATION_ERR_BAD_VALUE);
    char text[80];
    memset(text, 'r', 63);
    text[63] = '\0';
    assert(note_derivation_set_run(&d, text) == REALIZATION_OK);
    text[63] = 'r';
    text[64] = '\0';
    assert(note_derivation_set_run(&d, text) == REALIZATION_ERR_BAD_VALUE);
    NoteDerivation only_run;
    memset(&only_run, 0, sizeof(only_run));
    assert(note_derivation_set_run(&only_run, "x") == REALIZATION_OK && note_derivation_present(&only_run));
}

static void test_roles(void) {
    static const char *const names[] = {"melody",  "counter-melody", "harmony-line", "bass",    "chords",  "pad",
                                        "accompaniment", "drums", "pedal", "subject", "answer", "countersubject"};
    for (int i = 0; i < 12; i++) {
        LineRole role = LINE_ROLE_PEDAL;
        assert(strcmp(line_role_name((LineRole)i), names[i]) == 0);
        assert(line_role_from_name(names[i], &role) && (int)role == i);
    }
    LineRole role = LINE_ROLE_BASS;
    assert(line_role_name((LineRole)12) == NULL && line_role_name((LineRole)-1) == NULL);
    assert(!line_role_from_name("Melody", &role) && !line_role_from_name(NULL, &role) && role == LINE_ROLE_BASS);
}

static void test_line_and_notes(void) {
    Line *line = NULL;
    assert(line_create("1bad", LINE_ROLE_MELODY, NULL, &line) == REALIZATION_ERR_BAD_ID && line == NULL);
    assert(line_create(NULL, LINE_ROLE_MELODY, NULL, &line) == REALIZATION_ERR_NULL);
    assert(line_create("melody", (LineRole)99, NULL, &line) == REALIZATION_ERR_BAD_VALUE && line == NULL);
    assert(line_create("melody", LINE_ROLE_MELODY, "Soprano", &line) == REALIZATION_OK && line != NULL);
    assert(strcmp(line_id(line), "melody") == 0 && line_role(line) == LINE_ROLE_MELODY && strcmp(line_name(line), "Soprano") == 0);
    Line *unnamed = NULL;
    assert(line_create("bass", LINE_ROLE_BASS, NULL, &unnamed) == REALIZATION_OK && strcmp(line_name(unnamed), "") == 0);
    line_destroy(unnamed);

    LineNote first = note("n1", 0, 60);
    first.spelled = true;
    first.letter = PITCH_LETTER_C;
    first.accidental = 0;
    assert(line_add_note(line, &first) == REALIZATION_OK && line_note_count(line) == 1);
    LineNote duplicate = note("n1", 1, 62);
    assert(line_add_note(line, &duplicate) == REALIZATION_ERR_DUPLICATE_ID && line_note_count(line) == 1);
    LineNote bad = note("2n", 1, 62);
    assert(line_add_note(line, &bad) == REALIZATION_ERR_BAD_ID);
    bad = note("n2", -1, 62);
    assert(line_add_note(line, &bad) == REALIZATION_ERR_BAD_VALUE);
    bad = note("n2", 1, 128);
    assert(line_add_note(line, &bad) == REALIZATION_ERR_BAD_VALUE);
    bad = note("n2", 1, -1);
    assert(line_add_note(line, &bad) == REALIZATION_ERR_BAD_VALUE);
    bad = note("n2", 1, 62);
    bad.duration = length(0, 1);
    assert(line_add_note(line, &bad) == REALIZATION_ERR_BAD_VALUE);
    bad = note("n2", 1, 62);
    bad.spelled = true;
    bad.letter = (PitchLetter)7;
    assert(line_add_note(line, &bad) == REALIZATION_ERR_BAD_VALUE);
    bad.letter = PITCH_LETTER_D;
    bad.accidental = 3;
    assert(line_add_note(line, &bad) == REALIZATION_ERR_BAD_VALUE);
    bad.accidental = -2;
    assert(line_add_note(line, &bad) == REALIZATION_OK);
    bad = note("n3", 2, 64);
    strcpy(bad.derivation.sources[0], "3x");
    bad.derivation.source_count = 1;
    assert(line_add_note(line, &bad) == REALIZATION_ERR_BAD_VALUE && line_note_count(line) == 2);
    LineNote third = note("n3", 2, 64);
    third.onset = at(5, 2);
    assert(note_derivation_add_source(&third.derivation, "motif1") == REALIZATION_OK);
    assert(line_add_note(line, &third) == REALIZATION_OK && line_note_count(line) == 3);
    assert(line_add_note(NULL, &third) == REALIZATION_ERR_NULL && line_add_note(line, NULL) == REALIZATION_ERR_NULL);

    assert(strcmp(line_note_at(line, 0)->id, "n1") == 0 && strcmp(line_note_at(line, 2)->id, "n3") == 0);
    assert(line_note_at(line, 3) == NULL && line_note_at(NULL, 0) == NULL);
    const LineNote *found = line_note_named(line, "n2");
    assert(found != NULL && found->accidental == -2 && found == line_note_at(line, 1));
    assert(line_note_named(line, "n9") == NULL && line_note_named(line, NULL) == NULL);
    assert(strcmp(line_note_named(line, "n3")->derivation.sources[0], "motif1") == 0);
    assert(line_note_count(NULL) == 0 && line_id(NULL) == NULL);

    LineRelation relation;
    memset(&relation, 0, sizeof(relation));
    relation.kind = LINE_RELATION_IMITATION;
    strcpy(relation.target, "bass");
    relation.interval = 7;
    relation.delay = length(2, 1);
    assert(line_add_relation(line, &relation) == REALIZATION_OK && line_relation_count(line) == 1);
    strcpy(relation.target, "melody");
    assert(line_add_relation(line, &relation) == REALIZATION_ERR_BAD_VALUE);
    strcpy(relation.target, "2bass");
    assert(line_add_relation(line, &relation) == REALIZATION_ERR_BAD_ID);
    strcpy(relation.target, "bass");
    relation.has_span = true;
    relation.span.start = at(4, 1);
    relation.span.end = at(4, 1);
    assert(line_add_relation(line, &relation) == REALIZATION_ERR_BAD_VALUE);
    relation.span.end = at(8, 1);
    relation.kind = LINE_RELATION_CALL_RESPONSE;
    assert(line_add_relation(line, &relation) == REALIZATION_OK && line_relation_count(line) == 2);
    relation.kind = (LineRelationKind)9;
    assert(line_add_relation(line, &relation) == REALIZATION_ERR_BAD_VALUE);
    const LineRelation *second = line_relation_at(line, 1);
    assert(second != NULL && second->kind == LINE_RELATION_CALL_RESPONSE && second->has_span && line_relation_at(line, 2) == NULL);
    assert(line_relation_at(line, 0)->interval == 7 && duration_same(line_relation_at(line, 0)->delay, length(2, 1)));

    for (int i = 0; i < 100; i++) {
        char id[16];
        snprintf(id, sizeof(id), "g%d", i);
        LineNote grown = note(id, i, 60);
        assert(line_add_note(line, &grown) == REALIZATION_OK);
    }
    assert(line_note_count(line) == 103 && line_note_named(line, "g99") != NULL);
    line_destroy(line);
    line_destroy(NULL);
}

int main(void) {
    test_ids();
    test_derivation();
    test_roles();
    test_line_and_notes();
    puts("realization_lines: all tests passed");
    return 0;
}
