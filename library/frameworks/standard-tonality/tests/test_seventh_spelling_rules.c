#include <assert.h>
#include <stdio.h>

#include "framework.h"
#include "seventh_spelling_rules.h"
#include "spelling_test_util.h"

typedef StandardTonalitySeventhQuality SeventhQuality;

#define MAMA STANDARD_TONALITY_SEVENTH_MAJOR_MAJOR
#define MAMI STANDARD_TONALITY_SEVENTH_MAJOR_MINOR
#define MIMI STANDARD_TONALITY_SEVENTH_MINOR_MINOR
#define HDIM STANDARD_TONALITY_SEVENTH_HALF_DIMINISHED
#define FDIM STANDARD_TONALITY_SEVENTH_FULLY_DIMINISHED

static void destroy4(PitchSpelling *notes[4]) {
    for (int i = 0; i < 4; i++) {
        pitch_spelling_destroy(notes[i]);
    }
}

static void check_note(const PitchSpelling *note, PitchLetter letter, int accidental) {
    assert(pitch_spelling_letter(note) == letter);
    assert(pitch_spelling_accidental(note) == accidental);
}

/* Checks both spelling routes against the expected four notes. */
static void check_spelled(PitchLetter root_letter, int root_accidental, SeventhQuality quality, const PitchLetter *letters,
                          const int *accidentals) {
    PitchSpelling *root = spell(root_letter, root_accidental, 4);

    PitchSpelling *by_intervals[4];
    assert(standard_tonality_spell_seventh(root, quality, by_intervals));
    for (int i = 0; i < 4; i++) {
        check_note(by_intervals[i], letters[i], accidentals[i]);
    }

    PitchSpelling *by_key[4];
    assert(standard_tonality_spell_seventh_by_key_signature(root, quality, by_key));
    for (int i = 0; i < 4; i++) {
        check_note(by_key[i], letters[i], accidentals[i]);
        assert(pitch_spelling_octave(by_key[i]) == pitch_spelling_octave(by_intervals[i]));
    }

    destroy4(by_intervals);
    destroy4(by_key);
    pitch_spelling_destroy(root);
}

static void test_rules_reference_framework(void) {
    for (int i = 0; i < STANDARD_TONALITY_SEVENTH_SPELLING_RULE_COUNT; i++) {
        const Rule *rule = standard_tonality_seventh_spelling_rule((StandardTonalitySeventhSpellingRule)i);
        const Constraint *constraint =
            standard_tonality_seventh_spelling_constraint((StandardTonalitySeventhSpellingRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), standard_tonality_framework()));
        assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    }
    assert(standard_tonality_seventh_spelling_rule(STANDARD_TONALITY_SEVENTH_SPELLING_RULE_COUNT) == NULL);
}

static void test_alteration(void) {
    int third;
    int fifth;
    int seventh;
    assert(standard_tonality_seventh_alteration(MAMA, &third, &fifth, &seventh));
    assert(third == 0 && fifth == 0 && seventh == 0);
    assert(standard_tonality_seventh_alteration(MAMI, &third, &fifth, &seventh));
    assert(third == 0 && fifth == 0 && seventh == -1);
    assert(standard_tonality_seventh_alteration(MIMI, &third, &fifth, &seventh));
    assert(third == -1 && fifth == 0 && seventh == -1);
    assert(standard_tonality_seventh_alteration(HDIM, &third, &fifth, &seventh));
    assert(third == -1 && fifth == -1 && seventh == -1);
    assert(standard_tonality_seventh_alteration(FDIM, &third, &fifth, &seventh));
    assert(third == -1 && fifth == -1 && seventh == -2);

    assert(!standard_tonality_seventh_alteration((SeventhQuality)99, &third, &fifth, &seventh));
    assert(!standard_tonality_seventh_alteration(MAMA, NULL, &fifth, &seventh));
    assert(!standard_tonality_seventh_alteration(MAMA, &third, NULL, &seventh));
    assert(!standard_tonality_seventh_alteration(MAMA, &third, &fifth, NULL));
}

/* Example 8: D major-major seventh is D F-sharp A C-sharp. */
static void test_example_8(void) {
    PitchLetter letters[] = {PITCH_LETTER_D, PITCH_LETTER_F, PITCH_LETTER_A, PITCH_LETTER_C};
    int accidentals[] = {0, 1, 0, 1};
    check_spelled(PITCH_LETTER_D, 0, MAMA, letters, accidentals);
}

/* Example 9: A-flat fully diminished is A-flat C-flat E-double-flat
   G-double-flat. */
static void test_example_9(void) {
    PitchLetter letters[] = {PITCH_LETTER_A, PITCH_LETTER_C, PITCH_LETTER_E, PITCH_LETTER_G};
    int accidentals[] = {-1, -1, -2, -2};
    check_spelled(PITCH_LETTER_A, -1, FDIM, letters, accidentals);
}

