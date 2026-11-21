#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "realization/line.h"
#include "realization/performer_part.h"
#include "realization/scoring.h"
#include "realization_test_access.h"

/*
 * Scoring: a four-line chorale into four parts and into one two-staff part, an octave transform, one line doubled into two parts, a
 * hand-off, regeneration that keeps an edited note and flags it, the refusals (nothing changes), and the same ids and order twice.
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

/* A line whose notes n0, n1, ... start at 0, 1, ... one beat long with the given pitches. */
static Line *make_line(const char *id, LineRole role, const int *midis, size_t count) {
    Line *line = NULL;
    assert(line_create(id, role, NULL, &line) == REALIZATION_OK);
    for (size_t i = 0; i < count; i++) {
        LineNote note;
        memset(&note, 0, sizeof(note));
        snprintf(note.id, sizeof(note.id), "n%zu", i);
        note.onset = at((long long)i);
        note.duration = beats(1);
        note.midi = midis[i];
        note.spelled = true;
        note.letter = PITCH_LETTER_C;
        note.accidental = (int)(i % 2);
        assert(line_add_note(line, &note) == REALIZATION_OK);
    }
    return line;
}

static PerformerPart *make_part(const char *id, int staves) {
    PerformerPart *part = NULL;
    assert(performer_part_create(id, "omt.instrument.piano", id, 1, 1, staves, 0, &part) == REALIZATION_OK);
    return part;
}

static ScoringAssignment assign(const char *id, const char *line, const char *part, int staff, int voice) {
    ScoringAssignment a;
    memset(&a, 0, sizeof(a));
    strcpy(a.id, id);
    strcpy(a.line, line);
    strcpy(a.part, part);
    a.whole = true;
    a.staff = staff;
    a.voice = voice;
    return a;
}

static const int SOPRANO[4] = {72, 74, 76, 77};
static const int ALTO[4] = {67, 69, 71, 72};
static const int TENOR[4] = {60, 62, 64, 65};
static const int BASS[4] = {48, 50, 52, 53};

static void test_chorale_one_to_one(void) {
    Line *lines[4] = {make_line("s", LINE_ROLE_MELODY, SOPRANO, 4), make_line("a", LINE_ROLE_HARMONY_LINE, ALTO, 4),
                      make_line("t", LINE_ROLE_HARMONY_LINE, TENOR, 4), make_line("b", LINE_ROLE_BASS, BASS, 4)};
    PerformerPart *parts[4] = {make_part("p1", 1), make_part("p2", 1), make_part("p3", 1), make_part("p4", 1)};
    ScoringAssignment as[4] = {assign("a1", "s", "p1", 1, 1), assign("a2", "a", "p2", 1, 1), assign("a3", "t", "p3", 1, 1),
                               assign("a4", "b", "p4", 1, 1)};
    assert(realization_score(lines, 4, as, 4, parts, 4, "run-1") == REALIZATION_OK);
    const int *pitches[4] = {SOPRANO, ALTO, TENOR, BASS};
    for (size_t p = 0; p < 4; p++) {
        assert(performer_part_note_count(parts[p]) == 4);
        for (size_t i = 0; i < 4; i++) {
            const PerformerNote *n = performer_part_note_at(parts[p], i);
            char expected[32];
            snprintf(expected, sizeof(expected), "%s-n%zu", performer_part_id(parts[p]), i);
            assert(strcmp(n->id, expected) == 0 && n->midi == pitches[p][i] && n->staff == 1 && n->voice == 1);
            assert(musical_time_same(n->onset, at((long long)i)) && duration_same(n->duration, beats(1)));
            assert(n->spelled && n->letter == PITCH_LETTER_C && n->accidental == (int)(i % 2));
            assert(n->derivation.source_count == 1 && strcmp(n->derivation.sources[0], expected + 3) == 0);
            assert(strcmp(n->derivation.operation, "score") == 0 && strcmp(n->derivation.run, "run-1") == 0);
            assert(!n->edited && !n->detached && !n->review);
        }
    }
    for (int i = 0; i < 4; i++) {
        line_destroy(lines[i]);
        performer_part_destroy(parts[i]);
    }
}

