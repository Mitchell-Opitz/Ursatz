#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "borrowed_rules.h"
#include "counting_rules.h"
#include "meter_test_util.h"
#include "rule_check.h"

static Tuplet *make(unsigned actual, unsigned normal) {
    Tuplet *tuplet = NULL;
    assert(tuplet_create(actual, normal, &tuplet) == TUPLET_OK);
    return tuplet;
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_meter_borrowed_rule, standard_meter_borrowed_constraint,
                                    STANDARD_METER_BORROWED_RULE_COUNT);
}

/* A triplet is borrowed in simple meter, a duplet in compound; in its own meter it is not. */
static void test_borrowed(void) {
    Tuplet *triplet = make(3, 2);
    Tuplet *duplet = make(2, 3);
    Tuplet *quintuplet = make(5, 4);

    assert(standard_meter_tuplet_borrowed(triplet, METER_TYPE_SIMPLE));
    assert(standard_meter_tuplet_borrowed(duplet, METER_TYPE_COMPOUND));

    assert(!standard_meter_tuplet_borrowed(triplet, METER_TYPE_COMPOUND));
    assert(!standard_meter_tuplet_borrowed(duplet, METER_TYPE_SIMPLE));
    assert(!standard_meter_tuplet_borrowed(quintuplet, METER_TYPE_SIMPLE));
    assert(!standard_meter_tuplet_borrowed(quintuplet, METER_TYPE_COMPOUND));
    assert(!standard_meter_tuplet_borrowed(triplet, METER_TYPE_UNCLASSIFIED));
    assert(!standard_meter_tuplet_borrowed(NULL, METER_TYPE_SIMPLE));

    tuplet_destroy(triplet);
    tuplet_destroy(duplet);
    tuplet_destroy(quintuplet);
}

static bool level_of(const TimeSignature *time_signature, long numerator, unsigned long denominator,
                     TupletLevel *level) {
    Duration *span = make_duration(numerator, denominator);
    bool result = standard_meter_tuplet_level(time_signature, span, level);
    duration_destroy(span);
    return result;
}

/* Beat, subdivision, and multi-beat triplets in 2/4 (Example 1). */
static void test_triplet_levels_in_simple_meter(void) {
    TimeSignature *two_four = make_signature(2, 2);
    TupletLevel level;

    assert(level_of(two_four, 1, 4, &level) && level == TUPLET_LEVEL_BEAT);         /* eighth triplet */
    assert(level_of(two_four, 1, 8, &level) && level == TUPLET_LEVEL_SUBDIVISION); /* sixteenth triplet = an eighth */
    assert(level_of(two_four, 1, 2, &level) && level == TUPLET_LEVEL_MULTI_BEAT);   /* quarter triplet = a half */
    assert(level_of(two_four, 1, 16, &level) && level == TUPLET_LEVEL_SUBDIVISION);

    time_signature_destroy(two_four);
}

/* Duplets in a compound beat: 6/8's beat is 3/8. */
static void test_duplet_levels_in_compound_meter(void) {
    TimeSignature *six_eight = make_signature(6, 3);
    TupletLevel level;

    assert(level_of(six_eight, 3, 8, &level) && level == TUPLET_LEVEL_BEAT);
    assert(level_of(six_eight, 3, 16, &level) && level == TUPLET_LEVEL_SUBDIVISION);
    assert(level_of(six_eight, 3, 4, &level) && level == TUPLET_LEVEL_MULTI_BEAT);

    time_signature_destroy(six_eight);
}

static void test_level_declines(void) {
    TimeSignature *four_four = make_signature(4, 2);
    TimeSignature *five_eight = make_signature(5, 3);
    TupletLevel level = TUPLET_LEVEL_BEAT;

    assert(!level_of(four_four, 1, 3, &level));  /* neither divides the beat nor is whole beats */
    assert(!level_of(four_four, 3, 8, &level));  /* one and a half beats */
    assert(!level_of(four_four, 0, 1, &level));  /* empty */
    assert(!level_of(five_eight, 1, 4, &level)); /* unclassified: no beat */
    assert(!standard_meter_tuplet_level(NULL, NULL, &level));
    assert(level == TUPLET_LEVEL_BEAT);

    time_signature_destroy(four_four);
    time_signature_destroy(five_eight);
}

