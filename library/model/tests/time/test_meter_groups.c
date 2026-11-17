#include "model/time/time_signature.h"

#include <assert.h>
#include <stdio.h>

/*
 * Beat groups of a TimeSignature (R-587): the default groups of time_signature_create, explicit groups, the beat length
 * (the one owner of the beat), compound, value equality, copy, the 64-group capacity of the default groups and the
 * limits of time_signature_create_grouped.
 */

static NoteValue *unit(int exponent) {
    NoteValue *value = NULL;
    assert(note_value_create(exponent, &value) == NOTE_VALUE_OK);
    return value;
}

static TimeSignature *plain(unsigned top, int exponent) {
    NoteValue *bottom = unit(exponent);
    TimeSignature *signature = NULL;
    assert(time_signature_create(top, bottom, &signature) == TIME_SIGNATURE_OK);
    note_value_destroy(bottom);
    return signature;
}

static TimeSignature *grouped(const unsigned *groups, size_t count, int exponent) {
    NoteValue *bottom = unit(exponent);
    TimeSignature *signature = NULL;
    assert(time_signature_create_grouped(groups, count, bottom, &signature) == TIME_SIGNATURE_OK);
    note_value_destroy(bottom);
    return signature;
}

static void assert_groups(const TimeSignature *signature, const unsigned *expected, size_t count) {
    assert(time_signature_group_count(signature) == count);
    unsigned sum = 0;
    for (size_t i = 0; i < count; i++) {
        assert(time_signature_group_at(signature, i) == expected[i]);
        sum += expected[i];
    }
    assert(time_signature_group_at(signature, count) == 0); /* past the last */
    assert(time_signature_top_number(signature) == sum);
}

static void assert_beat(const TimeSignature *signature, long long num, long long den) {
    Duration beat;
    assert(time_signature_beat_length(signature, &beat) == DURATION_OK);
    assert(beat.value.num == num && beat.value.den == den);
}

static void test_default_groups(void) {
    /* a top above 3 that 3 divides is groups of three, over any bottom */
    for (int exponent = 0; exponent <= 4; exponent++) {
        TimeSignature *six = plain(6, exponent);
        assert_groups(six, (unsigned[]){3, 3}, 2);
        TimeSignature *nine = plain(9, exponent);
        assert_groups(nine, (unsigned[]){3, 3, 3}, 3);
        TimeSignature *twelve = plain(12, exponent);
        assert_groups(twelve, (unsigned[]){3, 3, 3, 3}, 4);
        time_signature_destroy(six);
        time_signature_destroy(nine);
        time_signature_destroy(twelve);
    }
    /* any other top is one unit per group, 3 included */
    TimeSignature *four = plain(4, 2);
    assert_groups(four, (unsigned[]){1, 1, 1, 1}, 4);
    TimeSignature *three = plain(3, 2);
    assert_groups(three, (unsigned[]){1, 1, 1}, 3);
    TimeSignature *five = plain(5, 3);
    assert_groups(five, (unsigned[]){1, 1, 1, 1, 1}, 5);
    TimeSignature *one = plain(1, 2);
    assert_groups(one, (unsigned[]){1}, 1);
    TimeSignature *fifteen = plain(15, 3);
    assert_groups(fifteen, (unsigned[]){3, 3, 3, 3, 3}, 5);
    time_signature_destroy(four);
    time_signature_destroy(three);
    time_signature_destroy(five);
    time_signature_destroy(one);
    time_signature_destroy(fifteen);
}

static void test_explicit_groups(void) {
    unsigned three_two[] = {3, 2};
    TimeSignature *signature = grouped(three_two, 2, 3);
    assert_groups(signature, three_two, 2);
    assert(time_signature_bottom_unit_exponent(signature) == 3 && time_signature_bottom_number(signature) == 8);
    Duration measure;
    assert(time_signature_measure_duration(signature, &measure) == DURATION_OK);
    assert(measure.value.num == 5 && measure.value.den == 8);
    time_signature_destroy(signature);

    unsigned two_two_three[] = {2, 2, 3};
    signature = grouped(two_two_three, 3, 3);
    assert_groups(signature, two_two_three, 3);
    time_signature_destroy(signature);

    unsigned sixteen[] = {16};
    signature = grouped(sixteen, 1, 4);
    assert_groups(signature, sixteen, 1);
    time_signature_destroy(signature);
}

