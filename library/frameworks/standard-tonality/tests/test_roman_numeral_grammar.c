#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "roman_numeral_grammar.h"

typedef StandardTonalityNumeralText Text;
#define Q(name) STANDARD_TONALITY_NUMERAL_##name

static Text plain(int degree, StandardTonalityNumeralQuality quality, int inversion) {
    Text text;
    memset(&text, 0, sizeof(text));
    text.degree = degree;
    text.quality = quality;
    text.inversion = inversion;
    return text;
}

static Text parsed(const char *label) {
    Text text;
    if (!standard_tonality_numeral_parse(label, &text)) {
        fprintf(stderr, "expected %s to parse\n", label);
        assert(0);
    }
    return text;
}

static void expect_fields(const char *label, int alteration, int degree, StandardTonalityNumeralQuality quality,
                          int inversion) {
    Text text = parsed(label);
    assert(text.alteration == alteration && text.degree == degree && text.quality == quality &&
           text.inversion == inversion && !text.has_target);
}

static void expect_canonical(const char *label, const char *canonical) {
    char buffer[64];
    Text text = parsed(label);
    assert(standard_tonality_numeral_format(&text, buffer, sizeof(buffer)));
    if (strcmp(buffer, canonical) != 0) {
        fprintf(stderr, "%s formatted as %s, expected %s\n", label, buffer, canonical);
        assert(0);
    }
    /* parse then format is stable */
    Text again = parsed(buffer);
    char second[64];
    assert(standard_tonality_numeral_format(&again, second, sizeof(second)) && strcmp(second, canonical) == 0);
    assert(memcmp(&again, &text, sizeof(Text)) == 0);
}

static void expect_rejected(const char *label) {
    Text text = plain(3, Q(MINOR), 1);
    text.alteration = 1;
    Text before = text;
    if (standard_tonality_numeral_parse(label, &text)) {
        fprintf(stderr, "expected %s to be rejected\n", label);
        assert(0);
    }
    assert(memcmp(&text, &before, sizeof(Text)) == 0); /* output untouched */
}

static void test_every_numeral_and_quality(void) {
    for (int degree = 1; degree <= 7; degree++) {
        for (int quality = 0; quality < Q(QUALITY_COUNT); quality++) {
            int top = standard_tonality_numeral_quality_is_seventh((StandardTonalityNumeralQuality)quality) ? 3 : 2;
            for (int inversion = 0; inversion <= top; inversion++) {
                for (int alteration = -1; alteration <= 1; alteration++) {
                    Text text = plain(degree, (StandardTonalityNumeralQuality)quality, inversion);
                    text.alteration = alteration;
                    char buffer[64];
                    assert(standard_tonality_numeral_format(&text, buffer, sizeof(buffer)));
                    Text back = parsed(buffer);
                    if (memcmp(&back, &text, sizeof(Text)) != 0) {
                        fprintf(stderr, "%s did not round trip\n", buffer);
                        assert(0);
                    }
                }
            }
        }
    }
}

static void test_case_and_marks(void) {
    expect_fields("I", 0, 1, Q(MAJOR), 0);
    expect_fields("ii", 0, 2, Q(MINOR), 0);
    expect_fields("III+", 0, 3, Q(AUGMENTED), 0);
    expect_fields("viio", 0, 7, Q(DIMINISHED), 0);
    expect_fields("vii\xC2\xB0", 0, 7, Q(DIMINISHED), 0);
    expect_fields("V7", 0, 5, Q(DOMINANT_SEVENTH), 0);
    expect_fields("I7", 0, 1, Q(DOMINANT_SEVENTH), 0);
    expect_fields("IM7", 0, 1, Q(MAJOR_SEVENTH), 0);
    expect_fields("IVmaj7", 0, 4, Q(MAJOR_SEVENTH), 0);
    expect_fields("ii7", 0, 2, Q(MINOR_SEVENTH), 0);
    expect_fields("viio7", 0, 7, Q(FULLY_DIMINISHED_SEVENTH), 0);
    expect_fields("vii\xC2\xB0" "7", 0, 7, Q(FULLY_DIMINISHED_SEVENTH), 0);
    expect_fields("ii\xC3\xB8" "7", 0, 2, Q(HALF_DIMINISHED_SEVENTH), 0);
    expect_fields("ii\xE2\x88\x85" "7", 0, 2, Q(HALF_DIMINISHED_SEVENTH), 0);
    expect_fields("ii/o7", 0, 2, Q(HALF_DIMINISHED_SEVENTH), 0);
    expect_fields("vii%7", 0, 7, Q(HALF_DIMINISHED_SEVENTH), 0);
    for (int degree = 1; degree <= 7; degree++) {
        static const char *const UPPER[7] = {"I", "II", "III", "IV", "V", "VI", "VII"};
        static const char *const LOWER[7] = {"i", "ii", "iii", "iv", "v", "vi", "vii"};
        expect_fields(UPPER[degree - 1], 0, degree, Q(MAJOR), 0);
        expect_fields(LOWER[degree - 1], 0, degree, Q(MINOR), 0);
    }
}

