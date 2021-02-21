#include <assert.h>
#include <stdio.h>

#include "framework.h"
#include "spelling_test_util.h"
#include "triad_in_key_rules.h"

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

static void destroy_triad(PitchSpelling *triad[3]) {
    for (size_t i = 0; i < 3; i++) {
        pitch_spelling_destroy(triad[i]);
    }
}

static void test_rules_reference_framework(void) {
    for (int i = 0; i < STANDARD_TONALITY_TRIAD_IN_KEY_RULE_COUNT; i++) {
        const Rule *rule = standard_tonality_triad_in_key_rule((StandardTonalityTriadInKeyRule)i);
        const Constraint *constraint = standard_tonality_triad_in_key_constraint((StandardTonalityTriadInKeyRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), standard_tonality_framework()));
        assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    }
    assert(standard_tonality_triad_in_key_rule(STANDARD_TONALITY_TRIAD_IN_KEY_RULE_COUNT) == NULL);
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
    PitchSpelling *triad[3] = {NULL, NULL, NULL};
    assert(!standard_tonality_spell_diatonic_triad(STANDARD_TONALITY_TRIAD_KEY_MAJOR, c4, 0, triad));
    assert(!standard_tonality_spell_diatonic_triad(STANDARD_TONALITY_TRIAD_KEY_MAJOR, c4, 8, triad));
    assert(!standard_tonality_spell_diatonic_triad((Key)99, c4, 1, triad));
    assert(!standard_tonality_spell_diatonic_triad(STANDARD_TONALITY_TRIAD_KEY_MAJOR, NULL, 1, triad));
    assert(!standard_tonality_spell_diatonic_triad(STANDARD_TONALITY_TRIAD_KEY_MAJOR, c4, 1, NULL));
    assert(triad[0] == NULL);
    pitch_spelling_destroy(c4);

    /* A key whose scale needs a triple accidental has no triads. */
    PitchSpelling *g_double_sharp = spell(PITCH_LETTER_G, 2, 4);
    assert(!standard_tonality_spell_diatonic_triad(STANDARD_TONALITY_TRIAD_KEY_MAJOR, g_double_sharp, 1, triad));
    assert(triad[0] == NULL);
    pitch_spelling_destroy(g_double_sharp);
}

/* Example 6: G major. */
static void test_g_major_triads(void) {
    PitchSpelling *g = spell(PITCH_LETTER_G, 0, 3);
    PitchLetter roots[] = {PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_B, PITCH_LETTER_C,
                           PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F};
    int root_accidentals[] = {0, 0, 0, 0, 0, 0, 1}; /* F-sharp */
    TriadQuality expected[] = {MAJOR, MINOR, MINOR, MAJOR, MAJOR, MINOR, DIMINISHED};
    for (int degree = 1; degree <= 7; degree++) {
        PitchSpelling *triad[3];
        assert(standard_tonality_spell_diatonic_triad(STANDARD_TONALITY_TRIAD_KEY_MAJOR, g, degree, triad));
        assert(pitch_spelling_letter(triad[0]) == roots[degree - 1]);
        assert(pitch_spelling_accidental(triad[0]) == root_accidentals[degree - 1]);
        TriadQuality quality = (TriadQuality)42;
        assert(standard_tonality_triad_stack_quality(triad[0], triad[1], triad[2], &quality));
        assert(quality == expected[degree - 1]);
        destroy_triad(triad);
    }
    pitch_spelling_destroy(g);
}

/* Example 7: G minor, natural and with the raised leading tone. */
static void test_g_minor_triads(void) {
    PitchSpelling *g = spell(PITCH_LETTER_G, 0, 3);
    PitchSpelling *triad[3];

    /* sol without the raised leading tone: D minor (D F A). */
    assert(standard_tonality_spell_diatonic_triad(STANDARD_TONALITY_TRIAD_KEY_MINOR_NATURAL, g, 5, triad));
    assert(pitch_spelling_letter(triad[0]) == PITCH_LETTER_D);
    assert(pitch_spelling_accidental(triad[1]) == 0); /* F natural */
    destroy_triad(triad);

    /* sol with it: D major (D F-sharp A). */
    assert(standard_tonality_spell_diatonic_triad(STANDARD_TONALITY_TRIAD_KEY_MINOR_HARMONIC, g, 5, triad));
    assert(pitch_spelling_letter(triad[0]) == PITCH_LETTER_D);
    assert(pitch_spelling_letter(triad[1]) == PITCH_LETTER_F && pitch_spelling_accidental(triad[1]) == 1);
    destroy_triad(triad);

    /* ti, raised: F-sharp diminished (F-sharp A C). */
    assert(standard_tonality_spell_diatonic_triad(STANDARD_TONALITY_TRIAD_KEY_MINOR_HARMONIC, g, 7, triad));
    assert(pitch_spelling_letter(triad[0]) == PITCH_LETTER_F && pitch_spelling_accidental(triad[0]) == 1);
    TriadQuality quality = (TriadQuality)42;
    assert(standard_tonality_triad_stack_quality(triad[0], triad[1], triad[2], &quality));
    assert(quality == DIMINISHED);
    destroy_triad(triad);

    /* te, natural: F major (F A C). */
    assert(standard_tonality_spell_diatonic_triad(STANDARD_TONALITY_TRIAD_KEY_MINOR_NATURAL, g, 7, triad));
    assert(pitch_spelling_letter(triad[0]) == PITCH_LETTER_F && pitch_spelling_accidental(triad[0]) == 0);
    assert(standard_tonality_triad_stack_quality(triad[0], triad[1], triad[2], &quality));
    assert(quality == MAJOR);
    destroy_triad(triad);

    pitch_spelling_destroy(g);
}

/* KEY_INDEPENDENT, property: in every key with a scale, the triad on each
   degree has the quality the table gives -- the qualities do not change
   with the key. */
static void test_qualities_hold_in_every_key(void) {
    PitchLetter letters[] = {PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F,
                             PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_B};
    int checked = 0;
    for (int key = 0; key < STANDARD_TONALITY_TRIAD_KEY_COUNT; key++) {
        for (size_t l = 0; l < 7; l++) {
            for (int accidental = -1; accidental <= 1; accidental++) {
                PitchSpelling *tonic = spell(letters[l], accidental, 3);
                for (int degree = 1; degree <= 7; degree++) {
                    PitchSpelling *triad[3];
                    if (!standard_tonality_spell_diatonic_triad((Key)key, tonic, degree, triad)) {
                        continue;
                    }
                    TriadQuality quality = (TriadQuality)42;
                    assert(standard_tonality_triad_stack_quality(triad[0], triad[1], triad[2], &quality));
                    assert(quality == quality_in((Key)key, degree));
                    assert(standard_tonality_triad_on_lines_or_spaces(triad[0], triad[1], triad[2]));
                    destroy_triad(triad);
                    checked++;
                }
                pitch_spelling_destroy(tonic);
            }
        }
    }
    assert(checked > 400);
}

int main(void) {
    test_rules_reference_framework();
    test_major_key();
    test_natural_minor_key();
    test_raised_leading_tone();
    test_raised_leading_tone_changes_only_the_triads_that_contain_it();
    test_rejects_invalid_arguments();
    test_g_major_triads();
    test_g_minor_triads();
    test_qualities_hold_in_every_key();
    printf("test_triad_in_key_rules (standard-tonality): ok\n");
    return 0;
}
