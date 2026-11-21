#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "theory/tonality/roman_numeral_rules.h"
#include "theory/tonality/seventh_in_key_rules.h"
#include "theory/tonality/triad_in_key_rules.h"

typedef StandardTonalityRomanNumeral Numeral;
typedef StandardTonalityTriadQuality TriadQuality;
typedef StandardTonalitySeventhQuality SeventhQuality;

#define MAJOR STANDARD_TONALITY_TRIAD_MAJOR
#define MINOR STANDARD_TONALITY_TRIAD_MINOR
#define DIMINISHED STANDARD_TONALITY_TRIAD_DIMINISHED
#define AUGMENTED STANDARD_TONALITY_TRIAD_AUGMENTED

#define MAMA STANDARD_TONALITY_SEVENTH_MAJOR_MAJOR
#define MAMI STANDARD_TONALITY_SEVENTH_MAJOR_MINOR
#define MIMI STANDARD_TONALITY_SEVENTH_MINOR_MINOR
#define HDIM STANDARD_TONALITY_SEVENTH_HALF_DIMINISHED
#define FDIM STANDARD_TONALITY_SEVENTH_FULLY_DIMINISHED

static void check_triad(int degree, TriadQuality quality, const char *expected) {
    Numeral numeral;
    char buffer[32];
    assert(standard_tonality_roman_numeral_triad(degree, quality, &numeral));
    assert(standard_tonality_roman_numeral_format(&numeral, buffer, sizeof(buffer)));
    assert(strcmp(buffer, expected) == 0);
}

static void check_seventh(int degree, SeventhQuality quality, const char *expected) {
    Numeral numeral;
    char buffer[32];
    assert(standard_tonality_roman_numeral_seventh_chord(degree, quality, &numeral));
    assert(standard_tonality_roman_numeral_format(&numeral, buffer, sizeof(buffer)));
    assert(strcmp(buffer, expected) == 0);
}

/* Example 1: the numerals for the degrees. */
static void test_degrees(void) {
    const char *upper[] = {"I", "II", "III", "IV", "V", "VI", "VII"};
    const char *lower[] = {"i", "ii", "iii", "iv", "v", "vi", "vii"};
    for (int degree = 1; degree <= 7; degree++) {
        assert(strcmp(standard_tonality_roman_numeral_degree(degree, true), upper[degree - 1]) == 0);
        assert(strcmp(standard_tonality_roman_numeral_degree(degree, false), lower[degree - 1]) == 0);
    }
    assert(standard_tonality_roman_numeral_degree(0, true) == NULL);
    assert(standard_tonality_roman_numeral_degree(8, false) == NULL);
}

static void test_symbols(void) {
    assert(strcmp(standard_tonality_roman_numeral_triad_symbol(MAJOR), "") == 0);
    assert(strcmp(standard_tonality_roman_numeral_triad_symbol(MINOR), "") == 0);
    assert(strcmp(standard_tonality_roman_numeral_triad_symbol(DIMINISHED), "\xC2\xB0") == 0);
    assert(strcmp(standard_tonality_roman_numeral_triad_symbol(AUGMENTED), "+") == 0);
    assert(standard_tonality_roman_numeral_triad_symbol((TriadQuality)99) == NULL);

    assert(strcmp(standard_tonality_roman_numeral_seventh_symbol(MAMA), "7") == 0);
    assert(strcmp(standard_tonality_roman_numeral_seventh_symbol(MAMI), "7") == 0);
    assert(strcmp(standard_tonality_roman_numeral_seventh_symbol(MIMI), "7") == 0);
    assert(strcmp(standard_tonality_roman_numeral_seventh_symbol(HDIM), "\xE2\x88\x85" "7") == 0);
    assert(strcmp(standard_tonality_roman_numeral_seventh_symbol(FDIM), "\xC2\xB0" "7") == 0);
    assert(standard_tonality_roman_numeral_seventh_symbol((SeventhQuality)99) == NULL);
}

/* Example 3: V, ii, V+, vii-circle. */
static void test_triad_examples(void) {
    check_triad(5, MAJOR, "V");
    check_triad(2, MINOR, "ii");
    check_triad(5, AUGMENTED, "V+");
    check_triad(7, DIMINISHED, "vii\xC2\xB0");
    check_triad(3, AUGMENTED, "III+");
    check_triad(1, MAJOR, "I");
    check_triad(4, MINOR, "iv");
}