static void test_alterations(void) {
    expect_fields("bVI", -1, 6, Q(MAJOR), 0);
    expect_fields("#iv6", 1, 4, Q(MINOR), 1);
    expect_fields("\xE2\x99\xAD" "VII", -1, 7, Q(MAJOR), 0);
    expect_fields("\xE2\x99\xAF" "ivo", 1, 4, Q(DIMINISHED), 0);
    expect_canonical("\xE2\x99\xAD" "VII", "bVII");
    expect_canonical("\xE2\x99\xAF" "ivo", "#ivo");
}

static void test_figures_and_slash_spellings(void) {
    static const struct {
        const char *label;
        StandardTonalityNumeralQuality quality;
        int inversion;
    } FIGURES[] = {
        {"V", Q(MAJOR), 0},           {"V6", Q(MAJOR), 1},          {"V64", Q(MAJOR), 2},
        {"V6/4", Q(MAJOR), 2},        {"V7", Q(DOMINANT_SEVENTH), 0}, {"V65", Q(DOMINANT_SEVENTH), 1},
        {"V6/5", Q(DOMINANT_SEVENTH), 1}, {"V43", Q(DOMINANT_SEVENTH), 2}, {"V4/3", Q(DOMINANT_SEVENTH), 2},
        {"V42", Q(DOMINANT_SEVENTH), 3}, {"V4/2", Q(DOMINANT_SEVENTH), 3}, {"V2", Q(DOMINANT_SEVENTH), 3},
        {"ii6", Q(MINOR), 1},         {"ii64", Q(MINOR), 2},        {"ii65", Q(MINOR_SEVENTH), 1},
        {"ii/o2", Q(HALF_DIMINISHED_SEVENTH), 3}, {"ii/o4/3", Q(HALF_DIMINISHED_SEVENTH), 2},
        {"ii/o6/5", Q(HALF_DIMINISHED_SEVENTH), 1}, {"viio6", Q(DIMINISHED), 1},
        {"viio64", Q(DIMINISHED), 2}, {"viio6/5", Q(FULLY_DIMINISHED_SEVENTH), 1},
        {"viio43", Q(FULLY_DIMINISHED_SEVENTH), 2}, {"IM65", Q(MAJOR_SEVENTH), 1},
        {"III+6", Q(AUGMENTED), 1}};
    for (size_t i = 0; i < sizeof(FIGURES) / sizeof(FIGURES[0]); i++) {
        expect_fields(FIGURES[i].label, 0, 0 + parsed(FIGURES[i].label).degree, FIGURES[i].quality,
                      FIGURES[i].inversion);
    }
    expect_canonical("V6/4", "V64");
    expect_canonical("V6/5", "V65");
    expect_canonical("V4/3", "V43");
    expect_canonical("V4/2", "V42");
    expect_canonical("V2", "V42");
    expect_canonical("ii%7", "ii\xC3\xB8" "7");
    expect_canonical("ii/o6/5", "ii\xC3\xB8" "65");
    expect_canonical("ii\xE2\x88\x85" "7", "ii\xC3\xB8" "7");
    expect_canonical("vii\xC2\xB0" "7", "viio7");
    expect_canonical("IVmaj7", "IVM7");
}

