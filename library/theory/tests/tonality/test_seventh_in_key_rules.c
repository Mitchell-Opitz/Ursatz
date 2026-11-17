#include <assert.h>
#include <stdio.h>

#include "theory/tonality/framework.h"
#include "theory/tonality/seventh_in_key_rules.h"
#include "theory/tests/tonality/spelling_test_util.h"

typedef StandardTonalitySeventhQuality SeventhQuality;

#define MAMA STANDARD_TONALITY_SEVENTH_MAJOR_MAJOR
#define MAMI STANDARD_TONALITY_SEVENTH_MAJOR_MINOR
#define MIMI STANDARD_TONALITY_SEVENTH_MINOR_MINOR
#define HDIM STANDARD_TONALITY_SEVENTH_HALF_DIMINISHED
#define FDIM STANDARD_TONALITY_SEVENTH_FULLY_DIMINISHED

#define MAJOR_KEY STANDARD_TONALITY_SEVENTH_KEY_MAJOR
#define MINOR_KEY STANDARD_TONALITY_SEVENTH_KEY_MINOR_NATURAL
#define RAISED STANDARD_TONALITY_SEVENTH_KEY_MINOR_RAISED_LEADING_TONE

static void check_table(StandardTonalitySeventhKey key, const SeventhQuality expected[7]) {
    for (int degree = 1; degree <= 7; degree++) {
        SeventhQuality quality = (SeventhQuality)42;
        assert(standard_tonality_seventh_quality_in_key(key, degree, &quality));
        assert(quality == expected[degree - 1]);
    }
}

/* Example 6: in a major key, major-major on do and fa, major-minor on sol,
   minor-minor on re, mi, and la, half-diminished on ti. */
static void test_major_key(void) {
    SeventhQuality expected[] = {MAMA, MIMI, MIMI, MAMA, MAMI, MIMI, HDIM};
    check_table(MAJOR_KEY, expected);
}

/* Natural minor: minor-minor on do, fa, sol; major-major on me and le;
   major-minor on te; half-diminished on re. */
static void test_minor_key(void) {
    SeventhQuality expected[] = {MIMI, HDIM, MAMA, MIMI, MIMI, MAMA, MAMI};
    check_table(MINOR_KEY, expected);
}

/* Example 7: with the raised leading tone, major-minor on sol and fully
   diminished on ti; do, re, me, fa, and le as in natural minor. */
static void test_raised_leading_tone(void) {
    SeventhQuality expected[] = {MIMI, HDIM, MAMA, MIMI, MAMI, MAMA, FDIM};
    check_table(RAISED, expected);
}

static void test_table_rejects_invalid_arguments(void) {
    SeventhQuality quality = (SeventhQuality)42;
    assert(!standard_tonality_seventh_quality_in_key((StandardTonalitySeventhKey)99, 1, &quality));
    assert(!standard_tonality_seventh_quality_in_key(MAJOR_KEY, 0, &quality));
    assert(!standard_tonality_seventh_quality_in_key(MAJOR_KEY, 8, &quality));
    assert(!standard_tonality_seventh_quality_in_key(MAJOR_KEY, 1, NULL));
    assert(quality == (SeventhQuality)42);
}

int main(void) {
    test_major_key();
    test_minor_key();
    test_raised_leading_tone();
    test_table_rejects_invalid_arguments();
    printf("test_seventh_in_key_rules (standard-tonality): ok\n");
    return 0;
}
