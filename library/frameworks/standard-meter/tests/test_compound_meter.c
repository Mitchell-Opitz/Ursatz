#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "anacrusis_rules.h"
#include "beaming_rules.h"
#include "beat_grid.h"
#include "beat_tie_rules.h"
#include "counting_rules.h"
#include "measure_rules.h"
#include "meter_class_rules.h"
#include "meter_test_util.h"

static NoteValue *value(int exponent) {
    NoteValue *note_value = NULL;
    assert(note_value_create(exponent, &note_value) == NOTE_VALUE_OK);
    return note_value;
}

static void assert_count(const TimeSignature *time_signature, long numerator, unsigned long denominator,
                         const char *expected, unsigned beat) {
    Duration *offset = make_duration(numerator, denominator);
    StandardMeterCount count;
    assert(standard_meter_count(time_signature, offset, &count));
    assert(count.beat == beat);
    char text[16];
    assert(standard_meter_count_text(&count, true, text, sizeof(text)));
    assert(strcmp(text, expected) == 0);
    duration_destroy(offset);
}

/* 6/8: 1 ta la ta li ta 2 ta la ta li ta, with la and li on the eighths. */
static void test_counting_six_eight(void) {
    TimeSignature *six_eight = make_signature(6, 3);
    const char *expected[] = {"1", "ta", "la", "ta", "li", "ta", "2", "ta", "la", "ta", "li", "ta"};
    for (long i = 0; i < 12; i++) {
        assert_count(six_eight, i, 16, expected[i], (unsigned)(i / 6) + 1);
    }

    /* the eighth-note divisions alone: 1 la li 2 la li */
    assert_count(six_eight, 1, 8, "la", 1);
    assert_count(six_eight, 1, 4, "li", 1);
    assert_count(six_eight, 3, 8, "2", 2);
    assert_count(six_eight, 1, 2, "la", 2);
    assert_count(six_eight, 5, 8, "li", 2);

    StandardMeterCount count;
    Duration *offset = make_duration(1, 8);
    assert(standard_meter_count(six_eight, offset, &count));
    assert(count.parts == STANDARD_METER_COUNT_GRID_COMPOUND && count.position == 2);
    duration_destroy(offset);
    time_signature_destroy(six_eight);
}

/* The same counting at the other division units: a dotted half beat in 6/4, a dotted eighth in 6/16. */
static void test_counting_other_division_units(void) {
    TimeSignature *six_four = make_signature(6, 2);
    assert_count(six_four, 1, 4, "la", 1);
    assert_count(six_four, 1, 2, "li", 1);
    assert_count(six_four, 3, 4, "2", 2);
    assert_count(six_four, 1, 8, "ta", 1);
    time_signature_destroy(six_four);

    TimeSignature *six_sixteen = make_signature(6, 4);
    assert_count(six_sixteen, 1, 16, "la", 1);
    assert_count(six_sixteen, 1, 8, "li", 1);
    assert_count(six_sixteen, 3, 16, "2", 2);
    time_signature_destroy(six_sixteen);
}

/* Compound triple and quadruple have three and four beats. */
static void test_counting_triple_and_quadruple(void) {
    TimeSignature *nine_eight = make_signature(9, 3);
    assert_count(nine_eight, 3, 4, "3", 3);
    assert_count(nine_eight, 7, 8, "li", 3);
    time_signature_destroy(nine_eight);

    TimeSignature *twelve_eight = make_signature(12, 3);
    assert_count(twelve_eight, 9, 8, "4", 4);
    assert_count(twelve_eight, 5, 4, "li", 4);
    time_signature_destroy(twelve_eight);
}

static void test_counting_declines(void) {
    TimeSignature *six_eight = make_signature(6, 3);
    TimeSignature *five_eight = make_signature(5, 3);
    StandardMeterCount count = {99, 99, 99, NULL};

    Duration *end = make_duration(3, 4);
    Duration *finer = make_duration(1, 32); /* half a sixth of a beat */
    Duration *start = make_duration(0, 1);
    assert(!standard_meter_count(six_eight, end, &count));
    assert(!standard_meter_count(six_eight, finer, &count));
    assert(!standard_meter_count(five_eight, start, &count)); /* unclassified: no beat grid */
    assert(count.beat == 99);

    duration_destroy(end);
    duration_destroy(finer);
    duration_destroy(start);
    time_signature_destroy(six_eight);
    time_signature_destroy(five_eight);
}

/* A dot leaves a simple beat uncounted; in compound the dotted note is the beat. */
static void test_dots_and_parentheses(void) {
    assert(standard_meter_dot_silences_beat(METER_TYPE_SIMPLE));
    assert(!standard_meter_dot_silences_beat(METER_TYPE_COMPOUND));
    assert(!standard_meter_dot_silences_beat(METER_TYPE_UNCLASSIFIED));

    /* a rest or tie still parenthesizes in compound: the text API is shared */
    TimeSignature *six_eight = make_signature(6, 3);
    Duration *offset = make_duration(3, 8);
    StandardMeterCount count;
    char text[16];
    assert(standard_meter_count(six_eight, offset, &count));
    assert(standard_meter_count_text(&count, false, text, sizeof(text)));
    assert(strcmp(text, "(2)") == 0);
    duration_destroy(offset);
    time_signature_destroy(six_eight);
}