static void test_applied(void) {
    Text text = parsed("V7/V");
    assert(text.has_target && text.quality == Q(DOMINANT_SEVENTH) && text.degree == 5 && text.target_degree == 5 &&
           !text.target_minor && text.target_alteration == 0);
    text = parsed("viio7/ii");
    assert(text.has_target && text.quality == Q(FULLY_DIMINISHED_SEVENTH) && text.degree == 7 &&
           text.target_degree == 2 && text.target_minor);
    text = parsed("V/bVI");
    assert(text.has_target && text.target_degree == 6 && text.target_alteration == -1 && !text.target_minor);
    text = parsed("V65/bVI");
    assert(text.inversion == 1 && text.target_alteration == -1 && text.target_degree == 6);
    text = parsed("bII6/V");
    assert(text.alteration == -1 && text.degree == 2 && text.inversion == 1 && text.target_degree == 5);
    text = parsed("V6/5/V");
    assert(text.quality == Q(DOMINANT_SEVENTH) && text.inversion == 1 && text.target_degree == 5);
    text = parsed("ii/o6/5/V");
    assert(text.quality == Q(HALF_DIMINISHED_SEVENTH) && text.inversion == 1 && text.target_degree == 5);
    text = parsed("V/#iv");
    assert(text.target_alteration == 1 && text.target_degree == 4 && text.target_minor);
    expect_canonical("V6/4/V", "V64/V");
    expect_canonical("viio6/V", "viio6/V");
    expect_canonical("V4/3/\xE2\x99\xAD" "VI", "V43/bVI");
}

static void test_rejected(void) {
    const char *const REJECTED[] = {"",        "x",      "VIII",    "Vi",      "vI",      "Iv",     "viI",    "Vo",
                                    "V\xC2\xB0", "vi+",   "i+",      "V+7",     "viio\xC3\xB8", "vii\xC3\xB8", "vii/o",  "vii%",
                                    "ii\xC3\xB8" "6", "iiM7",  "VM",      "V7 ",     " V7",     "V7x",    "V8",     "V5",
                                    "V66",     "V9",     "V7/",     "V/",      "V/x",     "V/V/V",  "V/V7",   "V/Vo",
                                    "V/V6",    "bb V",   "bbV",     "b",       "#",       "V/b",    "Vmaj",   "viio75",
                                    "vii7o",   "V6/6",   "V4/4",    "V3/4",    "V+/V7",   "\xE2\x99\xAD", "iiio\xC3\xB8"};
    for (size_t i = 0; i < sizeof(REJECTED) / sizeof(REJECTED[0]); i++) {
        expect_rejected(REJECTED[i]);
    }
    Text text = plain(1, Q(MAJOR), 0);
    assert(!standard_tonality_numeral_parse(NULL, &text));
    assert(!standard_tonality_numeral_parse("V", NULL));
}