static void test_beat_length(void) {
    TimeSignature *signature = plain(4, 2);
    assert_beat(signature, 1, 4); /* 4/4: a quarter */
    time_signature_destroy(signature);
    signature = plain(3, 2);
    assert_beat(signature, 1, 4);
    time_signature_destroy(signature);
    signature = plain(6, 3);
    assert_beat(signature, 3, 8); /* 6/8: a dotted quarter */
    time_signature_destroy(signature);
    signature = plain(12, 3);
    assert_beat(signature, 3, 8);
    time_signature_destroy(signature);
    signature = plain(6, 2);
    assert_beat(signature, 3, 4); /* 6/4: a dotted half */
    time_signature_destroy(signature);
    signature = plain(15, 3);
    assert_beat(signature, 3, 8);
    time_signature_destroy(signature);
    signature = plain(5, 3);
    assert_beat(signature, 1, 8);
    time_signature_destroy(signature);
    signature = plain(1, 2);
    assert_beat(signature, 1, 4);
    time_signature_destroy(signature);

    /* equal explicit groups are the beat, unequal ones keep the bottom unit */
    unsigned fours[] = {4, 4};
    signature = grouped(fours, 2, 4);
    assert_beat(signature, 1, 4);
    time_signature_destroy(signature);
    unsigned three_two[] = {3, 2};
    signature = grouped(three_two, 2, 3);
    assert_beat(signature, 1, 8);
    time_signature_destroy(signature);
    unsigned two_three_two[] = {2, 3, 2};
    signature = grouped(two_three_two, 3, 3);
    assert_beat(signature, 1, 8);
    time_signature_destroy(signature);
}

static void test_compound(void) {
    unsigned tops[] = {6, 9, 12, 15, 18, 24};
    for (int i = 0; i < 6; i++) {
        TimeSignature *signature = plain(tops[i], 3);
        assert(time_signature_is_compound(signature));
        time_signature_destroy(signature);
    }
    unsigned simple[] = {1, 2, 3, 4, 5, 7, 8, 10};
    for (int i = 0; i < 8; i++) {
        TimeSignature *signature = plain(simple[i], 3);
        assert(!time_signature_is_compound(signature));
        time_signature_destroy(signature);
    }
    /* a lone group of three is not compound; two or more groups all of three are; 3+2 is not */
    unsigned lone[] = {3};
    TimeSignature *signature = grouped(lone, 1, 3);
    assert(!time_signature_is_compound(signature));
    time_signature_destroy(signature);
    unsigned two_threes[] = {3, 3};
    signature = grouped(two_threes, 2, 3);
    assert(time_signature_is_compound(signature));
    time_signature_destroy(signature);
    unsigned three_two[] = {3, 2};
    signature = grouped(three_two, 2, 3);
    assert(!time_signature_is_compound(signature));
    time_signature_destroy(signature);
    unsigned twos[] = {2, 2, 2};
    signature = grouped(twos, 3, 3);
    assert(!time_signature_is_compound(signature));
    time_signature_destroy(signature);
}

static void test_equality(void) {
    TimeSignature *six_eight = plain(6, 3);
    unsigned threes[] = {3, 3};
    TimeSignature *explicit_six_eight = grouped(threes, 2, 3);
    assert(time_signature_equals(six_eight, explicit_six_eight)); /* the same groups over the same bottom */

    unsigned two_fours[] = {1, 1, 1, 1, 1, 1};
    TimeSignature *six_ones = grouped(two_fours, 6, 3);
    assert(!time_signature_equals(six_eight, six_ones)); /* the same top and bottom, other groups */
    unsigned twos[] = {2, 2, 2};
    TimeSignature *three_twos = grouped(twos, 3, 3);
    assert(!time_signature_equals(six_eight, three_twos));
    assert(!time_signature_equals(six_ones, three_twos));

    TimeSignature *six_four = plain(6, 2);
    assert(!time_signature_equals(six_eight, six_four)); /* the same groups over another bottom */

    unsigned three_two[] = {3, 2};
    unsigned two_three[] = {2, 3};
    TimeSignature *a = grouped(three_two, 2, 3);
    TimeSignature *b = grouped(two_three, 2, 3);
    TimeSignature *five_eight = plain(5, 3);
    assert(!time_signature_equals(a, b)); /* order matters */
    assert(!time_signature_equals(a, five_eight));
    assert(time_signature_equals(a, a));

    TimeSignature *two_four = plain(2, 2);
    TimeSignature *four_eight = plain(4, 3);
    assert(!time_signature_equals(two_four, four_eight)); /* 2/4 and 4/8 are different */

    assert(!time_signature_equals(NULL, six_eight) && !time_signature_equals(six_eight, NULL) &&
           !time_signature_equals(NULL, NULL));

    time_signature_destroy(six_eight);
    time_signature_destroy(explicit_six_eight);
    time_signature_destroy(six_ones);
    time_signature_destroy(three_twos);
    time_signature_destroy(six_four);
    time_signature_destroy(a);
    time_signature_destroy(b);
    time_signature_destroy(five_eight);
    time_signature_destroy(two_four);
    time_signature_destroy(four_eight);
}

static void test_copy(void) {
    unsigned three_two[] = {3, 2};
    TimeSignature *source = grouped(three_two, 2, 3);
    TimeSignature *copy = NULL;
    assert(time_signature_copy(source, &copy) == TIME_SIGNATURE_OK);
    assert(copy != NULL && copy != source && time_signature_equals(source, copy));
    assert_groups(copy, three_two, 2);
    time_signature_destroy(source); /* independent of the source */
    assert_groups(copy, three_two, 2);
    time_signature_destroy(copy);

    copy = (TimeSignature *)0x1;
    assert(time_signature_copy(NULL, &copy) == TIME_SIGNATURE_ERR_NULL_SOURCE);
    assert(copy == (TimeSignature *)0x1);

    TimeSignature *twelve = plain(12, 3);
    assert(time_signature_copy(twelve, &copy) == TIME_SIGNATURE_OK);
    assert(time_signature_equals(twelve, copy) && time_signature_is_compound(copy));
    time_signature_destroy(twelve);
    time_signature_destroy(copy);
}

