#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "framework.h"
#include "roman_numeral_analysis_rules.h"
#include "spelling_test_util.h"
#include "triad_in_key_rules.h"

typedef StandardTonalityRomanNumeral Numeral;
typedef StandardTonalityTriadQuality TriadQuality;
typedef StandardTonalitySeventhQuality SeventhQuality;

#define MAJOR_KEY STANDARD_TONALITY_ROMAN_KEY_MAJOR
#define MINOR_KEY STANDARD_TONALITY_ROMAN_KEY_MINOR

#define MAJOR STANDARD_TONALITY_TRIAD_MAJOR
#define MINOR STANDARD_TONALITY_TRIAD_MINOR
#define DIMINISHED STANDARD_TONALITY_TRIAD_DIMINISHED
#define AUGMENTED STANDARD_TONALITY_TRIAD_AUGMENTED

#define MAMA STANDARD_TONALITY_SEVENTH_MAJOR_MAJOR
#define MAMI STANDARD_TONALITY_SEVENTH_MAJOR_MINOR
#define MIMI STANDARD_TONALITY_SEVENTH_MINOR_MINOR
#define HDIM STANDARD_TONALITY_SEVENTH_HALF_DIMINISHED
#define FDIM STANDARD_TONALITY_SEVENTH_FULLY_DIMINISHED

typedef struct {
    PitchLetter letter;
    int accidental;
    int octave;
} Note;

static void test_rules_reference_framework(void) {
    for (int i = 0; i < STANDARD_TONALITY_ROMAN_NUMERAL_ANALYSIS_RULE_COUNT; i++) {
        const Rule *rule = standard_tonality_roman_numeral_analysis_rule((StandardTonalityRomanNumeralAnalysisRule)i);
        const Constraint *constraint =
            standard_tonality_roman_numeral_analysis_constraint((StandardTonalityRomanNumeralAnalysisRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), standard_tonality_framework()));
        assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    }
    assert(standard_tonality_roman_numeral_analysis_rule(STANDARD_TONALITY_ROMAN_NUMERAL_ANALYSIS_RULE_COUNT) ==
           NULL);
}

static void check_key_label(PitchLetter letter, int accidental, StandardTonalityRomanKeyMode mode, const char *expected) {
    char buffer[32];
    assert(standard_tonality_roman_key_label(letter, accidental, mode, buffer, sizeof(buffer)));
    assert(strcmp(buffer, expected) == 0);
}

/* KEY: a capital note name is major, "mi" after it minor. */
static void test_key_labels(void) {
    check_key_label(PITCH_LETTER_C, 0, MAJOR_KEY, "C");
    check_key_label(PITCH_LETTER_G, 0, MINOR_KEY, "Gmi");
    check_key_label(PITCH_LETTER_B, -1, MINOR_KEY, "B\xE2\x99\xAD" "mi");
    check_key_label(PITCH_LETTER_F, 1, MAJOR_KEY, "F\xE2\x99\xAF");

    char buffer[32];
    assert(!standard_tonality_roman_key_label(PITCH_LETTER_C, 0, (StandardTonalityRomanKeyMode)9, buffer,
                                              sizeof(buffer)));
    assert(!standard_tonality_roman_key_label(PITCH_LETTER_C, 3, MAJOR_KEY, buffer, sizeof(buffer)));
    assert(!standard_tonality_roman_key_label(PITCH_LETTER_C, 0, MAJOR_KEY, NULL, 32));
    assert(!standard_tonality_roman_key_label(PITCH_LETTER_G, 0, MINOR_KEY, buffer, 3)); /* "Gmi" needs 4 */
}

static void check_parsed_key(const char *label, PitchLetter letter, int accidental, StandardTonalityRomanKeyMode mode) {
    PitchLetter parsed_letter = PITCH_LETTER_A;
    int parsed_accidental = 42;
    StandardTonalityRomanKeyMode parsed_mode = (StandardTonalityRomanKeyMode)42;
    assert(standard_tonality_parse_roman_key_label(label, &parsed_letter, &parsed_accidental, &parsed_mode));
    assert(parsed_letter == letter && parsed_accidental == accidental && parsed_mode == mode);
}