static void test_format_failures(void) {
    char buffer[64];
    Text text = plain(5, Q(DOMINANT_SEVENTH), 1);
    assert(standard_tonality_numeral_format(&text, buffer, sizeof(buffer)) && strcmp(buffer, "V65") == 0);
    assert(!standard_tonality_numeral_format(&text, buffer, 3)); /* "V65" needs 4 bytes */
    assert(standard_tonality_numeral_format(&text, buffer, 4));
    assert(!standard_tonality_numeral_format(NULL, buffer, sizeof(buffer)));
    assert(!standard_tonality_numeral_format(&text, NULL, sizeof(buffer)));
    assert(!standard_tonality_numeral_format(&text, buffer, 0));

    strcpy(buffer, "keep");
    Text bad = plain(0, Q(MAJOR), 0);
    assert(!standard_tonality_numeral_format(&bad, buffer, sizeof(buffer)) && strcmp(buffer, "keep") == 0);
    bad = plain(8, Q(MAJOR), 0);
    assert(!standard_tonality_numeral_format(&bad, buffer, sizeof(buffer)));
    bad = plain(5, Q(QUALITY_COUNT), 0);
    assert(!standard_tonality_numeral_format(&bad, buffer, sizeof(buffer)));
    bad = plain(5, Q(MAJOR), 3); /* a triad has no third inversion */
    assert(!standard_tonality_numeral_format(&bad, buffer, sizeof(buffer)));
    bad = plain(5, Q(DOMINANT_SEVENTH), 4);
    assert(!standard_tonality_numeral_format(&bad, buffer, sizeof(buffer)));
    bad = plain(5, Q(MAJOR), -1);
    assert(!standard_tonality_numeral_format(&bad, buffer, sizeof(buffer)));
    bad = plain(5, Q(MAJOR), 0);
    bad.alteration = 2;
    assert(!standard_tonality_numeral_format(&bad, buffer, sizeof(buffer)));
    bad = plain(5, Q(MAJOR), 0);
    bad.has_target = true;
    bad.target_degree = 0;
    assert(!standard_tonality_numeral_format(&bad, buffer, sizeof(buffer)));
    bad.target_degree = 5;
    bad.target_alteration = 2;
    assert(!standard_tonality_numeral_format(&bad, buffer, sizeof(buffer)));
    assert(!standard_tonality_numeral_format_styled(&text, (StandardTonalityNumeralStyle)9, buffer, sizeof(buffer)));
    assert(strcmp(buffer, "keep") == 0);
}

static void test_styles(void) {
    char buffer[64];
    Text text = parsed("ii/o6/5/V");
    assert(standard_tonality_numeral_format_styled(&text, STANDARD_TONALITY_NUMERAL_STYLE_CANONICAL, buffer,
                                                   sizeof(buffer)) &&
           strcmp(buffer, "ii\xC3\xB8" "65/V") == 0);
    assert(standard_tonality_numeral_format_styled(&text, STANDARD_TONALITY_NUMERAL_STYLE_DISPLAY, buffer,
                                                   sizeof(buffer)) &&
           strcmp(buffer, "ii\xE2\x88\x85" "6/5/V") == 0);
    assert(standard_tonality_numeral_format_styled(&text, STANDARD_TONALITY_NUMERAL_STYLE_ROMANTEXT, buffer,
                                                   sizeof(buffer)) &&
           strcmp(buffer, "ii/o6/5/V") == 0);
    text = parsed("viio64");
    assert(standard_tonality_numeral_format_styled(&text, STANDARD_TONALITY_NUMERAL_STYLE_DISPLAY, buffer,
                                                   sizeof(buffer)) &&
           strcmp(buffer, "vii\xC2\xB0" "6/4") == 0);
    assert(standard_tonality_numeral_format_styled(&text, STANDARD_TONALITY_NUMERAL_STYLE_ROMANTEXT, buffer,
                                                   sizeof(buffer)) &&
           strcmp(buffer, "viio6/4") == 0);
    /* every display and romantext spelling reads back to the same value */
    for (int quality = 0; quality < Q(QUALITY_COUNT); quality++) {
        for (int style = 0; style <= STANDARD_TONALITY_NUMERAL_STYLE_ROMANTEXT; style++) {
            Text original = plain(4, (StandardTonalityNumeralQuality)quality, 1);
            original.has_target = true;
            original.target_degree = 2;
            original.target_minor = true;
            assert(standard_tonality_numeral_format_styled(&original, (StandardTonalityNumeralStyle)style, buffer,
                                                           sizeof(buffer)));
            Text back = parsed(buffer);
            assert(memcmp(&back, &original, sizeof(Text)) == 0);
        }
    }
}

static int root(const char *label, StandardTonalityMode mode) {
    Text text = parsed(label);
    int semitones = -1;
    assert(standard_tonality_numeral_root_semitone(&text, mode, &semitones));
    return semitones;
}