/* The default groups of the largest top number (64, one unit each) fit the signature's internal room: 64 groups, though
   create_grouped takes 16 at most. */
static void test_capacity_and_limits(void) {
    TimeSignature *sixty_four = plain(TIME_SIGNATURE_MAX_TOP_NUMBER, 4);
    assert(TIME_SIGNATURE_MAX_TOP_NUMBER == 64);
    assert(time_signature_group_count(sixty_four) == 64);
    for (size_t i = 0; i < 64; i++) {
        assert(time_signature_group_at(sixty_four, i) == 1);
    }
    assert(time_signature_group_at(sixty_four, 64) == 0);
    assert(!time_signature_is_compound(sixty_four));
    assert_beat(sixty_four, 1, 16);
    TimeSignature *copy = NULL;
    assert(time_signature_copy(sixty_four, &copy) == TIME_SIGNATURE_OK);
    assert(time_signature_group_count(copy) == 64 && time_signature_equals(sixty_four, copy));
    time_signature_destroy(copy);
    time_signature_destroy(sixty_four);

    TimeSignature *sixty_three = plain(63, 4); /* 21 groups of three */
    assert(time_signature_group_count(sixty_three) == 21 && time_signature_is_compound(sixty_three));
    time_signature_destroy(sixty_three);

    /* create_grouped: 1 to 16 groups of 1 to 16 units, the sum within the top number range */
    NoteValue *eighth = unit(3);
    TimeSignature *signature = NULL;
    unsigned sixteen_ones[16];
    for (int i = 0; i < 16; i++) {
        sixteen_ones[i] = 1;
    }
    assert(time_signature_create_grouped(sixteen_ones, 16, eighth, &signature) == TIME_SIGNATURE_OK);
    assert(time_signature_group_count(signature) == 16 && time_signature_top_number(signature) == 16);
    time_signature_destroy(signature);

    unsigned seventeen_ones[17];
    for (int i = 0; i < 17; i++) {
        seventeen_ones[i] = 1;
    }
    signature = NULL;
    assert(time_signature_create_grouped(seventeen_ones, 17, eighth, &signature) == TIME_SIGNATURE_ERR_GROUPS_OUT_OF_RANGE);
    assert(time_signature_create_grouped(sixteen_ones, 0, eighth, &signature) == TIME_SIGNATURE_ERR_GROUPS_OUT_OF_RANGE);
    assert(time_signature_create_grouped(NULL, 2, eighth, &signature) == TIME_SIGNATURE_ERR_GROUPS_OUT_OF_RANGE);
    unsigned zero_group[] = {3, 0};
    unsigned wide_group[] = {17};
    assert(time_signature_create_grouped(zero_group, 2, eighth, &signature) == TIME_SIGNATURE_ERR_GROUPS_OUT_OF_RANGE);
    assert(time_signature_create_grouped(wide_group, 1, eighth, &signature) == TIME_SIGNATURE_ERR_GROUPS_OUT_OF_RANGE);
    assert(signature == NULL);

    /* the top number is the sum: 16 groups of 4 is 64 and fits, 16 groups of 5 is 80 and does not */
    unsigned fours[16];
    unsigned fives[16];
    for (int i = 0; i < 16; i++) {
        fours[i] = 4;
        fives[i] = 5;
    }
    assert(time_signature_create_grouped(fours, 16, eighth, &signature) == TIME_SIGNATURE_OK);
    assert(time_signature_top_number(signature) == 64 && time_signature_group_count(signature) == 16);
    time_signature_destroy(signature);
    signature = NULL;
    assert(time_signature_create_grouped(fives, 16, eighth, &signature) == TIME_SIGNATURE_ERR_TOP_NUMBER_OUT_OF_RANGE);
    assert(signature == NULL);

    unsigned three_two[] = {3, 2};
    assert(time_signature_create_grouped(three_two, 2, NULL, &signature) == TIME_SIGNATURE_ERR_NULL_BOTTOM_UNIT);
    NoteValue *dotted = NULL;
    assert(note_value_create_dotted(3, 1, &dotted) == NOTE_VALUE_OK);
    assert(time_signature_create_grouped(three_two, 2, dotted, &signature) == TIME_SIGNATURE_ERR_BOTTOM_UNIT_DOTTED);
    note_value_destroy(dotted);
    assert(signature == NULL);
    note_value_destroy(eighth);
}

int main(void) {
    test_default_groups();
    test_explicit_groups();
    test_beat_length();
    test_compound();
    test_equality();
    test_copy();
    test_capacity_and_limits();
    printf("test_meter_groups: ok\n");
    return 0;
}