/* KEY: "B-flat minor" is "B♭mi" or the lowercase "b♭". */
static void test_parse_key_labels(void) {
    check_parsed_key("C", PITCH_LETTER_C, 0, MAJOR_KEY);
    check_parsed_key("B\xE2\x99\xAD", PITCH_LETTER_B, -1, MAJOR_KEY);
    check_parsed_key("B\xE2\x99\xAD" "mi", PITCH_LETTER_B, -1, MINOR_KEY);
    check_parsed_key("b\xE2\x99\xAD", PITCH_LETTER_B, -1, MINOR_KEY);
    check_parsed_key("a", PITCH_LETTER_A, 0, MINOR_KEY);
    check_parsed_key("f\xE2\x99\xAF", PITCH_LETTER_F, 1, MINOR_KEY);
    /* Plain letters: b is the accidental after a letter, and B alone is B. */
    check_parsed_key("Bb", PITCH_LETTER_B, -1, MAJOR_KEY);
    check_parsed_key("bb", PITCH_LETTER_B, -1, MINOR_KEY);
    check_parsed_key("b", PITCH_LETTER_B, 0, MINOR_KEY);
    check_parsed_key("B", PITCH_LETTER_B, 0, MAJOR_KEY);
    check_parsed_key("F#mi", PITCH_LETTER_F, 1, MINOR_KEY);
}

/* Property: parsing a formatted key label gives the key back. */
static void test_key_label_round_trip(void) {
    PitchLetter letters[] = {PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F,
                             PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_B};
    int round_trips = 0;
    for (size_t l = 0; l < 7; l++) {
        for (int accidental = -2; accidental <= 2; accidental++) {
            for (int mode = 0; mode < 2; mode++) {
                char buffer[32];
                assert(standard_tonality_roman_key_label(letters[l], accidental, (StandardTonalityRomanKeyMode)mode,
                                                         buffer, sizeof(buffer)));
                PitchLetter letter;
                int parsed_accidental;
                StandardTonalityRomanKeyMode parsed_mode;
                assert(standard_tonality_parse_roman_key_label(buffer, &letter, &parsed_accidental, &parsed_mode));
                assert(letter == letters[l] && parsed_accidental == accidental &&
                       parsed_mode == (StandardTonalityRomanKeyMode)mode);
                round_trips++;
            }
        }
    }
    assert(round_trips == 7 * 5 * 2);
}

static void test_parse_key_rejects_malformed_labels(void) {
    PitchLetter letter = PITCH_LETTER_D;
    int accidental = 42;
    StandardTonalityRomanKeyMode mode = (StandardTonalityRomanKeyMode)42;
    /* A quality mark other than "mi", "mi" after a lowercase letter, a slash
       bass, an unknown letter, and trailing or leading text. */
    const char *bad[] = {"", "H", "h", "C+", "C\xC2\xB0", "Co", "bmi", "bbmi", "C/E", "Cmi/E", "Cx", "Cmimi", " C", "C ",
                         "Cma7", "C7"};
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        assert(!standard_tonality_parse_roman_key_label(bad[i], &letter, &accidental, &mode));
    }
    assert(!standard_tonality_parse_roman_key_label(NULL, &letter, &accidental, &mode));
    assert(!standard_tonality_parse_roman_key_label("C", NULL, &accidental, &mode));
    assert(letter == PITCH_LETTER_D && accidental == 42 && mode == (StandardTonalityRomanKeyMode)42);
}

/* DOMINANT_SEVENTH: V7 in major; V7 and VII7 in minor. */
static void test_dominant_seventh_degrees(void) {
    for (int degree = 1; degree <= 7; degree++) {
        assert(standard_tonality_roman_dominant_seventh_degree(MAJOR_KEY, degree) == (degree == 5));
        assert(standard_tonality_roman_dominant_seventh_degree(MINOR_KEY, degree) == (degree == 5 || degree == 7));
    }
    assert(!standard_tonality_roman_dominant_seventh_degree((StandardTonalityRomanKeyMode)9, 5));
}

static Numeral triad_numeral(int degree, TriadQuality quality) {
    Numeral numeral;
    assert(standard_tonality_roman_numeral_triad(degree, quality, &numeral));
    return numeral;
}

static Numeral seventh_numeral(int degree, SeventhQuality quality) {
    Numeral numeral;
    assert(standard_tonality_roman_numeral_seventh_chord(degree, quality, &numeral));
    return numeral;
}

/* RAISED_LEADING_TONE: V, V7, vii-circle, vii-circle 7, and III+ in minor
   imply it; the same numerals in major, and v and VII, do not. */
