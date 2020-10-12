#include <assert.h>
#include <stdio.h>

#include "rule_check.h"
#include "tuplet_rules.h"

static Tuplet *make(unsigned actual, unsigned normal) {
    Tuplet *tuplet = NULL;
    assert(tuplet_create(actual, normal, &tuplet) == TUPLET_OK);
    return tuplet;
}

static NoteValue *value(int exponent) {
    NoteValue *note_value = NULL;
    assert(note_value_create(exponent, &note_value) == NOTE_VALUE_OK);
    return note_value;
}

static void assert_duration(const Duration *duration, long numerator, unsigned long denominator) {
    assert(duration_numerator(duration) == numerator);
    assert(duration_denominator(duration) == denominator);
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_rhythm_tuplet_rule, standard_rhythm_tuplet_constraint,
                                    STANDARD_RHYTHM_TUPLET_RULE_COUNT);
}

/* Three in the time of two is a triplet, two in the time of three a duplet. */
static void test_names(void) {
    Tuplet *triplet = make(3, 2);
    Tuplet *duplet = make(2, 3);
    Tuplet *quintuplet = make(5, 4);
    Tuplet *sextuplet = make(6, 4);
    Tuplet *three_in_four = make(3, 4);

    assert(standard_rhythm_tuplet_kind(triplet) == TUPLET_KIND_TRIPLET);
    assert(standard_rhythm_tuplet_kind(duplet) == TUPLET_KIND_DUPLET);
    assert(standard_rhythm_tuplet_kind(quintuplet) == TUPLET_KIND_OTHER);
    assert(standard_rhythm_tuplet_kind(sextuplet) == TUPLET_KIND_OTHER); /* 6:4 is not 3:2 */
    assert(standard_rhythm_tuplet_kind(three_in_four) == TUPLET_KIND_OTHER);
    assert(standard_rhythm_tuplet_kind(NULL) == TUPLET_KIND_OTHER);

    tuplet_destroy(triplet);
    tuplet_destroy(duplet);
    tuplet_destroy(quintuplet);
    tuplet_destroy(sextuplet);
    tuplet_destroy(three_in_four);
}

/* A 3 over a triplet, a 2 over a duplet. */
static void test_number(void) {
    Tuplet *triplet = make(3, 2);
    Tuplet *duplet = make(2, 3);
    Tuplet *quintuplet = make(5, 4);

    assert(standard_rhythm_tuplet_number(triplet) == 3);
    assert(standard_rhythm_tuplet_number(duplet) == 2);
    assert(standard_rhythm_tuplet_number(quintuplet) == 5);

    tuplet_destroy(triplet);
    tuplet_destroy(duplet);
    tuplet_destroy(quintuplet);
}

/* Three eighths are a quarter, three sixteenths an eighth, three quarters a half. */
static void test_triplet_spans(void) {
    Tuplet *triplet = make(3, 2);
    int exponents[] = {3, 4, 2};
    long spans_n[] = {1, 1, 1};
    unsigned long spans_d[] = {4, 8, 2};
    for (int i = 0; i < 3; i++) {
        NoteValue *written = value(exponents[i]);
        Duration *span = NULL;
        assert(standard_rhythm_tuplet_span(triplet, written, &span));
        assert_duration(span, spans_n[i], spans_d[i]);
        duration_destroy(span);
        note_value_destroy(written);
    }

    /* each note of the eighth triplet is a twelfth of a whole note */
    NoteValue *eighth = value(3);
    Duration *each = NULL;
    assert(standard_rhythm_tuplet_note_duration(triplet, eighth, &each));
    assert_duration(each, 1, 12);
    duration_destroy(each);
    note_value_destroy(eighth);
    tuplet_destroy(triplet);
}

/* A duplet of dotted eighths spans three dotted eighths. */
static void test_duplet_span_and_dots(void) {
    Tuplet *duplet = make(2, 3);
    NoteValue *dotted_eighth = NULL;
    assert(note_value_create_dotted(3, 1, &dotted_eighth) == NOTE_VALUE_OK);

    Duration *span = NULL;
    assert(standard_rhythm_tuplet_span(duplet, dotted_eighth, &span));
    assert_duration(span, 9, 16); /* three dotted eighths */
    duration_destroy(span);

    /* a duplet of eighths in a dotted-quarter beat: two notes of 3/16 fill 3/8 */
    NoteValue *eighth = value(3);
    Duration *each = NULL;
    assert(standard_rhythm_tuplet_note_duration(duplet, eighth, &each));
    assert_duration(each, 3, 16);
    assert(standard_rhythm_tuplet_span(duplet, eighth, &span));
    assert_duration(span, 3, 8);
    duration_destroy(each);
    duration_destroy(span);

    note_value_destroy(eighth);
    note_value_destroy(dotted_eighth);
    tuplet_destroy(duplet);
}

static void test_declines(void) {
    Tuplet *triplet = make(3, 2);
    NoteValue *eighth = value(3);
    Duration *out = NULL;

    assert(!standard_rhythm_tuplet_span(NULL, eighth, &out));
    assert(!standard_rhythm_tuplet_span(triplet, NULL, &out));
    assert(!standard_rhythm_tuplet_note_duration(NULL, eighth, &out));
    assert(!standard_rhythm_tuplet_note_duration(triplet, NULL, &out));
    assert(out == NULL);

    note_value_destroy(eighth);
    tuplet_destroy(triplet);
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_number();
    test_triplet_spans();
    test_duplet_span_and_dots();
    test_declines();
    printf("test_tuplet_rules (standard-rhythm): ok\n");
    return 0;
}