/* Triplets at any level are typical; duplets at the beat or a subdivision. */
static void test_typical_level(void) {
    Tuplet *triplet = make(3, 2);
    Tuplet *duplet = make(2, 3);
    Tuplet *quintuplet = make(5, 4);

    assert(standard_meter_tuplet_level_typical(triplet, TUPLET_LEVEL_BEAT));
    assert(standard_meter_tuplet_level_typical(triplet, TUPLET_LEVEL_SUBDIVISION));
    assert(standard_meter_tuplet_level_typical(triplet, TUPLET_LEVEL_MULTI_BEAT));

    assert(standard_meter_tuplet_level_typical(duplet, TUPLET_LEVEL_BEAT));
    assert(standard_meter_tuplet_level_typical(duplet, TUPLET_LEVEL_SUBDIVISION));
    assert(!standard_meter_tuplet_level_typical(duplet, TUPLET_LEVEL_MULTI_BEAT));

    /* no convention is stated for other tuplets */
    assert(!standard_meter_tuplet_level_typical(quintuplet, TUPLET_LEVEL_BEAT));
    assert(!standard_meter_tuplet_level_typical(NULL, TUPLET_LEVEL_BEAT));

    tuplet_destroy(triplet);
    tuplet_destroy(duplet);
    tuplet_destroy(quintuplet);
}

/* Triplets are counted "1 la li", duplets "1 +". */
static void test_borrowed_counting(void) {
    Tuplet *triplet = make(3, 2);
    Tuplet *duplet = make(2, 3);
    Tuplet *quintuplet = make(5, 4);
    const char *syllable = "unset";

    assert(standard_meter_tuplet_count_syllable(triplet, 0, &syllable) && syllable == NULL);
    assert(standard_meter_tuplet_count_syllable(triplet, 1, &syllable) && strcmp(syllable, "la") == 0);
    assert(standard_meter_tuplet_count_syllable(triplet, 2, &syllable) && strcmp(syllable, "li") == 0);

    assert(standard_meter_tuplet_count_syllable(duplet, 0, &syllable) && syllable == NULL);
    assert(standard_meter_tuplet_count_syllable(duplet, 1, &syllable) && strcmp(syllable, "+") == 0);

    syllable = "unset";
    assert(!standard_meter_tuplet_count_syllable(triplet, 3, &syllable));  /* past the count */
    assert(!standard_meter_tuplet_count_syllable(duplet, 2, &syllable));
    assert(!standard_meter_tuplet_count_syllable(quintuplet, 0, &syllable)); /* not named */
    assert(!standard_meter_tuplet_count_syllable(NULL, 0, &syllable));
    assert(strcmp(syllable, "unset") == 0);

    tuplet_destroy(triplet);
    tuplet_destroy(duplet);
    tuplet_destroy(quintuplet);
}

/* The borrowed syllables are the other meter's own division syllables. */
static void test_borrowed_matches_native_division(void) {
    Tuplet *triplet = make(3, 2);
    TimeSignature *six_eight = make_signature(6, 3);
    const char *syllable;

    /* a triplet's second and third notes are the compound division at 1/3 and 2/3 of the beat */
    Duration *la = make_duration(1, 8);
    Duration *li = make_duration(1, 4);
    StandardMeterCount count;
    assert(standard_meter_count(six_eight, la, &count));
    assert(standard_meter_tuplet_count_syllable(triplet, 1, &syllable) && strcmp(syllable, count.syllable) == 0);
    assert(standard_meter_count(six_eight, li, &count));
    assert(standard_meter_tuplet_count_syllable(triplet, 2, &syllable) && strcmp(syllable, count.syllable) == 0);

    duration_destroy(la);
    duration_destroy(li);
    time_signature_destroy(six_eight);
    tuplet_destroy(triplet);
}

int main(void) {
    test_rules_reference_framework();
    test_borrowed();
    test_triplet_levels_in_simple_meter();
    test_duplet_levels_in_compound_meter();
    test_level_declines();
    test_typical_level();
    test_borrowed_counting();
    test_borrowed_matches_native_division();
    printf("test_borrowed_rules (standard-meter): ok\n");
    return 0;
}