static void test_implies_raised_leading_tone(void) {
    assert(standard_tonality_roman_numeral_implies_raised_leading_tone(MINOR_KEY, &(Numeral){5, false, MAJOR, MAMA}));
    Numeral v7 = seventh_numeral(5, MAMI);
    Numeral viio = triad_numeral(7, DIMINISHED);
    Numeral viio7 = seventh_numeral(7, FDIM);
    Numeral iii_plus = triad_numeral(3, AUGMENTED);
    assert(standard_tonality_roman_numeral_implies_raised_leading_tone(MINOR_KEY, &v7));
    assert(standard_tonality_roman_numeral_implies_raised_leading_tone(MINOR_KEY, &viio));
    assert(standard_tonality_roman_numeral_implies_raised_leading_tone(MINOR_KEY, &viio7));
    assert(standard_tonality_roman_numeral_implies_raised_leading_tone(MINOR_KEY, &iii_plus));

    Numeral v_minor = triad_numeral(5, MINOR);
    Numeral vii = triad_numeral(7, MAJOR);
    Numeral vii7 = seventh_numeral(7, MAMI);
    Numeral i = triad_numeral(1, MINOR);
    assert(!standard_tonality_roman_numeral_implies_raised_leading_tone(MINOR_KEY, &v_minor));
    assert(!standard_tonality_roman_numeral_implies_raised_leading_tone(MINOR_KEY, &vii));
    assert(!standard_tonality_roman_numeral_implies_raised_leading_tone(MINOR_KEY, &vii7));
    assert(!standard_tonality_roman_numeral_implies_raised_leading_tone(MINOR_KEY, &i));

    assert(!standard_tonality_roman_numeral_implies_raised_leading_tone(MAJOR_KEY, &v7));
    assert(!standard_tonality_roman_numeral_implies_raised_leading_tone(MAJOR_KEY, &viio));
    assert(!standard_tonality_roman_numeral_implies_raised_leading_tone(MINOR_KEY, NULL));
}

/* ROOT: the degree of a note name in the key; the raised leading tone is
   degree 7 and says so. */
static void test_root_degree(void) {
    PitchSpelling *g = spell(PITCH_LETTER_G, 0, 4);
    int degree = 0;
    bool raised = true;

    /* G major: A is 2, F-sharp is 7. */
    assert(standard_tonality_roman_root_degree(MAJOR_KEY, g, PITCH_LETTER_A, 0, &degree, &raised));
    assert(degree == 2 && !raised);
    assert(standard_tonality_roman_root_degree(MAJOR_KEY, g, PITCH_LETTER_F, 1, &degree, &raised));
    assert(degree == 7 && !raised);
    assert(!standard_tonality_roman_root_degree(MAJOR_KEY, g, PITCH_LETTER_F, 0, &degree, &raised));

    /* G minor: B-flat is 3, F is the unraised 7, F-sharp the raised 7. */
    assert(standard_tonality_roman_root_degree(MINOR_KEY, g, PITCH_LETTER_B, -1, &degree, &raised));
    assert(degree == 3 && !raised);
    assert(standard_tonality_roman_root_degree(MINOR_KEY, g, PITCH_LETTER_F, 0, &degree, &raised));
    assert(degree == 7 && !raised);
    assert(standard_tonality_roman_root_degree(MINOR_KEY, g, PITCH_LETTER_F, 1, &degree, &raised));
    assert(degree == 7 && raised);
    assert(!standard_tonality_roman_root_degree(MINOR_KEY, g, PITCH_LETTER_B, 0, &degree, &raised));

    /* The flag is optional. */
    assert(standard_tonality_roman_root_degree(MINOR_KEY, g, PITCH_LETTER_D, 0, &degree, NULL));
    assert(degree == 5);

    assert(!standard_tonality_roman_root_degree(MAJOR_KEY, NULL, PITCH_LETTER_G, 0, &degree, &raised));
    assert(!standard_tonality_roman_root_degree(MAJOR_KEY, g, PITCH_LETTER_G, 0, NULL, &raised));
    assert(!standard_tonality_roman_root_degree((StandardTonalityRomanKeyMode)9, g, PITCH_LETTER_G, 0, &degree, &raised));
    pitch_spelling_destroy(g);

    /* A key with no scale (G double-sharp major needs a triple sharp). */
    PitchSpelling *imaginary = spell(PITCH_LETTER_G, 2, 4);
    assert(!standard_tonality_roman_root_degree(MAJOR_KEY, imaginary, PITCH_LETTER_G, 2, &degree, &raised));
    pitch_spelling_destroy(imaginary);
}

/* Identifies the notes in the key of `tonic`, destroying the spellings it makes. */
static bool identify(StandardTonalityRomanKeyMode mode, PitchLetter tonic_letter, int tonic_accidental,
                     const Note *notes, size_t count, Numeral *out) {
    PitchSpelling *tonic = spell(tonic_letter, tonic_accidental, 4);
    PitchSpelling *spellings[16];
    for (size_t i = 0; i < count; i++) {
        spellings[i] = spell(notes[i].letter, notes[i].accidental, notes[i].octave);
    }
    bool result = standard_tonality_identify_roman_numeral(mode, tonic, (const PitchSpelling *const *)spellings, count,
                                                           out);
    for (size_t i = 0; i < count; i++) {
        pitch_spelling_destroy(spellings[i]);
    }
    pitch_spelling_destroy(tonic);
    return result;
}

