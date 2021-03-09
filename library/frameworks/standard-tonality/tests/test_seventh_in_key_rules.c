#include <assert.h>
#include <stdio.h>

#include "framework.h"
#include "seventh_in_key_rules.h"
#include "spelling_test_util.h"

typedef StandardTonalitySeventhQuality SeventhQuality;

#define MAMA STANDARD_TONALITY_SEVENTH_MAJOR_MAJOR
#define MAMI STANDARD_TONALITY_SEVENTH_MAJOR_MINOR
#define MIMI STANDARD_TONALITY_SEVENTH_MINOR_MINOR
#define HDIM STANDARD_TONALITY_SEVENTH_HALF_DIMINISHED
#define FDIM STANDARD_TONALITY_SEVENTH_FULLY_DIMINISHED

#define MAJOR_KEY STANDARD_TONALITY_SEVENTH_KEY_MAJOR
#define MINOR_KEY STANDARD_TONALITY_SEVENTH_KEY_MINOR_NATURAL
#define RAISED STANDARD_TONALITY_SEVENTH_KEY_MINOR_RAISED_LEADING_TONE

static void test_rules_reference_framework(void) {
    for (int i = 0; i < STANDARD_TONALITY_SEVENTH_IN_KEY_RULE_COUNT; i++) {
        const Rule *rule = standard_tonality_seventh_in_key_rule((StandardTonalitySeventhInKeyRule)i);
        const Constraint *constraint = standard_tonality_seventh_in_key_constraint((StandardTonalitySeventhInKeyRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), standard_tonality_framework()));
        assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    }
    assert(standard_tonality_seventh_in_key_rule(STANDARD_TONALITY_SEVENTH_IN_KEY_RULE_COUNT) == NULL);
}

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

static void destroy4(PitchSpelling *notes[4]) {
    for (int i = 0; i < 4; i++) {
        pitch_spelling_destroy(notes[i]);
    }
}

static void check_note(const PitchSpelling *note, PitchLetter letter, int accidental) {
    assert(pitch_spelling_letter(note) == letter);
    assert(pitch_spelling_accidental(note) == accidental);
}

/* The G major scale's seventh chords: G B D F-sharp, A C E G, B D F-sharp A,
   C E G B, D F-sharp A C, E G B D, F-sharp A C E. */
static void test_g_major_chords(void) {
    PitchSpelling *g = spell(PITCH_LETTER_G, 0, 4);
    PitchLetter letters[7][4] = {
        {PITCH_LETTER_G, PITCH_LETTER_B, PITCH_LETTER_D, PITCH_LETTER_F},
        {PITCH_LETTER_A, PITCH_LETTER_C, PITCH_LETTER_E, PITCH_LETTER_G},
        {PITCH_LETTER_B, PITCH_LETTER_D, PITCH_LETTER_F, PITCH_LETTER_A},
        {PITCH_LETTER_C, PITCH_LETTER_E, PITCH_LETTER_G, PITCH_LETTER_B},
        {PITCH_LETTER_D, PITCH_LETTER_F, PITCH_LETTER_A, PITCH_LETTER_C},
        {PITCH_LETTER_E, PITCH_LETTER_G, PITCH_LETTER_B, PITCH_LETTER_D},
        {PITCH_LETTER_F, PITCH_LETTER_A, PITCH_LETTER_C, PITCH_LETTER_E}};
    int accidentals[7][4] = {{0, 0, 0, 1}, {0, 0, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 0},
                             {0, 1, 0, 0}, {0, 0, 0, 0}, {1, 0, 0, 0}};
    for (int degree = 1; degree <= 7; degree++) {
        PitchSpelling *notes[4];
        assert(standard_tonality_spell_diatonic_seventh(MAJOR_KEY, g, degree, notes));
        for (int i = 0; i < 4; i++) {
            check_note(notes[i], letters[degree - 1][i], accidentals[degree - 1][i]);
        }
        destroy4(notes);
    }
    pitch_spelling_destroy(g);
}

/* G minor with the raised leading tone: the dominant D F-sharp A C and the
   fully diminished F-sharp A C E-flat. */