static void test_chorale_one_part(void) {
    Line *lines[4] = {make_line("s", LINE_ROLE_MELODY, SOPRANO, 4), make_line("a", LINE_ROLE_HARMONY_LINE, ALTO, 4),
                      make_line("t", LINE_ROLE_HARMONY_LINE, TENOR, 4), make_line("b", LINE_ROLE_BASS, BASS, 4)};
    PerformerPart *piano = make_part("piano", 2);
    ScoringAssignment as[4] = {assign("a1", "s", "piano", 1, 1), assign("a2", "a", "piano", 1, 2), assign("a3", "t", "piano", 2, 1),
                               assign("a4", "b", "piano", 2, 2)};
    assert(realization_score(lines, 4, as, 4, &piano, 1, "") == REALIZATION_OK);
    assert(performer_part_note_count(piano) == 16);
    /* by onset, ties by assignment order: soprano, alto, tenor, bass at each beat */
    for (size_t i = 0; i < 16; i++) {
        const PerformerNote *n = performer_part_note_at(piano, i);
        char expected[32];
        snprintf(expected, sizeof(expected), "piano-n%zu", i / 4);
        assert(strncmp(n->id, expected, strlen(expected)) == 0);
        assert(n->staff == (i % 4 < 2 ? 1 : 2) && n->voice == (int)(i % 2) + 1);
        assert(musical_time_same(n->onset, at((long long)(i / 4))));
    }
    /* four lines carry the same note ids n0..n3: the first of each id keeps it, the others get -2, -3 and -4 */
    assert(strcmp(performer_part_note_at(piano, 0)->id, "piano-n0") == 0);
    assert(strcmp(performer_part_note_at(piano, 1)->id, "piano-n0-2") == 0);
    assert(strcmp(performer_part_note_at(piano, 2)->id, "piano-n0-3") == 0);
    assert(strcmp(performer_part_note_at(piano, 3)->id, "piano-n0-4") == 0);
    assert(performer_part_note_at(piano, 0)->midi == 72 && performer_part_note_at(piano, 1)->midi == 67 &&
           performer_part_note_at(piano, 2)->midi == 60 && performer_part_note_at(piano, 3)->midi == 48);
    for (int i = 0; i < 4; i++) {
        line_destroy(lines[i]);
    }
    performer_part_destroy(piano);
}

static void test_octave_and_doubling(void) {
    Line *melody = make_line("m", LINE_ROLE_MELODY, SOPRANO, 4);
    PerformerPart *flute = make_part("flute", 1);
    PerformerPart *piccolo = make_part("piccolo", 1);
    PerformerPart *parts[2] = {flute, piccolo};
    ScoringAssignment as[2] = {assign("a1", "m", "flute", 1, 1), assign("a2", "m", "piccolo", 1, 1)};
    as[1].transform.kind = SCORING_TRANSFORM_OCTAVE;
    as[1].transform.octaves = 1;
    assert(realization_score(&melody, 1, as, 2, parts, 2, "r") == REALIZATION_OK);
    assert(performer_part_note_count(flute) == 4 && performer_part_note_count(piccolo) == 4);
    for (size_t i = 0; i < 4; i++) {
        const PerformerNote *low = performer_part_note_at(flute, i);
        const PerformerNote *high = performer_part_note_at(piccolo, i);
        assert(low->midi == SOPRANO[i] && high->midi == SOPRANO[i] + 12);
        assert(high->spelled && high->letter == low->letter && high->accidental == low->accidental);
        assert(strcmp(low->id, high->id) != 0 && strcmp(high->derivation.sources[0], low->derivation.sources[0]) == 0);
    }
    /* an octave transform of zero or of NONE changes nothing */
    as[1].transform.octaves = -2;
    assert(realization_score(&melody, 1, as, 2, parts, 2, "r") == REALIZATION_OK);
    assert(performer_part_note_at(piccolo, 0)->midi == SOPRANO[0] - 24);
    line_destroy(melody);
    performer_part_destroy(flute);
    performer_part_destroy(piccolo);
}