/* Six sixteenths per beat in 6/8, three in 6/16; a quarter does not divide a 6/8 beat. */
static void test_notes_per_beat(void) {
    NoteValue *sixteenth = value(4);
    NoteValue *eighth = value(3);
    NoteValue *quarter = value(2);
    unsigned long count = 0;

    TimeSignature *six_eight = make_signature(6, 3);
    assert(standard_meter_notes_per_beat(six_eight, sixteenth, &count) && count == 6);
    assert(standard_meter_notes_per_beat(six_eight, eighth, &count) && count == 3);
    count = 9;
    assert(!standard_meter_notes_per_beat(six_eight, quarter, &count));
    assert(count == 9);
    time_signature_destroy(six_eight);

    TimeSignature *six_sixteen = make_signature(6, 4);
    assert(standard_meter_notes_per_beat(six_sixteen, sixteenth, &count) && count == 3);
    assert(!standard_meter_notes_per_beat(six_sixteen, eighth, &count));
    time_signature_destroy(six_sixteen);

    TimeSignature *six_four = make_signature(6, 2);
    assert(standard_meter_notes_per_beat(six_four, quarter, &count) && count == 3);
    assert(standard_meter_notes_per_beat(six_four, eighth, &count) && count == 6);
    time_signature_destroy(six_four);

    /* simple meter is unchanged */
    TimeSignature *four_four = make_signature(4, 2);
    assert(standard_meter_notes_per_beat(four_four, sixteenth, &count) && count == 4);
    time_signature_destroy(four_four);

    TimeSignature *five_eight = make_signature(5, 3);
    assert(!standard_meter_notes_per_beat(five_eight, eighth, &count));
    time_signature_destroy(five_eight);

    note_value_destroy(sixteenth);
    note_value_destroy(eighth);
    note_value_destroy(quarter);
}

static bool within(const TimeSignature *time_signature, long start_n, unsigned long start_d, long length_n,
                   unsigned long length_d) {
    Duration *start = make_duration(start_n, start_d);
    Duration *length = make_duration(length_n, length_d);
    bool result = standard_meter_beam_group_within_beat(time_signature, start, length);
    duration_destroy(start);
    duration_destroy(length);
    return result;
}

/* Beams group by the dotted beat: six sixteenths in 6/8, three in 6/16. */
static void test_beam_groups(void) {
    TimeSignature *six_eight = make_signature(6, 3);
    assert(within(six_eight, 0, 1, 3, 8));  /* six sixteenths, beat 1 */
    assert(within(six_eight, 3, 8, 3, 8));  /* six sixteenths, beat 2 */
    assert(within(six_eight, 1, 4, 1, 8));  /* last eighth of beat 1, ends on the line */
    assert(!within(six_eight, 1, 4, 3, 16)); /* runs past the beat */
    assert(!within(six_eight, 0, 1, 1, 2)); /* four of six sixteenths... into beat 2 */
    assert(!within(six_eight, 3, 4, 1, 16)); /* starts after the measure */
    time_signature_destroy(six_eight);

    TimeSignature *six_sixteen = make_signature(6, 4);
    assert(within(six_sixteen, 0, 1, 3, 16));
    assert(within(six_sixteen, 3, 16, 1, 16));
    assert(!within(six_sixteen, 1, 8, 1, 8));
    time_signature_destroy(six_sixteen);

    /* twelve sixteenths: fours in 4/4 would be three groups, but sixes in 6/8 are two */
    TimeSignature *four_four = make_signature(4, 2);
    assert(within(four_four, 0, 1, 1, 4));
    assert(!within(four_four, 0, 1, 3, 8));
    time_signature_destroy(four_four);

    TimeSignature *five_eight = make_signature(5, 3);
    assert(!within(five_eight, 0, 1, 1, 8)); /* unclassified */
    time_signature_destroy(five_eight);
}

static bool split(const TimeSignature *time_signature, long start_n, unsigned long start_d, long length_n,
                  unsigned long length_d, long *first_n, unsigned long *first_d) {
    Duration *start = make_duration(start_n, start_d);
    Duration *length = make_duration(length_n, length_d);
    Duration *first = NULL;
    bool result = standard_meter_beat_split(time_signature, start, length, &first);
    if (result) {
        *first_n = duration_numerator(first);
        *first_d = duration_denominator(first);
        duration_destroy(first);
    } else {
        assert(first == NULL);
    }
    duration_destroy(start);
    duration_destroy(length);
    return result;
}