static void check_triad_numeral(StandardTonalityRomanKeyMode mode, PitchLetter tonic_letter, int tonic_accidental,
                                const Note *notes, size_t count, int degree, TriadQuality quality) {
    Numeral numeral = {42, true, (TriadQuality)42, (SeventhQuality)42};
    assert(identify(mode, tonic_letter, tonic_accidental, notes, count, &numeral));
    assert(numeral.degree == degree && !numeral.seventh && numeral.triad == quality);
}

static void check_seventh_numeral(StandardTonalityRomanKeyMode mode, PitchLetter tonic_letter, int tonic_accidental,
                                  const Note *notes, size_t count, int degree, SeventhQuality quality) {
    Numeral numeral = {42, false, (TriadQuality)42, (SeventhQuality)42};
    assert(identify(mode, tonic_letter, tonic_accidental, notes, count, &numeral));
    assert(numeral.degree == degree && numeral.seventh && numeral.seventh_quality == quality);
}

/* The diatonic triads of G major, by numeral: I ii iii IV V vi vii-circle. */
static void test_g_major_triads(void) {
    Note i[] = {{PITCH_LETTER_G, 0, 4}, {PITCH_LETTER_B, 0, 4}, {PITCH_LETTER_D, 0, 5}};
    Note ii[] = {{PITCH_LETTER_A, 0, 4}, {PITCH_LETTER_C, 0, 5}, {PITCH_LETTER_E, 0, 5}};
    Note iii[] = {{PITCH_LETTER_B, 0, 4}, {PITCH_LETTER_D, 0, 5}, {PITCH_LETTER_F, 1, 5}};
    Note iv[] = {{PITCH_LETTER_C, 0, 4}, {PITCH_LETTER_E, 0, 4}, {PITCH_LETTER_G, 0, 4}};
    Note v[] = {{PITCH_LETTER_D, 0, 4}, {PITCH_LETTER_F, 1, 4}, {PITCH_LETTER_A, 0, 4}};
    Note vi[] = {{PITCH_LETTER_E, 0, 4}, {PITCH_LETTER_G, 0, 4}, {PITCH_LETTER_B, 0, 4}};
    Note vii[] = {{PITCH_LETTER_F, 1, 4}, {PITCH_LETTER_A, 0, 4}, {PITCH_LETTER_C, 0, 5}};
    check_triad_numeral(MAJOR_KEY, PITCH_LETTER_G, 0, i, 3, 1, MAJOR);
    check_triad_numeral(MAJOR_KEY, PITCH_LETTER_G, 0, ii, 3, 2, MINOR);
    check_triad_numeral(MAJOR_KEY, PITCH_LETTER_G, 0, iii, 3, 3, MINOR);
    check_triad_numeral(MAJOR_KEY, PITCH_LETTER_G, 0, iv, 3, 4, MAJOR);
    check_triad_numeral(MAJOR_KEY, PITCH_LETTER_G, 0, v, 3, 5, MAJOR);
    check_triad_numeral(MAJOR_KEY, PITCH_LETTER_G, 0, vi, 3, 6, MINOR);
    check_triad_numeral(MAJOR_KEY, PITCH_LETTER_G, 0, vii, 3, 7, DIMINISHED);
}

/* G minor: i ii-circle III iv v VI VII, and with the raised leading tone
   V, vii-circle, and III+. */