/* Example 5: IV7, V7, ii7, ii-slashed-circle 7, vii-circle 7. */
static void test_seventh_examples(void) {
    check_seventh(4, MAMA, "IV7");
    check_seventh(5, MAMI, "V7");
    check_seventh(2, MIMI, "ii7");
    check_seventh(2, HDIM, "ii\xE2\x88\x85" "7");
    check_seventh(7, FDIM, "vii\xC2\xB0" "7");
}

/* Example 4: the diatonic triads of G major and G minor, by numeral. */
static void check_triad_key(StandardTonalityTriadKey key, const char *const expected[7]) {
    for (int degree = 1; degree <= 7; degree++) {
        TriadQuality quality;
        assert(standard_tonality_triad_quality_in_key(key, degree, &quality));
        check_triad(degree, quality, expected[degree - 1]);
    }
}

static void test_triads_in_keys(void) {
    const char *major[] = {"I", "ii", "iii", "IV", "V", "vi", "vii\xC2\xB0"};
    const char *natural[] = {"i", "ii\xC2\xB0", "III", "iv", "v", "VI", "VII"};
    const char *harmonic[] = {"i", "ii\xC2\xB0", "III+", "iv", "V", "VI", "vii\xC2\xB0"};
    check_triad_key(STANDARD_TONALITY_TRIAD_KEY_MAJOR, major);
    check_triad_key(STANDARD_TONALITY_TRIAD_KEY_MINOR_NATURAL, natural);
    check_triad_key(STANDARD_TONALITY_TRIAD_KEY_MINOR_HARMONIC, harmonic);
}

/* Example 6: the seventh chords of the major and minor scales, by numeral. */
static void check_seventh_key(StandardTonalitySeventhKey key, const char *const expected[7]) {
    for (int degree = 1; degree <= 7; degree++) {
        SeventhQuality quality;
        assert(standard_tonality_seventh_quality_in_key(key, degree, &quality));
        check_seventh(degree, quality, expected[degree - 1]);
    }
}

static void test_sevenths_in_keys(void) {
    const char *major[] = {"I7", "ii7", "iii7", "IV7", "V7", "vi7", "vii\xE2\x88\x85" "7"};
    const char *natural[] = {"i7", "ii\xE2\x88\x85" "7", "III7", "iv7", "v7", "VI7", "VII7"};
    const char *raised[] = {"i7", "ii\xE2\x88\x85" "7", "III7", "iv7", "V7", "VI7", "vii\xC2\xB0" "7"};
    check_seventh_key(STANDARD_TONALITY_SEVENTH_KEY_MAJOR, major);
    check_seventh_key(STANDARD_TONALITY_SEVENTH_KEY_MINOR_NATURAL, natural);
    check_seventh_key(STANDARD_TONALITY_SEVENTH_KEY_MINOR_RAISED_LEADING_TONE, raised);
}

/* SEVENTH_INFERENCE: dominant on 5 and 7, major elsewhere. */
static void test_inferred_seventh(void) {
    SeventhQuality quality;
    for (int degree = 1; degree <= 7; degree++) {
        assert(standard_tonality_roman_numeral_inferred_seventh(degree, &quality));
        assert(quality == ((degree == 5 || degree == 7) ? MAMI : MAMA));
    }
    quality = (SeventhQuality)42;
    assert(!standard_tonality_roman_numeral_inferred_seventh(0, &quality));
    assert(!standard_tonality_roman_numeral_inferred_seventh(8, &quality));
    assert(!standard_tonality_roman_numeral_inferred_seventh(5, NULL));
    assert(quality == (SeventhQuality)42);
}

static void test_unambiguous(void) {
    Numeral numeral;
    assert(standard_tonality_roman_numeral_seventh_chord(5, MAMI, &numeral));
    assert(standard_tonality_roman_numeral_is_unambiguous(&numeral));
    assert(standard_tonality_roman_numeral_seventh_chord(4, MAMA, &numeral));
    assert(standard_tonality_roman_numeral_is_unambiguous(&numeral));
    assert(standard_tonality_roman_numeral_seventh_chord(5, MAMA, &numeral));
    assert(!standard_tonality_roman_numeral_is_unambiguous(&numeral));
    assert(standard_tonality_roman_numeral_seventh_chord(2, MAMI, &numeral));
    assert(!standard_tonality_roman_numeral_is_unambiguous(&numeral));
    assert(standard_tonality_roman_numeral_seventh_chord(7, MAMA, &numeral));
    assert(!standard_tonality_roman_numeral_is_unambiguous(&numeral));
    assert(standard_tonality_roman_numeral_seventh_chord(2, HDIM, &numeral));
    assert(standard_tonality_roman_numeral_is_unambiguous(&numeral));
    assert(standard_tonality_roman_numeral_triad(5, MAJOR, &numeral));
    assert(standard_tonality_roman_numeral_is_unambiguous(&numeral));
    assert(!standard_tonality_roman_numeral_is_unambiguous(NULL));
}