/* An eighth starting on the third division that runs a sixteenth into the next beat is split at the beat line. */
static void test_beat_split(void) {
    TimeSignature *six_eight = make_signature(6, 3);
    long n = 0;
    unsigned long d = 0;

    assert(split(six_eight, 1, 4, 3, 16, &n, &d));
    assert(n == 1 && d == 8);
    assert(split(six_eight, 1, 4, 1, 4, &n, &d));
    assert(n == 1 && d == 8);

    assert(!split(six_eight, 1, 8, 1, 8, &n, &d));  /* ends by the next beat line */
    assert(!split(six_eight, 3, 8, 3, 8, &n, &d));  /* starts on beat 2 */
    assert(!split(six_eight, 0, 1, 3, 4, &n, &d));  /* dotted half on beat 1 */
    assert(!split(six_eight, 3, 4, 1, 8, &n, &d));  /* outside the measure */
    time_signature_destroy(six_eight);

    TimeSignature *five_eight = make_signature(5, 3);
    assert(!split(five_eight, 1, 16, 1, 8, &n, &d));
    time_signature_destroy(five_eight);
}

static void test_beat_grid(void) {
    TimeSignature *six_eight = make_signature(6, 3);
    Duration *offset = make_duration(5, 16);
    unsigned index = 99;
    bool on_beat = true;

    assert(standard_meter_beat_at(six_eight, offset, &index, &on_beat));
    assert(index == 0 && !on_beat);
    duration_destroy(offset);

    offset = make_duration(3, 8);
    assert(standard_meter_beat_at(six_eight, offset, &index, &on_beat));
    assert(index == 1 && on_beat);
    duration_destroy(offset);

    offset = make_duration(3, 4);
    index = 99;
    assert(!standard_meter_beat_at(six_eight, offset, &index, &on_beat));
    assert(index == 99);
    duration_destroy(offset);

    Duration *start = NULL;
    assert(standard_meter_beat_start(six_eight, 0, &start));
    assert_duration(start, 0, 1);
    duration_destroy(start);
    assert(standard_meter_beat_start(six_eight, 1, &start));
    assert_duration(start, 3, 8);
    duration_destroy(start);
    assert(standard_meter_beat_start(six_eight, 2, &start)); /* the end of the measure */
    assert_duration(start, 3, 4);
    duration_destroy(start);
    start = NULL;
    assert(!standard_meter_beat_start(six_eight, 3, &start));
    assert(start == NULL);

    TimeSignature *five_eight = make_signature(5, 3);
    assert(!standard_meter_beat_start(five_eight, 0, &start));
    time_signature_destroy(five_eight);
    time_signature_destroy(six_eight);
}

/* 6/8, 9/8, 12/8 are duple, triple, quadruple; a measure is one beat group. */
static void test_measure_grouping(void) {
    unsigned tops[] = {6, 9, 12};
    MeterGrouping groupings[] = {METER_GROUPING_DUPLE, METER_GROUPING_TRIPLE, METER_GROUPING_QUADRUPLE};
    for (int i = 0; i < 3; i++) {
        TimeSignature *time_signature = make_signature(tops[i], 3);
        assert(standard_meter_measure_grouping(time_signature) == groupings[i]);
        time_signature_destroy(time_signature);
    }

    TimeSignature *five_eight = make_signature(5, 3);
    assert(standard_meter_measure_grouping(five_eight) == METER_GROUPING_NONE);
    time_signature_destroy(five_eight);

    /* simple meter is unchanged */
    TimeSignature *three_four = make_signature(3, 2);
    assert(standard_meter_measure_grouping(three_four) == METER_GROUPING_TRIPLE);
    time_signature_destroy(three_four);
}

/* Pickups work through the measure length, so they are the same for compound. */
static void test_pickup_in_compound(void) {
    TimeSignature *six_eight = make_signature(6, 3);
    Duration *eighth = make_duration(1, 8);
    Duration *offset = NULL;
    Duration *final_measure = NULL;

    assert(standard_meter_pickup_valid(six_eight, eighth));
    assert(standard_meter_pickup_start_offset(six_eight, eighth, &offset));
    assert_duration(offset, 5, 8);
    assert(standard_meter_final_measure_duration(six_eight, eighth, &final_measure));
    assert_duration(final_measure, 5, 8);

    duration_destroy(offset);
    duration_destroy(final_measure);
    duration_destroy(eighth);
    time_signature_destroy(six_eight);
}

/* Beats per measure feed beat roles: a 6/8 measure has two beats. */
static void test_beat_roles_in_compound(void) {
    TimeSignature *six_eight = make_signature(6, 3);
    unsigned beats = 0;
    BeatRole role;
    assert(standard_meter_beats_per_measure(six_eight, &beats) && beats == 2);
    assert(standard_meter_beat_role(1, beats, &role) && role == BEAT_ROLE_DOWNBEAT);
    assert(standard_meter_beat_role(2, beats, &role) && role == BEAT_ROLE_UPBEAT);
    time_signature_destroy(six_eight);
}

int main(void) {
    test_counting_six_eight();
    test_counting_other_division_units();
    test_counting_triple_and_quadruple();
    test_counting_declines();
    test_dots_and_parentheses();
    test_notes_per_beat();
    test_beam_groups();
    test_beat_split();
    test_beat_grid();
    test_measure_grouping();
    test_pickup_in_compound();
    test_beat_roles_in_compound();
    printf("test_compound_meter (standard-meter): ok\n");
    return 0;
}