static void test_g_minor_triads(void) {
    Note i[] = {{PITCH_LETTER_G, 0, 4}, {PITCH_LETTER_B, -1, 4}, {PITCH_LETTER_D, 0, 5}};
    Note ii[] = {{PITCH_LETTER_A, 0, 4}, {PITCH_LETTER_C, 0, 5}, {PITCH_LETTER_E, -1, 5}};
    Note iii[] = {{PITCH_LETTER_B, -1, 4}, {PITCH_LETTER_D, 0, 5}, {PITCH_LETTER_F, 0, 5}};
    Note iv[] = {{PITCH_LETTER_C, 0, 4}, {PITCH_LETTER_E, -1, 4}, {PITCH_LETTER_G, 0, 4}};
    Note v_natural[] = {{PITCH_LETTER_D, 0, 4}, {PITCH_LETTER_F, 0, 4}, {PITCH_LETTER_A, 0, 4}};
    Note v_raised[] = {{PITCH_LETTER_D, 0, 4}, {PITCH_LETTER_F, 1, 4}, {PITCH_LETTER_A, 0, 4}};
    Note vi[] = {{PITCH_LETTER_E, -1, 4}, {PITCH_LETTER_G, 0, 4}, {PITCH_LETTER_B, -1, 4}};
    Note vii_natural[] = {{PITCH_LETTER_F, 0, 4}, {PITCH_LETTER_A, 0, 4}, {PITCH_LETTER_C, 0, 5}};
    Note vii_raised[] = {{PITCH_LETTER_F, 1, 4}, {PITCH_LETTER_A, 0, 4}, {PITCH_LETTER_C, 0, 5}};
    Note iii_aug[] = {{PITCH_LETTER_B, -1, 4}, {PITCH_LETTER_D, 0, 5}, {PITCH_LETTER_F, 1, 5}};
    check_triad_numeral(MINOR_KEY, PITCH_LETTER_G, 0, i, 3, 1, MINOR);
    check_triad_numeral(MINOR_KEY, PITCH_LETTER_G, 0, ii, 3, 2, DIMINISHED);
    check_triad_numeral(MINOR_KEY, PITCH_LETTER_G, 0, iii, 3, 3, MAJOR);
    check_triad_numeral(MINOR_KEY, PITCH_LETTER_G, 0, iv, 3, 4, MINOR);
    check_triad_numeral(MINOR_KEY, PITCH_LETTER_G, 0, v_natural, 3, 5, MINOR);
    check_triad_numeral(MINOR_KEY, PITCH_LETTER_G, 0, v_raised, 3, 5, MAJOR);
    check_triad_numeral(MINOR_KEY, PITCH_LETTER_G, 0, vi, 3, 6, MAJOR);
    check_triad_numeral(MINOR_KEY, PITCH_LETTER_G, 0, vii_natural, 3, 7, MAJOR);
    check_triad_numeral(MINOR_KEY, PITCH_LETTER_G, 0, vii_raised, 3, 7, DIMINISHED);
    check_triad_numeral(MINOR_KEY, PITCH_LETTER_G, 0, iii_aug, 3, 3, AUGMENTED);
}

/* Example 6: the seventh chords of G major. */
static void test_g_major_sevenths(void) {
    Note i[] = {{PITCH_LETTER_G, 0, 4}, {PITCH_LETTER_B, 0, 4}, {PITCH_LETTER_D, 0, 5}, {PITCH_LETTER_F, 1, 5}};
    Note ii[] = {{PITCH_LETTER_A, 0, 4}, {PITCH_LETTER_C, 0, 5}, {PITCH_LETTER_E, 0, 5}, {PITCH_LETTER_G, 0, 5}};
    Note iv[] = {{PITCH_LETTER_C, 0, 4}, {PITCH_LETTER_E, 0, 4}, {PITCH_LETTER_G, 0, 4}, {PITCH_LETTER_B, 0, 4}};
    Note v[] = {{PITCH_LETTER_D, 0, 4}, {PITCH_LETTER_F, 1, 4}, {PITCH_LETTER_A, 0, 4}, {PITCH_LETTER_C, 0, 5}};
    Note vii[] = {{PITCH_LETTER_F, 1, 4}, {PITCH_LETTER_A, 0, 4}, {PITCH_LETTER_C, 0, 5}, {PITCH_LETTER_E, 0, 5}};
    check_seventh_numeral(MAJOR_KEY, PITCH_LETTER_G, 0, i, 4, 1, MAMA);
    check_seventh_numeral(MAJOR_KEY, PITCH_LETTER_G, 0, ii, 4, 2, MIMI);
    check_seventh_numeral(MAJOR_KEY, PITCH_LETTER_G, 0, iv, 4, 4, MAMA);
    check_seventh_numeral(MAJOR_KEY, PITCH_LETTER_G, 0, v, 4, 5, MAMI);
    check_seventh_numeral(MAJOR_KEY, PITCH_LETTER_G, 0, vii, 4, 7, HDIM);
}

/* Example 6: G minor's sevenths, including the dominant on the unraised
   subtonic (VII7, F A C E-flat) and the fully diminished vii-circle 7. */
static void test_g_minor_sevenths(void) {
    Note i[] = {{PITCH_LETTER_G, 0, 4}, {PITCH_LETTER_B, -1, 4}, {PITCH_LETTER_D, 0, 5}, {PITCH_LETTER_F, 0, 5}};
    Note ii[] = {{PITCH_LETTER_A, 0, 4}, {PITCH_LETTER_C, 0, 5}, {PITCH_LETTER_E, -1, 5}, {PITCH_LETTER_G, 0, 5}};
    Note vii_dominant[] = {{PITCH_LETTER_F, 0, 4}, {PITCH_LETTER_A, 0, 4}, {PITCH_LETTER_C, 0, 5}, {PITCH_LETTER_E, -1, 5}};
    Note v7[] = {{PITCH_LETTER_D, 0, 4}, {PITCH_LETTER_F, 1, 4}, {PITCH_LETTER_A, 0, 4}, {PITCH_LETTER_C, 0, 5}};
    Note viio7[] = {{PITCH_LETTER_F, 1, 4}, {PITCH_LETTER_A, 0, 4}, {PITCH_LETTER_C, 0, 5}, {PITCH_LETTER_E, -1, 5}};
    check_seventh_numeral(MINOR_KEY, PITCH_LETTER_G, 0, i, 4, 1, MIMI);
    check_seventh_numeral(MINOR_KEY, PITCH_LETTER_G, 0, ii, 4, 2, HDIM);
    check_seventh_numeral(MINOR_KEY, PITCH_LETTER_G, 0, vii_dominant, 4, 7, MAMI);
    check_seventh_numeral(MINOR_KEY, PITCH_LETTER_G, 0, v7, 4, 5, MAMI);
    check_seventh_numeral(MINOR_KEY, PITCH_LETTER_G, 0, viio7, 4, 7, FDIM);
}