static void test_root_semitones(void) {
    const StandardTonalityMode major = STANDARD_TONALITY_MODE_MAJOR;
    const StandardTonalityMode natural = STANDARD_TONALITY_MODE_NATURAL_MINOR;
    const StandardTonalityMode harmonic = STANDARD_TONALITY_MODE_HARMONIC_MINOR;
    const StandardTonalityMode melodic = STANDARD_TONALITY_MODE_MELODIC_MINOR;
    assert(root("I", major) == 0 && root("ii", major) == 2 && root("iii", major) == 4 && root("IV", major) == 5);
    assert(root("V7", major) == 7 && root("vi", major) == 9 && root("viio", major) == 11);
    assert(root("bVI", major) == 8 && root("bVII", major) == 10 && root("#iv", major) == 6 && root("bII6", major) == 1);
    assert(root("i", natural) == 0 && root("III", natural) == 3 && root("iv", natural) == 5 && root("VI", natural) == 8);
    assert(root("VII", natural) == 10 && root("viio", natural) == 10);
    assert(root("V", harmonic) == 7 && root("viio7", harmonic) == 11 && root("VI", harmonic) == 8);
    assert(root("vi", melodic) == 9 && root("viio", melodic) == 11 && root("III", melodic) == 3);
    assert(root("#vii", natural) == 11);
    /* applied numerals go through their target */
    assert(root("V/V", major) == 2 && root("V7/V", major) == 2 && root("V/ii", major) == 9);
    assert(root("V/vi", major) == 4 && root("V/IV", major) == 0);
    assert(root("viio7/ii", major) == 1 && root("viio/V", major) == 6);
    assert(root("V/bVI", major) == 3 && root("V65/bVI", major) == 3);
    assert(root("V/III", natural) == 10 && root("V/V", harmonic) == 2 && root("V/iv", natural) == 0);
    assert(root("viio/iv", natural) == 4);
    assert(root("V/#iv", major) == 1);

    Text text = parsed("V");
    int semitones = 99;
    assert(!standard_tonality_numeral_root_semitone(&text, (StandardTonalityMode)9, &semitones) && semitones == 99);
    assert(!standard_tonality_numeral_root_semitone(&text, major, NULL));
    assert(!standard_tonality_numeral_root_semitone(NULL, major, &semitones));
    Text bad = plain(0, Q(MAJOR), 0);
    assert(!standard_tonality_numeral_root_semitone(&bad, major, &semitones) && semitones == 99);
}