static void test_hand_off(void) {
    static const int eight[8] = {60, 62, 64, 65, 67, 69, 71, 72};
    Line *line = make_line("m", LINE_ROLE_MELODY, eight, 8);
    PerformerPart *first = make_part("first", 1);
    PerformerPart *second = make_part("second", 1);
    PerformerPart *parts[2] = {first, second};
    ScoringAssignment as[2] = {assign("a1", "m", "first", 1, 1), assign("a2", "m", "second", 1, 1)};
    as[0].whole = false;
    as[0].span.start = at(0);
    as[0].span.end = at(4);
    as[1].whole = false;
    as[1].span.start = at(4);
    as[1].span.end = at(8);
    assert(realization_score(&line, 1, as, 2, parts, 2, "r") == REALIZATION_OK);
    assert(performer_part_note_count(first) == 4 && performer_part_note_count(second) == 4);
    assert(performer_part_note_at(first, 3)->midi == 65 && performer_part_note_at(second, 0)->midi == 67);
    line_destroy(line);

    /* a long note that starts in the span is not clipped at its end */
    Line *long_line = NULL;
    assert(line_create("m", LINE_ROLE_MELODY, NULL, &long_line) == REALIZATION_OK);
    LineNote n;
    memset(&n, 0, sizeof(n));
    strcpy(n.id, "held");
    n.onset = at(3);
    n.duration = beats(3);
    n.midi = 60;
    assert(line_add_note(long_line, &n) == REALIZATION_OK);
    assert(realization_score(&long_line, 1, as, 2, parts, 2, "r") == REALIZATION_OK);
    assert(performer_part_note_count(first) == 1 && performer_part_note_count(second) == 0);
    assert(duration_same(performer_part_note_at(first, 0)->duration, beats(3)));
    line_destroy(long_line);
    performer_part_destroy(first);
    performer_part_destroy(second);
}

static void test_regeneration(void) {
    Line *line = make_line("m", LINE_ROLE_MELODY, SOPRANO, 4);
    PerformerPart *part = make_part("p", 1);
    ScoringAssignment a = assign("a1", "m", "p", 1, 1);
    assert(realization_score(&line, 1, &a, 1, &part, 1, "r1") == REALIZATION_OK);
    /* a person edits n1 and adds an independent note; n2 is cut from the line */
    assert(performer_part_set_note_flags(part, "p-n1", true, false) == REALIZATION_OK);
    assert(performer_part_set_note_flags(part, "p-n2", false, true) == REALIZATION_OK);
    assert(performer_part_set_note_flags(part, "p-zzz", true, false) == REALIZATION_ERR_UNKNOWN_REFERENCE);
    PerformerNote own;
    memset(&own, 0, sizeof(own));
    strcpy(own.id, "mine");
    own.onset = at(1);
    own.duration = beats(1);
    own.midi = 40;
    own.staff = 1;
    own.voice = 2;
    assert(performer_part_add_note(part, &own) == REALIZATION_OK);
    assert(performer_part_add_note(part, &own) == REALIZATION_ERR_DUPLICATE_ID);

    /* the line changes: n1 gets another pitch, n2 is gone, n4 arrives */
    int changed[5] = {72, 75, 0, 77, 79};
    Line *newer = NULL;
    assert(line_create("m", LINE_ROLE_MELODY, NULL, &newer) == REALIZATION_OK);
    for (int i = 0; i < 5; i++) {
        if (i == 2) {
            continue;
        }
        LineNote n;
        memset(&n, 0, sizeof(n));
        snprintf(n.id, sizeof(n.id), "n%d", i);
        n.onset = at(i);
        n.duration = beats(1);
        n.midi = changed[i];
        assert(line_add_note(newer, &n) == REALIZATION_OK);
    }
    assert(realization_score(&newer, 1, &a, 1, &part, 1, "r2") == REALIZATION_OK);
    /* n0, n3, n4 regenerated; n1 edited and kept; n2 detached and kept; mine kept */
    assert(performer_part_note_count(part) == 6);
    const char *ids[6] = {"p-n0", "p-n1", "mine", "p-n2", "p-n3", "p-n4"};
    for (size_t i = 0; i < 6; i++) {
        assert(strcmp(performer_part_note_at(part, i)->id, ids[i]) == 0);
    }
    const PerformerNote *edited = performer_part_note_at(part, 1);
    assert(edited->edited && edited->review && edited->midi == SOPRANO[1] && strcmp(edited->derivation.run, "r1") == 0);
    const PerformerNote *detached = performer_part_note_at(part, 3);
    assert(detached->detached && detached->review && detached->midi == SOPRANO[2]);
    assert(!performer_part_note_at(part, 2)->review && performer_part_note_at(part, 2)->midi == 40);
    const PerformerNote *remade = performer_part_note_at(part, 4);
    assert(remade->midi == 77 && !remade->review && strcmp(remade->derivation.run, "r2") == 0);
    assert(performer_part_note_at(part, 0)->midi == 72 && strcmp(performer_part_note_at(part, 0)->derivation.run, "r2") == 0);
    assert(performer_part_note_at(part, 5)->midi == 79);

    /* an edited note whose pitch the line still gives is kept without a review flag */
    Line *same = make_line("m", LINE_ROLE_MELODY, SOPRANO, 4);
    PerformerPart *quiet = make_part("p", 1);
    assert(realization_score(&same, 1, &a, 1, &quiet, 1, "r1") == REALIZATION_OK);
    assert(performer_part_set_note_flags(quiet, "p-n1", true, false) == REALIZATION_OK);
    assert(realization_score(&same, 1, &a, 1, &quiet, 1, "r2") == REALIZATION_OK);
    assert(performer_part_note_count(quiet) == 4 && !performer_part_note_at(quiet, 1)->review &&
           strcmp(performer_part_note_at(quiet, 1)->derivation.run, "r1") == 0);
    line_destroy(same);
    performer_part_destroy(quiet);
    line_destroy(line);
    line_destroy(newer);
    performer_part_destroy(part);
}

