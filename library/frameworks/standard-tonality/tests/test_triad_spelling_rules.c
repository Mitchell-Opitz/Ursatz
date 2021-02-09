#include <assert.h>
#include <stdio.h>

#include "framework.h"
#include "spelling_test_util.h"
#include "triad_spelling_rules.h"

typedef StandardTonalityTriadQuality TriadQuality;

#define MAJOR STANDARD_TONALITY_TRIAD_MAJOR
#define MINOR STANDARD_TONALITY_TRIAD_MINOR
#define DIMINISHED STANDARD_TONALITY_TRIAD_DIMINISHED
#define AUGMENTED STANDARD_TONALITY_TRIAD_AUGMENTED

static void destroy_triad(PitchSpelling *triad[3]) {
    for (size_t i = 0; i < 3; i++) {
        pitch_spelling_destroy(triad[i]);
    }
}

static void check_triad(PitchLetter root_letter, int root_accidental, TriadQuality quality, const PitchLetter letters[3],
                        const int accidentals[3]) {
    PitchSpelling *root = spell(root_letter, root_accidental, 4);

    PitchSpelling *triad[3];
    assert(standard_tonality_spell_triad(root, quality, triad));
    for (size_t i = 0; i < 3; i++) {
        assert(pitch_spelling_letter(triad[i]) == letters[i]);
        assert(pitch_spelling_accidental(triad[i]) == accidentals[i]);
    }
    assert(pitch_spelling_equals(triad[0], root));

    /* The result is a stack in thirds of the quality asked for. */
    TriadQuality found = (TriadQuality)42;
    assert(standard_tonality_triad_stack_quality(triad[0], triad[1], triad[2], &found));
    assert(found == quality);
    assert(standard_tonality_triad_on_lines_or_spaces(triad[0], triad[1], triad[2]));
    destroy_triad(triad);

    /* By the written steps, where the root's key exists. */
    PitchSpelling *by_key[3];
    if (standard_tonality_spell_triad_by_key_signature(root, quality, by_key)) {
        for (size_t i = 0; i < 3; i++) {
            assert(pitch_spelling_letter(by_key[i]) == letters[i]);
            assert(pitch_spelling_accidental(by_key[i]) == accidentals[i]);
        }
        destroy_triad(by_key);
    }
    pitch_spelling_destroy(root);
}

static void test_rules_reference_framework(void) {
    for (int i = 0; i < STANDARD_TONALITY_TRIAD_SPELLING_RULE_COUNT; i++) {
        const Rule *rule = standard_tonality_triad_spelling_rule((StandardTonalityTriadSpellingRule)i);
        const Constraint *constraint = standard_tonality_triad_spelling_constraint((StandardTonalityTriadSpellingRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), standard_tonality_framework()));
        assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    }
    assert(standard_tonality_triad_spelling_rule(STANDARD_TONALITY_TRIAD_SPELLING_RULE_COUNT) == NULL);
}

/* ALTERATION: minor lowers the third, diminished lowers both, augmented
   raises the fifth. */
static void test_alteration(void) {
    int third = 42;
    int fifth = 42;
    assert(standard_tonality_triad_alteration(MAJOR, &third, &fifth) && third == 0 && fifth == 0);
    assert(standard_tonality_triad_alteration(MINOR, &third, &fifth) && third == -1 && fifth == 0);
    assert(standard_tonality_triad_alteration(DIMINISHED, &third, &fifth) && third == -1 && fifth == -1);
    assert(standard_tonality_triad_alteration(AUGMENTED, &third, &fifth) && third == 0 && fifth == 1);
    assert(!standard_tonality_triad_alteration((TriadQuality)99, &third, &fifth));
    assert(!standard_tonality_triad_alteration(MAJOR, NULL, &fifth));
    assert(!standard_tonality_triad_alteration(MAJOR, &third, NULL));
}

/* Example 8: D major is D F-sharp A; the key's C-sharp is not needed. */
static void test_d_major(void) {
    PitchLetter letters[3] = {PITCH_LETTER_D, PITCH_LETTER_F, PITCH_LETTER_A};
    int accidentals[3] = {0, 1, 0};
    check_triad(PITCH_LETTER_D, 0, MAJOR, letters, accidentals);
}

/* Example 9: A-flat minor. A-flat major is A-flat C E-flat; the third is
   lowered to C-flat. */
static void test_a_flat_minor(void) {
    PitchLetter letters[3] = {PITCH_LETTER_A, PITCH_LETTER_C, PITCH_LETTER_E};
    int major[3] = {-1, 0, -1};
    int minor[3] = {-1, -1, -1};
    check_triad(PITCH_LETTER_A, -1, MAJOR, letters, major);
    check_triad(PITCH_LETTER_A, -1, MINOR, letters, minor);
}