static void test_base_and_from_base(void) {
    StandardTonalityRomanNumeral base;
    assert(standard_tonality_numeral_base(&(Text){.degree = 5, .quality = Q(MAJOR)}, &base) && !base.seventh &&
           base.degree == 5 && base.triad == STANDARD_TONALITY_TRIAD_MAJOR);
    Text text = parsed("viio6/V");
    assert(standard_tonality_numeral_base(&text, &base) && !base.seventh && base.degree == 7 &&
           base.triad == STANDARD_TONALITY_TRIAD_DIMINISHED); /* inversion and target dropped */
    text = parsed("bVI");
    assert(standard_tonality_numeral_base(&text, &base) && base.degree == 6 &&
           base.triad == STANDARD_TONALITY_TRIAD_MAJOR);
    text = parsed("III+");
    assert(standard_tonality_numeral_base(&text, &base) && base.triad == STANDARD_TONALITY_TRIAD_AUGMENTED);
    text = parsed("ii");
    assert(standard_tonality_numeral_base(&text, &base) && base.triad == STANDARD_TONALITY_TRIAD_MINOR);

    text = parsed("V7");
    assert(standard_tonality_numeral_base(&text, &base) && base.seventh &&
           base.seventh_quality == STANDARD_TONALITY_SEVENTH_MAJOR_MINOR);
    text = parsed("VII7");
    assert(standard_tonality_numeral_base(&text, &base) &&
           base.seventh_quality == STANDARD_TONALITY_SEVENTH_MAJOR_MINOR);
    text = parsed("I7");
    assert(standard_tonality_numeral_base(&text, &base) &&
           base.seventh_quality == STANDARD_TONALITY_SEVENTH_MAJOR_MAJOR);
    text = parsed("V65");
    assert(standard_tonality_numeral_base(&text, &base) &&
           base.seventh_quality == STANDARD_TONALITY_SEVENTH_MAJOR_MINOR);
    text = parsed("VM7");
    assert(standard_tonality_numeral_base(&text, &base) &&
           base.seventh_quality == STANDARD_TONALITY_SEVENTH_MAJOR_MAJOR);
    text = parsed("ii7");
    assert(standard_tonality_numeral_base(&text, &base) &&
           base.seventh_quality == STANDARD_TONALITY_SEVENTH_MINOR_MINOR);
    text = parsed("ii/o7");
    assert(standard_tonality_numeral_base(&text, &base) &&
           base.seventh_quality == STANDARD_TONALITY_SEVENTH_HALF_DIMINISHED);
    text = parsed("viio7");
    assert(standard_tonality_numeral_base(&text, &base) &&
           base.seventh_quality == STANDARD_TONALITY_SEVENTH_FULLY_DIMINISHED);

    Text bad = plain(0, Q(MAJOR), 0);
    base.degree = 9;
    base.seventh = false;
    base.triad = STANDARD_TONALITY_TRIAD_MINOR;
    base.seventh_quality = STANDARD_TONALITY_SEVENTH_MINOR_MINOR;
    assert(!standard_tonality_numeral_base(&bad, &base) && base.degree == 9 && !base.seventh &&
           base.triad == STANDARD_TONALITY_TRIAD_MINOR && base.seventh_quality == STANDARD_TONALITY_SEVENTH_MINOR_MINOR);
    assert(!standard_tonality_numeral_base(&text, NULL));
    assert(!standard_tonality_numeral_base(NULL, &base));

    /* from_base: plain text, both major-based sevenths read as the dominant seventh */
    StandardTonalityRomanNumeral numeral;
    assert(standard_tonality_roman_numeral_seventh_chord(1, STANDARD_TONALITY_SEVENTH_MAJOR_MAJOR, &numeral));
    assert(standard_tonality_numeral_from_base(&numeral, &text) && text.quality == Q(DOMINANT_SEVENTH) &&
           text.degree == 1 && text.inversion == 0 && !text.has_target && text.alteration == 0);
    assert(standard_tonality_roman_numeral_triad(7, STANDARD_TONALITY_TRIAD_DIMINISHED, &numeral));
    assert(standard_tonality_numeral_from_base(&numeral, &text) && text.quality == Q(DIMINISHED) && text.degree == 7);
    numeral.degree = 8;
    assert(!standard_tonality_numeral_from_base(&numeral, &text));
    assert(!standard_tonality_numeral_from_base(NULL, &text));
    assert(!standard_tonality_numeral_from_base(&numeral, NULL));
}

/* Labels the accuracy fixtures (tests/accuracy annotations) write; each must parse. */
static void test_accuracy_labels(void) {
    const char *const LABELS[] = {
        "I", "i", "V", "v", "V7", "vi", "IV", "V6", "I6", "viio6", "IV6", "ii", "V6/5", "iv6", "i6", "V2", "ii6/5",
        "VI", "i64", "I64", "III", "iv", "iio6", "iii", "ii6", "VII", "viio7", "v6", "iv7", "ii7", "V7/IV", "V6/V",
        "V6/5/V", "V4/3", "viio64", "viio6/5", "vii", "vi6/5", "III6", "V7/V", "viio7/V", "viio6/V", "iv6/iv",
        "iii6", "ii2", "III+", "III+6", "V65/bVI", "bII6/V", "bVI", "bVII", "ii%7", "ii/o2", "ii/o4/3", "ii/o6/5",
        "vii%7", "vii/o4/3", "vii/o7"};
    for (size_t i = 0; i < sizeof(LABELS) / sizeof(LABELS[0]); i++) {
        char buffer[64];
        Text text = parsed(LABELS[i]);
        assert(standard_tonality_numeral_format(&text, buffer, sizeof(buffer)));
        Text again = parsed(buffer);
        assert(memcmp(&again, &text, sizeof(Text)) == 0);
    }
}

int main(void) {
    test_every_numeral_and_quality();
    test_case_and_marks();
    test_alterations();
    test_figures_and_slash_spellings();
    test_applied();
    test_rejected();
    test_format_failures();
    test_styles();
    test_root_semitones();
    test_base_and_from_base();
    test_accuracy_labels();
    return 0;
}
