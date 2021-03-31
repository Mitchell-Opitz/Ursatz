#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "framework.h"
#include "roman_numeral_rules.h"
#include "seventh_in_key_rules.h"
#include "triad_in_key_rules.h"

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

static void test_rules_reference_framework(void) {
    for (int i = 0; i < STANDARD_TONALITY_ROMAN_NUMERAL_RULE_COUNT; i++) {
        const Rule *rule = standard_tonality_roman_numeral_rule((StandardTonalityRomanNumeralRule)i);
        const Constraint *constraint = standard_tonality_roman_numeral_constraint((StandardTonalityRomanNumeralRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), standard_tonality_framework()));
        assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    }
    assert(standard_tonality_roman_numeral_rule(STANDARD_TONALITY_ROMAN_NUMERAL_RULE_COUNT) == NULL);
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

/* CASE: a major third above the root is uppercase, a minor third lowercase. */
static void test_case(void) {
    assert(standard_tonality_roman_numeral_triad_uppercase(MAJOR));
    assert(standard_tonality_roman_numeral_triad_uppercase(AUGMENTED));
    assert(!standard_tonality_roman_numeral_triad_uppercase(MINOR));
    assert(!standard_tonality_roman_numeral_triad_uppercase(DIMINISHED));
    assert(!standard_tonality_roman_numeral_triad_uppercase((TriadQuality)99));

    assert(standard_tonality_roman_numeral_seventh_uppercase(MAMA));
    assert(standard_tonality_roman_numeral_seventh_uppercase(MAMI));
    assert(!standard_tonality_roman_numeral_seventh_uppercase(MIMI));
    assert(!standard_tonality_roman_numeral_seventh_uppercase(HDIM));
    assert(!standard_tonality_roman_numeral_seventh_uppercase(FDIM));
    assert(!standard_tonality_roman_numeral_seventh_uppercase((SeventhQuality)99));
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

static void check_parsed_triad(const char *text, int degree, TriadQuality quality) {
    Numeral numeral;
    assert(standard_tonality_roman_numeral_parse(text, &numeral));
    assert(numeral.degree == degree && !numeral.seventh && numeral.triad == quality);
}

static void check_parsed_seventh(const char *text, int degree, SeventhQuality quality) {
    Numeral numeral;
    assert(standard_tonality_roman_numeral_parse(text, &numeral));
    assert(numeral.degree == degree && numeral.seventh && numeral.seventh_quality == quality);
}

static void test_parse_examples(void) {
    check_parsed_triad("V", 5, MAJOR);
    check_parsed_triad("ii", 2, MINOR);
    check_parsed_triad("V+", 5, AUGMENTED);
    check_parsed_triad("vii\xC2\xB0", 7, DIMINISHED);
    check_parsed_triad("VII", 7, MAJOR);
    check_parsed_triad("vi", 6, MINOR);
    check_parsed_triad("iv", 4, MINOR);
    check_parsed_triad("III+", 3, AUGMENTED);

    check_parsed_seventh("V7", 5, MAMI);
    check_parsed_seventh("IV7", 4, MAMA);
    check_parsed_seventh("VII7", 7, MAMI);
    check_parsed_seventh("ii7", 2, MIMI);
    check_parsed_seventh("ii\xE2\x88\x85" "7", 2, HDIM);
    check_parsed_seventh("vii\xC2\xB0" "7", 7, FDIM);
}

/* Plain-letter forms: o and the slashed letter. */
static void test_parse_plain_letters(void) {
    check_parsed_triad("viio", 7, DIMINISHED);
    check_parsed_seventh("viio7", 7, FDIM);
    check_parsed_seventh("ii\xC3\xB8" "7", 2, HDIM);
}

/* Property: every unambiguous numeral parses back to what was formatted. */
static void test_round_trip(void) {
    int round_trips = 0;
    for (int degree = 1; degree <= 7; degree++) {
        for (int q = 0; q < STANDARD_TONALITY_TRIAD_QUALITY_COUNT; q++) {
            Numeral numeral;
            char buffer[32];
            assert(standard_tonality_roman_numeral_triad(degree, (TriadQuality)q, &numeral));
            assert(standard_tonality_roman_numeral_format(&numeral, buffer, sizeof(buffer)));
            Numeral parsed;
            assert(standard_tonality_roman_numeral_parse(buffer, &parsed));
            assert(parsed.degree == degree && !parsed.seventh && parsed.triad == (TriadQuality)q);
            round_trips++;
        }
        for (int q = 0; q < STANDARD_TONALITY_SEVENTH_QUALITY_COUNT; q++) {
            Numeral numeral;
            char buffer[32];
            assert(standard_tonality_roman_numeral_seventh_chord(degree, (SeventhQuality)q, &numeral));
            if (!standard_tonality_roman_numeral_is_unambiguous(&numeral)) {
                continue;
            }
            assert(standard_tonality_roman_numeral_format(&numeral, buffer, sizeof(buffer)));
            Numeral parsed;
            assert(standard_tonality_roman_numeral_parse(buffer, &parsed));
            assert(parsed.degree == degree && parsed.seventh && parsed.seventh_quality == (SeventhQuality)q);
            round_trips++;
        }
    }
    /* 7 * 4 triads and 7 * 5 sevenths, less the major-major sevenths on 5 and
       7 and the major-minor sevenths on the other five degrees. */
    assert(round_trips == 7 * 4 + 7 * 5 - 7);
}

static void test_parse_rejects_malformed_numerals(void) {
    Numeral numeral = {42, true, (TriadQuality)42, (SeventhQuality)42};
    const char *bad[] = {"",    "VIII", "Vi",   "iV",   "Vo",   "V\xC2\xB0", "v+",  "iv+",  "V7x", "V77",
                         "vii\xE2\x88\x85", "VII\xE2\x88\x85" "7", "V\xC2\xB0" "7", "ii+7", "x",  " V", "V ", "i i", "IIII"};
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        assert(!standard_tonality_roman_numeral_parse(bad[i], &numeral));
    }
    assert(!standard_tonality_roman_numeral_parse(NULL, &numeral));
    assert(!standard_tonality_roman_numeral_parse("V", NULL));
    assert(numeral.degree == 42 && numeral.seventh && numeral.triad == (TriadQuality)42);
}