/* The same numerals in every key: V in any major key is the major triad on
   the dominant, whatever the tonic. */
static void test_numerals_are_key_independent(void) {
    Note d_major[] = {{PITCH_LETTER_A, 0, 4}, {PITCH_LETTER_C, 1, 5}, {PITCH_LETTER_E, 0, 5}};
    check_triad_numeral(MAJOR_KEY, PITCH_LETTER_D, 0, d_major, 3, 5, MAJOR);
    Note b_flat_major[] = {{PITCH_LETTER_F, 0, 4}, {PITCH_LETTER_A, 0, 4}, {PITCH_LETTER_C, 0, 5}};
    check_triad_numeral(MAJOR_KEY, PITCH_LETTER_B, -1, b_flat_major, 3, 5, MAJOR);
    Note c_minor_v7[] = {{PITCH_LETTER_G, 0, 3}, {PITCH_LETTER_B, 0, 3}, {PITCH_LETTER_D, 0, 4}, {PITCH_LETTER_F, 0, 4}};
    check_seventh_numeral(MINOR_KEY, PITCH_LETTER_C, 0, c_minor_v7, 4, 5, MAMI);
}

/* INVARIANCE: doubling, spacing, order, and the bass note do not change the
   numeral. */
static void test_invariance(void) {
    /* V in G major (D F-sharp A): doubled, spread, inverted, any order. */
    Note root_position[] = {{PITCH_LETTER_D, 0, 4}, {PITCH_LETTER_F, 1, 4}, {PITCH_LETTER_A, 0, 4}};
    Note first_inversion[] = {{PITCH_LETTER_F, 1, 3}, {PITCH_LETTER_A, 0, 3}, {PITCH_LETTER_D, 0, 4}};
    Note second_inversion[] = {{PITCH_LETTER_A, 0, 2}, {PITCH_LETTER_D, 0, 4}, {PITCH_LETTER_F, 1, 4}};
    Note doubled_open[] = {{PITCH_LETTER_D, 0, 2}, {PITCH_LETTER_A, 0, 3}, {PITCH_LETTER_D, 0, 4}, {PITCH_LETTER_F, 1, 5},
                           {PITCH_LETTER_D, 0, 5}};
    Note shuffled[] = {{PITCH_LETTER_F, 1, 5}, {PITCH_LETTER_D, 0, 3}, {PITCH_LETTER_A, 0, 4}};
    check_triad_numeral(MAJOR_KEY, PITCH_LETTER_G, 0, root_position, 3, 5, MAJOR);
    check_triad_numeral(MAJOR_KEY, PITCH_LETTER_G, 0, first_inversion, 3, 5, MAJOR);
    check_triad_numeral(MAJOR_KEY, PITCH_LETTER_G, 0, second_inversion, 3, 5, MAJOR);
    check_triad_numeral(MAJOR_KEY, PITCH_LETTER_G, 0, doubled_open, 5, 5, MAJOR);
    check_triad_numeral(MAJOR_KEY, PITCH_LETTER_G, 0, shuffled, 3, 5, MAJOR);

    /* V7 in G major with the seventh in the bass. */
    Note v7_third_inversion[] = {{PITCH_LETTER_C, 0, 3}, {PITCH_LETTER_D, 0, 4}, {PITCH_LETTER_F, 1, 4}, {PITCH_LETTER_A, 0, 4}};
    check_seventh_numeral(MAJOR_KEY, PITCH_LETTER_G, 0, v7_third_inversion, 4, 5, MAMI);
}

/* Example 7: a V7 with the fifth missing is still V7. The unsaid fifth of a
   minor-third, minor-seventh chord is the key's to settle. */