static void snapshot_ids(const PerformerPart *part, char out[512]) {
    out[0] = '\0';
    for (size_t i = 0; i < performer_part_note_count(part); i++) {
        strcat(out, performer_part_note_at(part, i)->id);
        strcat(out, ";");
    }
}

static void test_refusals_change_nothing(void) {
    Line *line = make_line("m", LINE_ROLE_MELODY, SOPRANO, 4);
    PerformerPart *part = make_part("p", 1);
    ScoringAssignment a = assign("a1", "m", "p", 1, 1);
    assert(realization_score(&line, 1, &a, 1, &part, 1, "r1") == REALIZATION_OK);
    char before[512], after[512];
    snapshot_ids(part, before);

    ScoringAssignment bad = a;
    bad.transform.kind = SCORING_TRANSFORM_INTERVAL;
    bad.transform.semitones = 4;
    bad.transform.steps = 2;
    assert(realization_score(&line, 1, &bad, 1, &part, 1, "r2") == REALIZATION_ERR_NOT_BUILT);
    snapshot_ids(part, after);
    assert(strcmp(before, after) == 0 && strcmp(performer_part_note_at(part, 0)->derivation.run, "r1") == 0);

    ScoringAssignment two[2] = {a, a};
    assert(realization_score(&line, 1, two, 2, &part, 1, "r2") == REALIZATION_ERR_DUPLICATE_ID);
    bad = a;
    strcpy(bad.line, "nothing");
    assert(realization_score(&line, 1, &bad, 1, &part, 1, "r2") == REALIZATION_ERR_UNKNOWN_REFERENCE);
    bad = a;
    strcpy(bad.part, "nothing");
    assert(realization_score(&line, 1, &bad, 1, &part, 1, "r2") == REALIZATION_ERR_UNKNOWN_REFERENCE);
    bad = a;
    bad.staff = 2;
    assert(realization_score(&line, 1, &bad, 1, &part, 1, "r2") == REALIZATION_ERR_BAD_VALUE);
    bad = a;
    bad.transform.kind = SCORING_TRANSFORM_OCTAVE;
    bad.transform.octaves = 4;
    assert(realization_score(&line, 1, &bad, 1, &part, 1, "r2") == REALIZATION_OK); /* 77 + 48 = 125 still sounds */
    assert(realization_score(&line, 1, &a, 1, &part, 1, "r1") == REALIZATION_OK);
    static const int high_pitches[2] = {100, 60};
    Line *high = make_line("m", LINE_ROLE_MELODY, high_pitches, 2);
    assert(realization_score(&high, 1, &bad, 1, &part, 1, "r2") == REALIZATION_ERR_BAD_VALUE);
    line_destroy(high);
    bad.transform.octaves = 5;
    assert(scoring_assignment_check(&bad) == REALIZATION_ERR_BAD_VALUE);
    bad = a;
    bad.voice = 0;
    assert(scoring_assignment_check(&bad) == REALIZATION_ERR_BAD_VALUE);
    bad = a;
    bad.whole = false;
    bad.span.start = at(2);
    bad.span.end = at(2);
    assert(scoring_assignment_check(&bad) == REALIZATION_ERR_BAD_VALUE);
    strcpy(bad.id, "9");
    assert(scoring_assignment_check(&bad) == REALIZATION_ERR_BAD_ID && scoring_assignment_check(NULL) == REALIZATION_ERR_NULL);
    assert(realization_score(&line, 1, &a, 1, &part, 1, NULL) == REALIZATION_ERR_NULL);
    assert(realization_score(&line, 1, &a, 1, &part, 1, "tab\t") == REALIZATION_ERR_BAD_VALUE);
    Line *twin[2] = {line, line};
    assert(realization_score(twin, 2, &a, 1, &part, 1, "r2") == REALIZATION_ERR_DUPLICATE_ID);
    snapshot_ids(part, after);
    assert(strcmp(before, after) == 0 && performer_part_note_count(part) == 4);

    /* a generated id longer than an id may be */
    PerformerPart *wide = NULL;
    char long_id[REALIZATION_ID_MAX];
    memset(long_id, 'p', 62);
    long_id[62] = '\0';
    assert(performer_part_create(long_id, "omt.instrument.piano", "wide", 1, 1, 1, 0, &wide) == REALIZATION_OK);
    ScoringAssignment w = assign("a1", "m", long_id, 1, 1);
    assert(realization_score(&line, 1, &w, 1, &wide, 1, "r") == REALIZATION_ERR_BAD_VALUE && performer_part_note_count(wide) == 0);
    performer_part_destroy(wide);
    line_destroy(line);
    performer_part_destroy(part);
}

