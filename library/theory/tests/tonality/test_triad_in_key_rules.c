#include <assert.h>
#include <stdio.h>

#include "theory/tests/tonality/spelling_test_util.h"
#include "theory/tonality/triad_in_key_rules.h"

typedef StandardTonalityTriadQuality TriadQuality;
typedef StandardTonalityTriadKey Key;

#define MAJOR STANDARD_TONALITY_TRIAD_MAJOR
#define MINOR STANDARD_TONALITY_TRIAD_MINOR
#define DIMINISHED STANDARD_TONALITY_TRIAD_DIMINISHED
#define AUGMENTED STANDARD_TONALITY_TRIAD_AUGMENTED

static TriadQuality quality_in(Key key, int degree) {
    TriadQuality quality = (TriadQuality)42;
    assert(standard_tonality_triad_quality_in_key(key, degree, &quality));
    return quality;
}

/* MAJOR_KEY, Example 6: major on do fa sol; minor on re mi la; diminished
   on ti. */
static void test_major_key(void) {
    TriadQuality expected[] = {MAJOR, MINOR, MINOR, MAJOR, MAJOR, MINOR, DIMINISHED};
    for (int degree = 1; degree <= 7; degree++) {
        assert(quality_in(STANDARD_TONALITY_TRIAD_KEY_MAJOR, degree) == expected[degree - 1]);
    }
}

/* MINOR_KEY, Example 7: minor on do fa sol; major on me le te; diminished
   on re. */
static void test_natural_minor_key(void) {
    TriadQuality expected[] = {MINOR, DIMINISHED, MAJOR, MINOR, MINOR, MAJOR, MAJOR};
    for (int degree = 1; degree <= 7; degree++) {
        assert(quality_in(STANDARD_TONALITY_TRIAD_KEY_MINOR_NATURAL, degree) == expected[degree - 1]);
    }
}

/* RAISED_LEADING_TONE: sol is major, ti diminished, re still diminished. */
static void test_raised_leading_tone(void) {
    assert(quality_in(STANDARD_TONALITY_TRIAD_KEY_MINOR_HARMONIC, 5) == MAJOR);
    assert(quality_in(STANDARD_TONALITY_TRIAD_KEY_MINOR_HARMONIC, 7) == DIMINISHED);
    assert(quality_in(STANDARD_TONALITY_TRIAD_KEY_MINOR_HARMONIC, 2) == DIMINISHED);
    /* do, fa, le stay as in natural minor. */
    assert(quality_in(STANDARD_TONALITY_TRIAD_KEY_MINOR_HARMONIC, 1) == MINOR);
    assert(quality_in(STANDARD_TONALITY_TRIAD_KEY_MINOR_HARMONIC, 4) == MINOR);
    assert(quality_in(STANDARD_TONALITY_TRIAD_KEY_MINOR_HARMONIC, 6) == MAJOR);
    /* The triad on me holds the raised leading tone too: augmented. */
    assert(quality_in(STANDARD_TONALITY_TRIAD_KEY_MINOR_HARMONIC, 3) == AUGMENTED);
}

/* Raising the leading tone changes exactly the triads that contain it: the
   ones on me, sol, and ti. */
static void test_raised_leading_tone_changes_only_the_triads_that_contain_it(void) {
    for (int degree = 1; degree <= 7; degree++) {
        bool contains_leading_tone = degree == 3 || degree == 5 || degree == 7;
        bool changed = quality_in(STANDARD_TONALITY_TRIAD_KEY_MINOR_NATURAL, degree) !=
                       quality_in(STANDARD_TONALITY_TRIAD_KEY_MINOR_HARMONIC, degree);
        assert(changed == contains_leading_tone);
    }
}

static void test_rejects_invalid_arguments(void) {
    TriadQuality quality = (TriadQuality)42;
    assert(!standard_tonality_triad_quality_in_key(STANDARD_TONALITY_TRIAD_KEY_MAJOR, 0, &quality));
    assert(!standard_tonality_triad_quality_in_key(STANDARD_TONALITY_TRIAD_KEY_MAJOR, 8, &quality));
    assert(!standard_tonality_triad_quality_in_key((Key)99, 1, &quality));
    assert(!standard_tonality_triad_quality_in_key(STANDARD_TONALITY_TRIAD_KEY_MAJOR, 1, NULL));
    assert(quality == (TriadQuality)42);

    PitchSpelling *c4 = spell(PITCH_LETTER_C, 0, 4);
    pitch_spelling_destroy(c4);

    /* A key whose scale needs a triple accidental has no triads. */
    PitchSpelling *g_double_sharp = spell(PITCH_LETTER_G, 2, 4);
    pitch_spelling_destroy(g_double_sharp);
}

int main(void) {
    test_major_key();
    test_natural_minor_key();
    test_raised_leading_tone();
    test_raised_leading_tone_changes_only_the_triads_that_contain_it();
    test_rejects_invalid_arguments();
    printf("test_triad_in_key_rules (standard-tonality): ok\n");
    return 0;
}