static void test_build_chord(void) {
    EntityID *target;
    assert(entity_id_create("sonority-test-roman", &target) == ENTITY_ID_OK);

    Numeral numeral;
    assert(standard_tonality_roman_numeral_seventh_chord(2, HDIM, &numeral));
    Chord *chord = NULL;
    assert(standard_tonality_build_roman_numeral_chord(&numeral, target, &chord) == CHORD_OK);
    assert(strcmp(chord_label(chord), "ii\xE2\x88\x85" "7") == 0);

    const Interpretation *interpretation = chord_interpretation(chord);
    assert(interpretation_framework_reference(interpretation) != NULL);
    assert(entity_id_equals(interpretation_target_id(interpretation), target));
    assert(strcmp(interpretation_author(interpretation), "standard-tonality-v1") == 0);
    chord_destroy(chord);
}

static void test_build_chord_rejects_invalid_numerals(void) {
    EntityID *target;
    assert(entity_id_create("sonority-test-roman-reject", &target) == ENTITY_ID_OK);
    Numeral numeral = {9, false, MAJOR, MAMA};
    Chord *chord = NULL;
    assert(standard_tonality_build_roman_numeral_chord(&numeral, target, &chord) == CHORD_ERR_NULL_LABEL);
    assert(standard_tonality_build_roman_numeral_chord(NULL, target, &chord) == CHORD_ERR_NULL_LABEL);
    assert(chord == NULL);
    /* The caller still owns the target on failure. */
    entity_id_destroy(target);
}

int main(void) {
    test_rules_reference_framework();
    test_degrees();
    test_case();
    test_symbols();
    test_triad_examples();
    test_seventh_examples();
    test_triads_in_keys();
    test_sevenths_in_keys();
    test_inferred_seventh();
    test_unambiguous();
    test_constructors_reject_invalid_arguments();
    test_format_rejects_invalid_arguments();
    test_parse_examples();
    test_parse_plain_letters();
    test_round_trip();
    test_parse_rejects_malformed_numerals();
    test_build_chord();
    test_build_chord_rejects_invalid_numerals();
    printf("test_roman_numeral_rules (standard-tonality): ok\n");
    return 0;
}