static void test_two_runs_agree(void) {
    Line *lines[2] = {make_line("s", LINE_ROLE_MELODY, SOPRANO, 4), make_line("b", LINE_ROLE_BASS, BASS, 4)};
    PerformerPart *x = make_part("piano", 2);
    PerformerPart *y = make_part("piano", 2);
    ScoringAssignment as[3] = {assign("a1", "s", "piano", 1, 1), assign("a2", "b", "piano", 2, 1), assign("a3", "s", "piano", 2, 2)};
    assert(realization_score(lines, 2, as, 3, &x, 1, "r") == REALIZATION_OK);
    assert(realization_score(lines, 2, as, 3, &y, 1, "r") == REALIZATION_OK);
    assert(realization_score(lines, 2, as, 3, &y, 1, "r") == REALIZATION_OK);
    char a[512], b[512];
    snapshot_ids(x, a);
    snapshot_ids(y, b);
    assert(strcmp(a, b) == 0 && performer_part_note_count(x) == 12);
    /* the soprano twice in one part: the second copy gets -2 */
    assert(strstr(a, "piano-n0;") != NULL && strstr(a, "piano-n0-2;") != NULL && strstr(a, "piano-n0-3;") != NULL);
    for (size_t i = 0; i < 12; i++) {
        const PerformerNote *p = performer_part_note_at(x, i), *q = performer_part_note_at(y, i);
        assert(p->midi == q->midi && p->staff == q->staff && p->voice == q->voice && musical_time_same(p->onset, q->onset));
    }
    line_destroy(lines[0]);
    line_destroy(lines[1]);
    performer_part_destroy(x);
    performer_part_destroy(y);
}

static void test_empty_and_parts_without_assignments(void) {
    PerformerPart *part = make_part("p", 1);
    assert(realization_score(NULL, 0, NULL, 0, &part, 1, "r") == REALIZATION_OK && performer_part_note_count(part) == 0);
    PerformerNote own;
    memset(&own, 0, sizeof(own));
    strcpy(own.id, "mine");
    own.onset = at(0);
    own.duration = beats(1);
    own.midi = 60;
    own.staff = 1;
    own.voice = 1;
    assert(performer_part_add_note(part, &own) == REALIZATION_OK);
    PerformerNote derived = own;
    strcpy(derived.id, "p-gone");
    strcpy(derived.derivation.sources[0], "gone");
    derived.derivation.source_count = 1;
    assert(performer_part_add_note(part, &derived) == REALIZATION_OK);
    assert(realization_score(NULL, 0, NULL, 0, &part, 1, "r") == REALIZATION_OK);
    assert(performer_part_note_count(part) == 1 && strcmp(performer_part_note_at(part, 0)->id, "mine") == 0);
    performer_part_destroy(part);
}

int main(void) {
    test_chorale_one_to_one();
    test_chorale_one_part();
    test_octave_and_doubling();
    test_hand_off();
    test_regeneration();
    test_refusals_change_nothing();
    test_two_runs_agree();
    test_empty_and_parts_without_assignments();
    puts("realization_scoring: all tests passed");
    return 0;
}