static void test_g_minor_raised_leading_tone(void) {
    PitchSpelling *g = spell(PITCH_LETTER_G, 0, 4);
    PitchSpelling *notes[4];

    assert(standard_tonality_spell_diatonic_seventh(RAISED, g, 5, notes));
    check_note(notes[0], PITCH_LETTER_D, 0);
    check_note(notes[1], PITCH_LETTER_F, 1);
    check_note(notes[2], PITCH_LETTER_A, 0);
    check_note(notes[3], PITCH_LETTER_C, 0);
    destroy4(notes);

    assert(standard_tonality_spell_diatonic_seventh(RAISED, g, 7, notes));
    check_note(notes[0], PITCH_LETTER_F, 1);
    check_note(notes[1], PITCH_LETTER_A, 0);
    check_note(notes[2], PITCH_LETTER_C, 0);
    check_note(notes[3], PITCH_LETTER_E, -1);
    destroy4(notes);

    /* Natural minor: te is F, a dominant seventh chord F A C E-flat. */
    assert(standard_tonality_spell_diatonic_seventh(MINOR_KEY, g, 7, notes));
    check_note(notes[0], PITCH_LETTER_F, 0);
    check_note(notes[1], PITCH_LETTER_A, 0);
    check_note(notes[2], PITCH_LETTER_C, 0);
    check_note(notes[3], PITCH_LETTER_E, -1);
    destroy4(notes);

    pitch_spelling_destroy(g);
}

/* KEY_INDEPENDENT: in every key, the spelled chord's quality is the
   table's. */
static void test_spelled_chords_match_the_table(void) {
    PitchLetter letters[] = {PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F,
                             PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_B};
    int checked = 0;
    for (size_t l = 0; l < 7; l++) {
        for (int accidental = -1; accidental <= 1; accidental++) {
            for (int key = 0; key < STANDARD_TONALITY_SEVENTH_KEY_COUNT; key++) {
                PitchSpelling *tonic = spell(letters[l], accidental, 4);
                for (int degree = 1; degree <= 7; degree++) {
                    PitchSpelling *notes[4];
                    if (!standard_tonality_spell_diatonic_seventh((StandardTonalitySeventhKey)key, tonic, degree,
                                                                  notes)) {
                        continue; /* a scale or chord past a double accidental */
                    }
                    SeventhQuality read;
                    SeventhQuality table;
                    assert(standard_tonality_seventh_stack_quality(notes[0], notes[1], notes[2], notes[3], &read));
                    assert(standard_tonality_seventh_quality_in_key((StandardTonalitySeventhKey)key, degree, &table));
                    assert(read == table);
                    destroy4(notes);
                    checked++;
                }
                pitch_spelling_destroy(tonic);
            }
        }
    }
    assert(checked > 0);
}

static void test_spell_rejects_invalid_arguments(void) {
    PitchSpelling *g = spell(PITCH_LETTER_G, 0, 4);
    PitchSpelling *notes[4] = {NULL, NULL, NULL, NULL};
    assert(!standard_tonality_spell_diatonic_seventh((StandardTonalitySeventhKey)99, g, 1, notes));
    assert(!standard_tonality_spell_diatonic_seventh(MAJOR_KEY, NULL, 1, notes));
    assert(!standard_tonality_spell_diatonic_seventh(MAJOR_KEY, g, 0, notes));
    assert(!standard_tonality_spell_diatonic_seventh(MAJOR_KEY, g, 8, notes));
    assert(!standard_tonality_spell_diatonic_seventh(MAJOR_KEY, g, 1, NULL));
    for (int i = 0; i < 4; i++) {
        assert(notes[i] == NULL);
    }
    pitch_spelling_destroy(g);
}

int main(void) {
    test_rules_reference_framework();
    test_major_key();
    test_minor_key();
    test_raised_leading_tone();
    test_table_rejects_invalid_arguments();
    test_g_major_chords();
    test_g_minor_raised_leading_tone();
    test_spelled_chords_match_the_table();
    test_spell_rejects_invalid_arguments();
    printf("test_seventh_in_key_rules (standard-tonality): ok\n");
    return 0;
}