/* Every quality on F, Example 4's shape. */
static void test_every_quality_on_f(void) {
    PitchLetter letters[] = {PITCH_LETTER_F, PITCH_LETTER_A, PITCH_LETTER_C, PITCH_LETTER_E};
    int major_major[] = {0, 0, 0, 0};
    int major_minor[] = {0, 0, 0, -1};
    int minor_minor[] = {0, -1, 0, -1};
    int half_diminished[] = {0, -1, -1, -1};
    int fully_diminished[] = {0, -1, -1, -2};
    check_spelled(PITCH_LETTER_F, 0, MAMA, letters, major_major);
    check_spelled(PITCH_LETTER_F, 0, MAMI, letters, major_minor);
    check_spelled(PITCH_LETTER_F, 0, MIMI, letters, minor_minor);
    check_spelled(PITCH_LETTER_F, 0, HDIM, letters, half_diminished);
    check_spelled(PITCH_LETTER_F, 0, FDIM, letters, fully_diminished);
}

/* B-flat minor-minor and G-sharp major-major, the chord symbols' examples. */
static void test_accidental_roots(void) {
    PitchLetter b_flat[] = {PITCH_LETTER_B, PITCH_LETTER_D, PITCH_LETTER_F, PITCH_LETTER_A};
    int b_flat_minor[] = {-1, -1, 0, -1};
    check_spelled(PITCH_LETTER_B, -1, MIMI, b_flat, b_flat_minor);

    PitchLetter g_sharp[] = {PITCH_LETTER_G, PITCH_LETTER_B, PITCH_LETTER_D, PITCH_LETTER_F};
    int g_sharp_major[] = {1, 1, 1, 2};
    check_spelled(PITCH_LETTER_G, 1, MAMA, g_sharp, g_sharp_major);
}

/* The seventh chord's triad and seventh agree with the interval rules. */
static void test_agrees_with_interval_rules(void) {
    PitchLetter letters[] = {PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F,
                             PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_B};
    int spelled = 0;
    for (size_t l = 0; l < 7; l++) {
        for (int accidental = -1; accidental <= 1; accidental++) {
            for (int q = 0; q < STANDARD_TONALITY_SEVENTH_QUALITY_COUNT; q++) {
                PitchSpelling *root = spell(letters[l], accidental, 4);
                PitchSpelling *notes[4];
                if (!standard_tonality_spell_seventh(root, (SeventhQuality)q, notes)) {
                    /* Beyond a double accidental. */
                    pitch_spelling_destroy(root);
                    continue;
                }
                SeventhQuality read;
                assert(standard_tonality_seventh_stack_quality(notes[0], notes[1], notes[2], notes[3], &read));
                assert(read == (SeventhQuality)q);
                spelled++;
                destroy4(notes);
                pitch_spelling_destroy(root);
            }
        }
    }
    assert(spelled > 0);
}

/* G double-sharp has no major scale (its key signature is imaginary): the
   written steps give no answer, but the interval route spells its fully
   diminished seventh chord, G double-sharp B-sharp D-sharp F-sharp. */
static void test_imaginary_root(void) {
    PitchSpelling *root = spell(PITCH_LETTER_G, 2, 4);
    PitchSpelling *notes[4] = {NULL, NULL, NULL, NULL};
    assert(!standard_tonality_spell_seventh_by_key_signature(root, FDIM, notes));
    for (int i = 0; i < 4; i++) {
        assert(notes[i] == NULL);
    }

    assert(standard_tonality_spell_seventh(root, FDIM, notes));
    check_note(notes[0], PITCH_LETTER_G, 2);
    check_note(notes[1], PITCH_LETTER_B, 1);
    check_note(notes[2], PITCH_LETTER_D, 1);
    check_note(notes[3], PITCH_LETTER_F, 1);
    destroy4(notes);
    pitch_spelling_destroy(root);
}

static void test_rejects_invalid_arguments(void) {
    PitchSpelling *root = spell(PITCH_LETTER_C, 0, 4);
    PitchSpelling *notes[4] = {NULL, NULL, NULL, NULL};
    assert(!standard_tonality_spell_seventh(NULL, MAMA, notes));
    assert(!standard_tonality_spell_seventh(root, (SeventhQuality)99, notes));
    assert(!standard_tonality_spell_seventh(root, MAMA, NULL));
    assert(!standard_tonality_spell_seventh_by_key_signature(NULL, MAMA, notes));
    assert(!standard_tonality_spell_seventh_by_key_signature(root, (SeventhQuality)99, notes));
    assert(!standard_tonality_spell_seventh_by_key_signature(root, MAMA, NULL));
    for (int i = 0; i < 4; i++) {
        assert(notes[i] == NULL);
    }
    pitch_spelling_destroy(root);
}

int main(void) {
    test_rules_reference_framework();
    test_alteration();
    test_example_8();
    test_example_9();
    test_every_quality_on_f();
    test_accidental_roots();
    test_agrees_with_interval_rules();
    test_imaginary_root();
    test_rejects_invalid_arguments();
    printf("test_seventh_spelling_rules (standard-tonality): ok\n");
    return 0;
}
