#include "time/tuplet.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>

static Tuplet *make(unsigned actual, unsigned normal) {
    Tuplet *tuplet = NULL;
    assert(tuplet_create(actual, normal, &tuplet) == TUPLET_OK);
    return tuplet;
}

static Duration *duration(long numerator, unsigned long denominator) {
    Duration *d = NULL;
    assert(duration_create(numerator, denominator, &d) == DURATION_OK);
    return d;
}

static void assert_duration(const Duration *d, long numerator, unsigned long denominator) {
    assert(duration_numerator(d) == numerator);
    assert(duration_denominator(d) == denominator);
}

static void test_create_rejects_invalid(void) {
    Tuplet *tuplet = NULL;
    assert(tuplet_create(0, 2, &tuplet) == TUPLET_ERR_ACTUAL_OUT_OF_RANGE);
    assert(tuplet_create(1, 2, &tuplet) == TUPLET_ERR_ACTUAL_OUT_OF_RANGE);
    assert(tuplet_create(TUPLET_MAX_COUNT + 1, 2, &tuplet) == TUPLET_ERR_ACTUAL_OUT_OF_RANGE);
    assert(tuplet_create(3, 0, &tuplet) == TUPLET_ERR_NORMAL_OUT_OF_RANGE);
    assert(tuplet_create(3, TUPLET_MAX_COUNT + 1, &tuplet) == TUPLET_ERR_NORMAL_OUT_OF_RANGE);
    assert(tuplet_create(3, 3, &tuplet) == TUPLET_ERR_EQUAL_COUNTS);
    assert(tuplet_create(2, 2, &tuplet) == TUPLET_ERR_EQUAL_COUNTS);
    assert(tuplet == NULL);
}

static void test_accessors_and_bounds(void) {
    Tuplet *triplet = make(3, 2);
    assert(tuplet_actual(triplet) == 3 && tuplet_normal(triplet) == 2);
    tuplet_destroy(triplet);

    Tuplet *duplet = make(2, 3);
    assert(tuplet_actual(duplet) == 2 && tuplet_normal(duplet) == 3);
    tuplet_destroy(duplet);

    /* normal may be larger or smaller than actual; both bounds accepted */
    Tuplet *widest = make(TUPLET_MAX_COUNT, 1);
    tuplet_destroy(widest);
    Tuplet *narrowest = make(2, TUPLET_MAX_COUNT);
    tuplet_destroy(narrowest);
}

/* Not reduced: a sextuplet 6:4 is not a triplet 3:2. */
static void test_equality_does_not_reduce(void) {
    Tuplet *triplet = make(3, 2);
    Tuplet *also_triplet = make(3, 2);
    Tuplet *sextuplet = make(6, 4);
    Tuplet *duplet = make(2, 3);

    assert(tuplet_equals(triplet, also_triplet));
    assert(!tuplet_equals(triplet, sextuplet));
    assert(!tuplet_equals(triplet, duplet));
    assert(!tuplet_equals(triplet, NULL));
    assert(!tuplet_equals(NULL, triplet));

    tuplet_destroy(triplet);
    tuplet_destroy(also_triplet);
    tuplet_destroy(sextuplet);
    tuplet_destroy(duplet);
}

/* Three eighth notes in a triplet are 1/12 each. */
static void test_note_duration(void) {
    Tuplet *triplet = make(3, 2);
    Duration *eighth = duration(1, 8);
    Duration *each = NULL;
    assert(tuplet_note_duration(triplet, eighth, &each) == DURATION_OK);
    assert_duration(each, 1, 12);
    duration_destroy(each);
    duration_destroy(eighth);

    /* a triplet of quarter notes: 1/4 * 2/3 = 1/6 each */
    Duration *quarter = duration(1, 4);
    assert(tuplet_note_duration(triplet, quarter, &each) == DURATION_OK);
    assert_duration(each, 1, 6);
    duration_destroy(each);
    duration_destroy(quarter);
    tuplet_destroy(triplet);

    /* a duplet of eighths: 1/8 * 3/2 = 3/16 each */
    Tuplet *duplet = make(2, 3);
    eighth = duration(1, 8);
    assert(tuplet_note_duration(duplet, eighth, &each) == DURATION_OK);
    assert_duration(each, 3, 16);
    duration_destroy(each);
    duration_destroy(eighth);
    tuplet_destroy(duplet);
}

/* A group lasts as long as the notes it replaces. */
static void test_span(void) {
    Tuplet *triplet = make(3, 2);
    Duration *span = NULL;

    Duration *eighth = duration(1, 8); /* three eighths = one quarter */
    assert(tuplet_span(triplet, eighth, &span) == DURATION_OK);
    assert_duration(span, 1, 4);
    duration_destroy(span);
    duration_destroy(eighth);

    Duration *sixteenth = duration(1, 16); /* three sixteenths = one eighth */
    assert(tuplet_span(triplet, sixteenth, &span) == DURATION_OK);
    assert_duration(span, 1, 8);
    duration_destroy(span);
    duration_destroy(sixteenth);

    Duration *quarter = duration(1, 4); /* three quarters = one half (multi-beat) */
    assert(tuplet_span(triplet, quarter, &span) == DURATION_OK);
    assert_duration(span, 1, 2);
    duration_destroy(span);
    duration_destroy(quarter);
    tuplet_destroy(triplet);

    /* a duplet of dotted-quarter-length notes spans three of them */
    Tuplet *duplet = make(2, 3);
    Duration *dotted_quarter = duration(3, 8);
    assert(tuplet_span(duplet, dotted_quarter, &span) == DURATION_OK);
    assert_duration(span, 9, 8);
    duration_destroy(span);
    duration_destroy(dotted_quarter);
    tuplet_destroy(duplet);
}

/* actual notes of the scaled length fill the span exactly. */
static void test_notes_fill_span(void) {
    unsigned counts[][2] = {{3, 2}, {2, 3}, {5, 4}, {6, 4}, {7, 8}};
    for (int i = 0; i < 5; i++) {
        Tuplet *tuplet = make(counts[i][0], counts[i][1]);
        Duration *nominal = duration(1, 8);
        Duration *each = NULL;
        Duration *span = NULL;
        assert(tuplet_note_duration(tuplet, nominal, &each) == DURATION_OK);
        assert(tuplet_span(tuplet, nominal, &span) == DURATION_OK);

        /* each * actual == span */
        assert(duration_numerator(each) * (long)tuplet_actual(tuplet) * (long)duration_denominator(span) ==
               duration_numerator(span) * (long)duration_denominator(each));

        duration_destroy(each);
        duration_destroy(span);
        duration_destroy(nominal);
        tuplet_destroy(tuplet);
    }
}

int main(void) {
    test_create_rejects_invalid();
    test_accessors_and_bounds();
    test_equality_does_not_reduce();
    test_note_duration();
    test_span();
    test_notes_fill_span();
    printf("test_tuplet: ok\n");
    return 0;
}