static void test_omitted_fifth(void) {
    /* D F-sharp C, and with the root doubled, in G major and G minor. */
    Note no_fifth[] = {{PITCH_LETTER_D, 0, 4}, {PITCH_LETTER_F, 1, 4}, {PITCH_LETTER_C, 0, 5}};
    Note doubled_root[] = {{PITCH_LETTER_D, 0, 3}, {PITCH_LETTER_F, 1, 4}, {PITCH_LETTER_C, 0, 5}, {PITCH_LETTER_D, 0, 5}};
    check_seventh_numeral(MAJOR_KEY, PITCH_LETTER_G, 0, no_fifth, 3, 5, MAMI);
    check_seventh_numeral(MAJOR_KEY, PITCH_LETTER_G, 0, doubled_root, 4, 5, MAMI);
    check_seventh_numeral(MINOR_KEY, PITCH_LETTER_G, 0, no_fifth, 3, 5, MAMI);

    /* IV7 without the fifth: C E B. */
    Note iv7[] = {{PITCH_LETTER_C, 0, 4}, {PITCH_LETTER_E, 0, 4}, {PITCH_LETTER_B, 0, 4}};
    check_seventh_numeral(MAJOR_KEY, PITCH_LETTER_G, 0, iv7, 3, 4, MAMA);

    /* A minor third and minor seventh, A C G: ii7 in G major, ii-slashed-
       circle 7 in G minor. */
    Note ii7[] = {{PITCH_LETTER_A, 0, 4}, {PITCH_LETTER_C, 0, 5}, {PITCH_LETTER_G, 0, 5}};
    check_seventh_numeral(MAJOR_KEY, PITCH_LETTER_G, 0, ii7, 3, 2, MIMI);
    check_seventh_numeral(MINOR_KEY, PITCH_LETTER_G, 0, ii7, 3, 2, HDIM);

    /* Minor third with a diminished seventh: F-sharp A E-flat, vii-circle 7. */
    Note viio7[] = {{PITCH_LETTER_F, 1, 4}, {PITCH_LETTER_A, 0, 4}, {PITCH_LETTER_E, -1, 5}};
    check_seventh_numeral(MINOR_KEY, PITCH_LETTER_G, 0, viio7, 3, 7, FDIM);

    /* A root, third, and seventh that are not a seventh chord's: no numeral. */
    Numeral numeral;
    Note minor_major[] = {{PITCH_LETTER_G, 0, 4}, {PITCH_LETTER_B, -1, 4}, {PITCH_LETTER_F, 1, 5}};
    assert(!identify(MINOR_KEY, PITCH_LETTER_G, 0, minor_major, 3, &numeral));
}

/* Chords from outside the key and chords outside the five qualities get no
   numeral. */
static void test_no_numeral(void) {
    Numeral numeral = {42, true, (TriadQuality)42, (SeventhQuality)42};
    /* A chromatic root: B-flat major in G major, D-flat major in G minor. */
    Note b_flat[] = {{PITCH_LETTER_B, -1, 4}, {PITCH_LETTER_D, 0, 5}, {PITCH_LETTER_F, 0, 5}};
    Note d_flat[] = {{PITCH_LETTER_D, -1, 4}, {PITCH_LETTER_F, 0, 4}, {PITCH_LETTER_A, -1, 4}};
    assert(!identify(MAJOR_KEY, PITCH_LETTER_G, 0, b_flat, 3, &numeral));
    assert(!identify(MINOR_KEY, PITCH_LETTER_G, 0, d_flat, 3, &numeral));

    /* A diatonic root with a quality the key does not give it: a major
       triad on ii (A C-sharp E), a secondary dominant of V. */
    Note ii_major[] = {{PITCH_LETTER_A, 0, 4}, {PITCH_LETTER_C, 1, 5}, {PITCH_LETTER_E, 0, 5}};
    assert(!identify(MAJOR_KEY, PITCH_LETTER_G, 0, ii_major, 3, &numeral));

    /* A dominant seventh on the second degree (A C-sharp E G): V7/V. */
    Note v7_of_v[] = {{PITCH_LETTER_A, 0, 4}, {PITCH_LETTER_C, 1, 5}, {PITCH_LETTER_E, 0, 5}, {PITCH_LETTER_G, 0, 5}};
    assert(!identify(MAJOR_KEY, PITCH_LETTER_G, 0, v7_of_v, 4, &numeral));

    /* Raised leading tone root outside minor: F-sharp diminished in G major
       is vii-circle (diatonic); in G minor the unraised F is VII, but F-sharp
       major is no chord of either. */
    Note f_sharp_major[] = {{PITCH_LETTER_F, 1, 4}, {PITCH_LETTER_A, 1, 4}, {PITCH_LETTER_C, 1, 5}};
    assert(!identify(MINOR_KEY, PITCH_LETTER_G, 0, f_sharp_major, 3, &numeral));

    /* Outside the five seventh qualities: a minor triad with a major
       seventh (G B-flat D F-sharp, the tonic with a raised leading tone). */
    Note minor_major[] = {{PITCH_LETTER_G, 0, 4}, {PITCH_LETTER_B, -1, 4}, {PITCH_LETTER_D, 0, 5}, {PITCH_LETTER_F, 1, 5}};
    assert(!identify(MINOR_KEY, PITCH_LETTER_G, 0, minor_major, 4, &numeral));

    /* Not a chord at all, and too few notes. */
    Note not_thirds[] = {{PITCH_LETTER_G, 0, 4}, {PITCH_LETTER_A, 0, 4}, {PITCH_LETTER_B, 0, 4}};
    assert(!identify(MAJOR_KEY, PITCH_LETTER_G, 0, not_thirds, 3, &numeral));
    Note two[] = {{PITCH_LETTER_G, 0, 4}, {PITCH_LETTER_B, 0, 4}};
    assert(!identify(MAJOR_KEY, PITCH_LETTER_G, 0, two, 2, &numeral));

    assert(numeral.degree == 42 && numeral.seventh && numeral.triad == (TriadQuality)42);
}