static void test_constructors_reject_invalid_arguments(void) {
    Numeral numeral = {0, false, MAJOR, MAMA};
    assert(!standard_tonality_roman_numeral_triad(0, MAJOR, &numeral));
    assert(!standard_tonality_roman_numeral_triad(8, MAJOR, &numeral));
    assert(!standard_tonality_roman_numeral_triad(1, (TriadQuality)99, &numeral));
    assert(!standard_tonality_roman_numeral_triad(1, MAJOR, NULL));
    assert(!standard_tonality_roman_numeral_seventh_chord(0, MAMA, &numeral));
    assert(!standard_tonality_roman_numeral_seventh_chord(1, (SeventhQuality)99, &numeral));
    assert(!standard_tonality_roman_numeral_seventh_chord(1, MAMA, NULL));
    assert(numeral.degree == 0);
}

static void test_format_rejects_invalid_arguments(void) {
    Numeral numeral;
    char buffer[32];
    assert(standard_tonality_roman_numeral_triad(5, MAJOR, &numeral));
    assert(!standard_tonality_roman_numeral_format(NULL, buffer, sizeof(buffer)));
    assert(!standard_tonality_roman_numeral_format(&numeral, NULL, 32));
    assert(!standard_tonality_roman_numeral_format(&numeral, buffer, 0));
    assert(!standard_tonality_roman_numeral_format(&numeral, buffer, 1)); /* "V" needs two bytes */
    assert(standard_tonality_roman_numeral_format(&numeral, buffer, 2));
    assert(strcmp(buffer, "V") == 0);

    numeral.degree = 9;
    assert(!standard_tonality_roman_numeral_format(&numeral, buffer, sizeof(buffer)));
    numeral.degree = 5;
    numeral.triad = (TriadQuality)99;
    assert(!standard_tonality_roman_numeral_format(&numeral, buffer, sizeof(buffer)));
}

static void test_degree_of_letters(void) {
    static const char *const UPPER[7] = {"I", "II", "III", "IV", "V", "VI", "VII"};
    static const char *const LOWER[7] = {"i", "ii", "iii", "iv", "v", "vi", "vii"};
    for (int degree = 1; degree <= 7; degree++) {
        assert(standard_tonality_roman_numeral_degree_of(UPPER[degree - 1], strlen(UPPER[degree - 1])) == degree);
        assert(standard_tonality_roman_numeral_degree_of(LOWER[degree - 1], strlen(LOWER[degree - 1])) == degree);
    }
    /* mixed case reads as the numeral it spells, and only the first `length` characters count */
    assert(standard_tonality_roman_numeral_degree_of("Iv", 2) == 4);
    assert(standard_tonality_roman_numeral_degree_of("vII", 3) == 7);
    assert(standard_tonality_roman_numeral_degree_of("V7", 1) == 5);
    assert(standard_tonality_roman_numeral_degree_of("ii6", 2) == 2);
    assert(standard_tonality_roman_numeral_degree_of("V7", 2) == 0);
    assert(standard_tonality_roman_numeral_degree_of("viii", 4) == 0);
    assert(standard_tonality_roman_numeral_degree_of("iiii", 4) == 0);
    assert(standard_tonality_roman_numeral_degree_of("", 0) == 0);
    assert(standard_tonality_roman_numeral_degree_of("viiiiiii", 8) == 0);
    assert(standard_tonality_roman_numeral_degree_of(NULL, 1) == 0);
}

int main(void) {
    test_degrees();
    test_degree_of_letters();
    test_symbols();
    test_triad_examples();
    test_seventh_examples();
    test_triads_in_keys();
    test_sevenths_in_keys();
    test_inferred_seventh();
    test_unambiguous();
    test_constructors_reject_invalid_arguments();
    test_format_rejects_invalid_arguments();
    printf("test_roman_numeral_rules (standard-tonality): ok\n");
    return 0;
}