static void test_every_quality_on_c(void) {
    PitchLetter letters[3] = {PITCH_LETTER_C, PITCH_LETTER_E, PITCH_LETTER_G};
    int major[3] = {0, 0, 0};
    int minor[3] = {0, -1, 0};
    int diminished[3] = {0, -1, -1};
    int augmented[3] = {0, 0, 1};
    check_triad(PITCH_LETTER_C, 0, MAJOR, letters, major);
    check_triad(PITCH_LETTER_C, 0, MINOR, letters, minor);
    check_triad(PITCH_LETTER_C, 0, DIMINISHED, letters, diminished);
    check_triad(PITCH_LETTER_C, 0, AUGMENTED, letters, augmented);
}

/* The symbols the source mentions: B-flat minor, F-sharp diminished. */
static void test_source_symbols(void) {
    PitchLetter b_letters[3] = {PITCH_LETTER_B, PITCH_LETTER_D, PITCH_LETTER_F};
    int b_flat_minor[3] = {-1, -1, 0};
    check_triad(PITCH_LETTER_B, -1, MINOR, b_letters, b_flat_minor);

    PitchLetter f_letters[3] = {PITCH_LETTER_F, PITCH_LETTER_A, PITCH_LETTER_C};
    int f_sharp_diminished[3] = {1, 0, 0};
    check_triad(PITCH_LETTER_F, 1, DIMINISHED, f_letters, f_sharp_diminished);
}

/* Octave numbers follow the letters: B4's third and fifth are in octave 5. */
static void test_octaves_follow_the_letters(void) {
    PitchSpelling *b4 = spell(PITCH_LETTER_B, 0, 4);
    PitchSpelling *triad[3];
    assert(standard_tonality_spell_triad(b4, MAJOR, triad));
    assert(pitch_spelling_octave(triad[0]) == 4);
    assert(pitch_spelling_octave(triad[1]) == 5); /* D-sharp */
    assert(pitch_spelling_octave(triad[2]) == 5); /* F-sharp */
    destroy_triad(triad);
    pitch_spelling_destroy(b4);
}

/* Property: for every root with a spelled triad, the interval-rule
   spelling is a stack in thirds of the right quality, and agrees with the
   key signature steps wherever the root's major scale exists. */
static void test_agrees_with_the_key_signature_steps(void) {
    PitchLetter letters[] = {PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F,
                             PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_B};
    int compared = 0;
    for (size_t l = 0; l < 7; l++) {
        for (int accidental = -2; accidental <= 2; accidental++) {
            PitchSpelling *root = spell(letters[l], accidental, 3);
            for (int q = 0; q < STANDARD_TONALITY_TRIAD_QUALITY_COUNT; q++) {
                PitchSpelling *by_intervals[3];
                PitchSpelling *by_key[3];
                bool intervals = standard_tonality_spell_triad(root, (TriadQuality)q, by_intervals);
                bool key = standard_tonality_spell_triad_by_key_signature(root, (TriadQuality)q, by_key);
                if (intervals) {
                    TriadQuality found = (TriadQuality)42;
                    assert(standard_tonality_triad_stack_quality(by_intervals[0], by_intervals[1], by_intervals[2],
                                                                 &found));
                    assert(found == (TriadQuality)q);
                }
                if (intervals && key) {
                    for (size_t i = 0; i < 3; i++) {
                        assert(pitch_spelling_equals(by_intervals[i], by_key[i]));
                    }
                    compared++;
                }
                if (intervals) {
                    destroy_triad(by_intervals);
                }
                if (key) {
                    destroy_triad(by_key);
                }
            }
            pitch_spelling_destroy(root);
        }
    }
    assert(compared > 60);
}

static void test_rejects_invalid_arguments(void) {
    PitchSpelling *c4 = spell(PITCH_LETTER_C, 0, 4);
    PitchSpelling *triad[3] = {NULL, NULL, NULL};
    assert(!standard_tonality_spell_triad(NULL, MAJOR, triad));
    assert(!standard_tonality_spell_triad(c4, (TriadQuality)99, triad));
    assert(!standard_tonality_spell_triad(c4, MAJOR, NULL));
    assert(!standard_tonality_spell_triad_by_key_signature(NULL, MAJOR, triad));
    assert(!standard_tonality_spell_triad_by_key_signature(c4, (TriadQuality)99, triad));
    assert(!standard_tonality_spell_triad_by_key_signature(c4, MAJOR, NULL));
    assert(triad[0] == NULL);
    pitch_spelling_destroy(c4);

    /* A root whose major scale needs a triple accidental: no key-signature
       spelling, and the augmented triad needs a triple sharp too. */
    PitchSpelling *g_double_sharp = spell(PITCH_LETTER_G, 2, 4);
    assert(!standard_tonality_spell_triad_by_key_signature(g_double_sharp, MAJOR, triad));
    assert(!standard_tonality_spell_triad(g_double_sharp, AUGMENTED, triad));
    assert(triad[0] == NULL);
    pitch_spelling_destroy(g_double_sharp);
}

int main(void) {
    test_rules_reference_framework();
    test_alteration();
    test_d_major();
    test_a_flat_minor();
    test_every_quality_on_c();
    test_source_symbols();
    test_octaves_follow_the_letters();
    test_agrees_with_the_key_signature_steps();
    test_rejects_invalid_arguments();
    printf("test_triad_spelling_rules (standard-tonality): ok\n");
    return 0;
}