static void test_identify_rejects_invalid_arguments(void) {
    PitchSpelling *g = spell(PITCH_LETTER_G, 0, 4);
    PitchSpelling *b = spell(PITCH_LETTER_B, 0, 4);
    PitchSpelling *d = spell(PITCH_LETTER_D, 0, 5);
    const PitchSpelling *notes[] = {g, b, d};
    Numeral numeral;
    assert(standard_tonality_identify_roman_numeral(MAJOR_KEY, g, notes, 3, &numeral));
    assert(!standard_tonality_identify_roman_numeral(MAJOR_KEY, NULL, notes, 3, &numeral));
    assert(!standard_tonality_identify_roman_numeral(MAJOR_KEY, g, NULL, 3, &numeral));
    assert(!standard_tonality_identify_roman_numeral(MAJOR_KEY, g, notes, 3, NULL));
    assert(!standard_tonality_identify_roman_numeral((StandardTonalityRomanKeyMode)9, g, notes, 3, &numeral));
    const PitchSpelling *with_null[] = {g, NULL, d};
    assert(!standard_tonality_identify_roman_numeral(MAJOR_KEY, g, with_null, 3, &numeral));
    pitch_spelling_destroy(g);
    pitch_spelling_destroy(b);
    pitch_spelling_destroy(d);
}

/* Property: the numeral identified from a spelled diatonic chord formats
   and parses back to itself, in every major and minor key. */
static void test_identified_numerals_round_trip(void) {
    PitchLetter letters[] = {PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F,
                             PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_B};
    int checked = 0;
    for (size_t l = 0; l < 7; l++) {
        for (int accidental = -1; accidental <= 1; accidental++) {
            for (int mode = 0; mode < 2; mode++) {
                PitchSpelling *tonic = spell(letters[l], accidental, 4);
                for (int degree = 1; degree <= 7; degree++) {
                    PitchSpelling *notes[3];
                    bool spelled;
                    if (mode == MAJOR_KEY) {
                        spelled = standard_tonality_spell_diatonic_triad(STANDARD_TONALITY_TRIAD_KEY_MAJOR, tonic,
                                                                         degree, notes);
                    } else {
                        spelled = standard_tonality_spell_diatonic_triad(STANDARD_TONALITY_TRIAD_KEY_MINOR_HARMONIC,
                                                                         tonic, degree, notes);
                    }
                    if (!spelled) {
                        continue;
                    }
                    Numeral numeral;
                    assert(standard_tonality_identify_roman_numeral((StandardTonalityRomanKeyMode)mode, tonic,
                                                                    (const PitchSpelling *const *)notes, 3, &numeral));
                    assert(numeral.degree == degree && !numeral.seventh);

                    char buffer[32];
                    Numeral parsed;
                    assert(standard_tonality_roman_numeral_format(&numeral, buffer, sizeof(buffer)));
                    assert(standard_tonality_roman_numeral_parse(buffer, &parsed));
                    assert(parsed.degree == numeral.degree && parsed.triad == numeral.triad);
                    for (int i = 0; i < 3; i++) {
                        pitch_spelling_destroy(notes[i]);
                    }
                    checked++;
                }
                pitch_spelling_destroy(tonic);
            }
        }
    }
    assert(checked > 0);
}

int main(void) {
    test_rules_reference_framework();
    test_key_labels();
    test_parse_key_labels();
    test_key_label_round_trip();
    test_parse_key_rejects_malformed_labels();
    test_dominant_seventh_degrees();
    test_implies_raised_leading_tone();
    test_root_degree();
    test_g_major_triads();
    test_g_minor_triads();
    test_g_major_sevenths();
    test_g_minor_sevenths();
    test_numerals_are_key_independent();
    test_invariance();
    test_omitted_fifth();
    test_no_numeral();
    test_identify_rejects_invalid_arguments();
    test_identified_numerals_round_trip();
    printf("test_roman_numeral_analysis_rules (standard-tonality): ok\n");
    return 0;
}
