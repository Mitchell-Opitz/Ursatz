#include <assert.h>
#include <stdio.h>

#include "framework.h"
#include "seventh_identification_rules.h"
#include "seventh_spelling_rules.h"
#include "spelling_test_util.h"

typedef StandardTonalitySeventhQuality SeventhQuality;

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

/* Identifies the notes, destroying the spellings it makes. */
static bool identify(const Note *notes, size_t count, PitchLetter *letter, int *accidental, SeventhQuality *quality) {
    PitchSpelling *spellings[16];
    for (size_t i = 0; i < count; i++) {
        spellings[i] = spell(notes[i].letter, notes[i].accidental, notes[i].octave);
    }
    bool result = standard_tonality_identify_seventh((const PitchSpelling *const *)spellings, count, letter,
                                                     accidental, quality);
    for (size_t i = 0; i < count; i++) {
        pitch_spelling_destroy(spellings[i]);
    }
    return result;
}

static void check_identified(const Note *notes, size_t count, PitchLetter expected_letter, int expected_accidental,
                             SeventhQuality expected_quality) {
    PitchLetter letter = PITCH_LETTER_A;
    int accidental = 42;
    SeventhQuality quality = (SeventhQuality)42;
    assert(identify(notes, count, &letter, &accidental, &quality));
    assert(letter == expected_letter);
    assert(accidental == expected_accidental);
    assert(quality == expected_quality);
}

static void test_rules_reference_framework(void) {
    for (int i = 0; i < STANDARD_TONALITY_SEVENTH_IDENTIFICATION_RULE_COUNT; i++) {
        const Rule *rule = standard_tonality_seventh_identification_rule((StandardTonalitySeventhIdentificationRule)i);
        const Constraint *constraint =
            standard_tonality_seventh_identification_constraint((StandardTonalitySeventhIdentificationRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), standard_tonality_framework()));
        assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    }
    assert(standard_tonality_seventh_identification_rule(STANDARD_TONALITY_SEVENTH_IDENTIFICATION_RULE_COUNT) == NULL);
}

/* Example 10: C-sharp E G B is C-sharp half-diminished. */
static void test_example_10(void) {
    Note notes[] = {{PITCH_LETTER_C, 1, 4}, {PITCH_LETTER_E, 0, 4}, {PITCH_LETTER_G, 0, 4}, {PITCH_LETTER_B, 0, 4}};
    check_identified(notes, 4, PITCH_LETTER_C, 1, HDIM);
}

static void test_every_quality_on_c(void) {
    Note major_major[] = {{PITCH_LETTER_C, 0, 4}, {PITCH_LETTER_E, 0, 4}, {PITCH_LETTER_G, 0, 4}, {PITCH_LETTER_B, 0, 4}};
    Note major_minor[] = {{PITCH_LETTER_C, 0, 4}, {PITCH_LETTER_E, 0, 4}, {PITCH_LETTER_G, 0, 4}, {PITCH_LETTER_B, -1, 4}};
    Note minor_minor[] = {{PITCH_LETTER_C, 0, 4}, {PITCH_LETTER_E, -1, 4}, {PITCH_LETTER_G, 0, 4}, {PITCH_LETTER_B, -1, 4}};
    Note half_diminished[] = {{PITCH_LETTER_C, 0, 4}, {PITCH_LETTER_E, -1, 4}, {PITCH_LETTER_G, -1, 4}, {PITCH_LETTER_B, -1, 4}};
    Note fully_diminished[] = {{PITCH_LETTER_C, 0, 4}, {PITCH_LETTER_E, -1, 4}, {PITCH_LETTER_G, -1, 4}, {PITCH_LETTER_B, -2, 4}};
    check_identified(major_major, 4, PITCH_LETTER_C, 0, MAMA);
    check_identified(major_minor, 4, PITCH_LETTER_C, 0, MAMI);
    check_identified(minor_minor, 4, PITCH_LETTER_C, 0, MIMI);
    check_identified(half_diminished, 4, PITCH_LETTER_C, 0, HDIM);
    check_identified(fully_diminished, 4, PITCH_LETTER_C, 0, FDIM);
}

/* OCTAVE_EQUIVALENCE, Example 11: doublings and open spacing do not change
   the identification, alone or together, in any order, and the chord may
   run across clefs. */
static void test_doubling_and_spacing(void) {
    /* G B D F with the root and fifth doubled. */
    Note doubled[] = {{PITCH_LETTER_G, 0, 3}, {PITCH_LETTER_G, 0, 4}, {PITCH_LETTER_B, 0, 4}, {PITCH_LETTER_D, 0, 5},
                      {PITCH_LETTER_F, 0, 5}, {PITCH_LETTER_D, 0, 4}};
    check_identified(doubled, 6, PITCH_LETTER_G, 0, MAMI);

    /* Open spacing, a bass-clef root and treble-clef upper notes. */
    Note open_spacing[] = {{PITCH_LETTER_G, 0, 2}, {PITCH_LETTER_D, 0, 4}, {PITCH_LETTER_F, 0, 5}, {PITCH_LETTER_B, 0, 5}};
    check_identified(open_spacing, 4, PITCH_LETTER_G, 0, MAMI);

    /* Out of order, the highest note first. */
    Note both[] = {{PITCH_LETTER_F, 0, 5}, {PITCH_LETTER_G, 0, 2}, {PITCH_LETTER_B, 0, 3}, {PITCH_LETTER_D, 0, 4},
                   {PITCH_LETTER_G, 0, 5}};
    check_identified(both, 5, PITCH_LETTER_G, 0, MAMI);

    /* Inverted order: the seventh in the bass still names the root. */
    Note seventh_in_bass[] = {{PITCH_LETTER_F, 0, 3}, {PITCH_LETTER_G, 0, 4}, {PITCH_LETTER_B, 0, 4}, {PITCH_LETTER_D, 0, 5}};
    check_identified(seventh_in_bass, 4, PITCH_LETTER_G, 0, MAMI);
}

/* Property: every quality on every root, and any octave placement of its
   notes, is identified as it was spelled. */
static void test_identifies_every_spelled_seventh(void) {
    PitchLetter letters[] = {PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F,
                             PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_B};
    int identified = 0;
    for (size_t l = 0; l < 7; l++) {
        for (int accidental = -1; accidental <= 1; accidental++) {
            for (int q = 0; q < STANDARD_TONALITY_SEVENTH_QUALITY_COUNT; q++) {
                PitchSpelling *root = spell(letters[l], accidental, 4);
                PitchSpelling *notes[4];
                if (!standard_tonality_spell_seventh(root, (SeventhQuality)q, notes)) {
                    pitch_spelling_destroy(root);
                    continue;
                }
                pitch_spelling_destroy(root);

                /* Spread the notes over three octaves, reversed. */
                PitchSpelling *spread[4];
                for (int i = 0; i < 4; i++) {
                    spread[3 - i] = spell(pitch_spelling_letter(notes[i]), pitch_spelling_accidental(notes[i]),
                                          pitch_spelling_octave(notes[i]) + (i % 3) - 1);
                }

                PitchLetter letter;
                int found_accidental;
                SeventhQuality quality;
                assert(standard_tonality_identify_seventh((const PitchSpelling *const *)notes, 4, &letter,
                                                          &found_accidental, &quality));
                assert(letter == letters[l] && found_accidental == accidental && quality == (SeventhQuality)q);
                assert(standard_tonality_identify_seventh((const PitchSpelling *const *)spread, 4, &letter,
                                                          &found_accidental, &quality));
                assert(letter == letters[l] && found_accidental == accidental && quality == (SeventhQuality)q);

                /* The written steps agree wherever the root has a key. */
                if (standard_tonality_identify_seventh_by_key_signature((const PitchSpelling *const *)notes, 4,
                                                                        &letter, &found_accidental, &quality)) {
                    assert(letter == letters[l] && found_accidental == accidental && quality == (SeventhQuality)q);
                }

                for (int i = 0; i < 4; i++) {
                    pitch_spelling_destroy(notes[i]);
                    pitch_spelling_destroy(spread[i]);
                }
                identified++;
            }
        }
    }
    assert(identified > 0);
}

/* A root with an imaginary key signature is still identified: G
   double-sharp, B-sharp, D-sharp, F-sharp is fully diminished, though G
   double-sharp has no major scale. The written steps give no answer. */
static void test_imaginary_root(void) {
    Note notes[] = {{PITCH_LETTER_G, 2, 4}, {PITCH_LETTER_B, 1, 4}, {PITCH_LETTER_D, 1, 5}, {PITCH_LETTER_F, 1, 5}};
    check_identified(notes, 4, PITCH_LETTER_G, 2, FDIM);

    PitchSpelling *spellings[4] = {spell(PITCH_LETTER_G, 2, 4), spell(PITCH_LETTER_B, 1, 4),
                                   spell(PITCH_LETTER_D, 1, 5), spell(PITCH_LETTER_F, 1, 5)};
    PitchLetter letter = PITCH_LETTER_A;
    int accidental = 42;
    SeventhQuality quality = (SeventhQuality)42;
    assert(!standard_tonality_identify_seventh_by_key_signature((const PitchSpelling *const *)spellings, 4, &letter,
                                                                &accidental, &quality));
    assert(letter == PITCH_LETTER_A && accidental == 42 && quality == (SeventhQuality)42);
    for (size_t i = 0; i < 4; i++) {
        pitch_spelling_destroy(spellings[i]);
    }
}

static void test_rejects_non_sevenths(void) {
    PitchLetter letter = PITCH_LETTER_A;
    int accidental = 42;
    SeventhQuality quality = (SeventhQuality)42;

    /* A triad, and a triad with a doubled note: only three names. */
    Note triad[] = {{PITCH_LETTER_C, 0, 4}, {PITCH_LETTER_E, 0, 4}, {PITCH_LETTER_G, 0, 4}};
    Note doubled_triad[] = {{PITCH_LETTER_C, 0, 4}, {PITCH_LETTER_E, 0, 4}, {PITCH_LETTER_G, 0, 4}, {PITCH_LETTER_C, 0, 5}};
    assert(!identify(triad, 3, &letter, &accidental, &quality));
    assert(!identify(doubled_triad, 4, &letter, &accidental, &quality));

    /* A ninth chord (five names), and four names that do not stack. */
    Note ninth[] = {{PITCH_LETTER_C, 0, 4}, {PITCH_LETTER_E, 0, 4}, {PITCH_LETTER_G, 0, 4}, {PITCH_LETTER_B, 0, 4},
                    {PITCH_LETTER_D, 0, 5}};
    Note not_thirds[] = {{PITCH_LETTER_C, 0, 4}, {PITCH_LETTER_D, 0, 4}, {PITCH_LETTER_E, 0, 4}, {PITCH_LETTER_F, 0, 4}};
    assert(!identify(ninth, 5, &letter, &accidental, &quality));
    assert(!identify(not_thirds, 4, &letter, &accidental, &quality));

    /* A minor triad with a major seventh is stacked but not one of the five;
       the same note name twice with different accidentals is five names. */
    Note minor_major[] = {{PITCH_LETTER_C, 0, 4}, {PITCH_LETTER_E, -1, 4}, {PITCH_LETTER_G, 0, 4}, {PITCH_LETTER_B, 0, 4}};
    Note two_accidentals[] = {{PITCH_LETTER_C, 0, 4}, {PITCH_LETTER_E, 0, 4}, {PITCH_LETTER_E, -1, 4}, {PITCH_LETTER_G, 0, 4},
                              {PITCH_LETTER_B, 0, 4}};
    assert(!identify(minor_major, 4, &letter, &accidental, &quality));
    assert(!identify(two_accidentals, 5, &letter, &accidental, &quality));

    assert(letter == PITCH_LETTER_A && accidental == 42 && quality == (SeventhQuality)42);
    assert(!standard_tonality_identify_seventh(NULL, 4, &letter, &accidental, &quality));
    PitchSpelling *c4 = spell(PITCH_LETTER_C, 0, 4);
    const PitchSpelling *with_null[] = {c4, NULL, c4, c4};
    assert(!standard_tonality_identify_seventh(with_null, 4, &letter, &accidental, &quality));
    assert(!standard_tonality_identify_seventh(with_null, 4, NULL, &accidental, &quality));
    pitch_spelling_destroy(c4);
}

static void check_closed(const PitchSpelling *spelling, PitchLetter letter, int accidental, int octave) {
    assert(pitch_spelling_letter(spelling) == letter);
    assert(pitch_spelling_accidental(spelling) == accidental);
    assert(pitch_spelling_octave(spelling) == octave);
}

/* CLOSED_POSITION: doubled and widely spaced, written closed with no
   doublings, root first, placed in the root's lowest octave. */
static void test_closed_position(void) {
    PitchSpelling *g2 = spell(PITCH_LETTER_G, 0, 2);
    PitchSpelling *d4 = spell(PITCH_LETTER_D, 0, 4);
    PitchSpelling *f5 = spell(PITCH_LETTER_F, 0, 5);
    PitchSpelling *b5 = spell(PITCH_LETTER_B, 0, 5);
    PitchSpelling *g4 = spell(PITCH_LETTER_G, 0, 4);
    const PitchSpelling *notes[] = {f5, b5, g4, d4, g2};

    PitchSpelling *closed[4];
    assert(standard_tonality_seventh_closed_position(notes, 5, closed));
    check_closed(closed[0], PITCH_LETTER_G, 0, 2);
    check_closed(closed[1], PITCH_LETTER_B, 0, 2);
    check_closed(closed[2], PITCH_LETTER_D, 0, 3);
    check_closed(closed[3], PITCH_LETTER_F, 0, 3);
    for (int i = 0; i < 4; i++) {
        pitch_spelling_destroy(closed[i]);
    }
    pitch_spelling_destroy(g2);
    pitch_spelling_destroy(d4);
    pitch_spelling_destroy(f5);
    pitch_spelling_destroy(b5);
    pitch_spelling_destroy(g4);
}

/* The octave number follows the letter: a seventh chord on B wraps to the
   next octave at C. */
static void test_closed_position_wraps_the_octave(void) {
    PitchSpelling *b3 = spell(PITCH_LETTER_B, 0, 3);
    PitchSpelling *d5 = spell(PITCH_LETTER_D, 1, 5);
    PitchSpelling *f6 = spell(PITCH_LETTER_F, 1, 6);
    PitchSpelling *a4 = spell(PITCH_LETTER_A, 0, 4);
    const PitchSpelling *notes[] = {a4, f6, b3, d5};

    PitchSpelling *closed[4];
    assert(standard_tonality_seventh_closed_position(notes, 4, closed));
    check_closed(closed[0], PITCH_LETTER_B, 0, 3);
    check_closed(closed[1], PITCH_LETTER_D, 1, 4);
    check_closed(closed[2], PITCH_LETTER_F, 1, 4);
    check_closed(closed[3], PITCH_LETTER_A, 0, 4);
    for (int i = 0; i < 4; i++) {
        pitch_spelling_destroy(closed[i]);
    }
    pitch_spelling_destroy(b3);
    pitch_spelling_destroy(d5);
    pitch_spelling_destroy(f6);
    pitch_spelling_destroy(a4);
}

static void test_closed_position_rejects_non_sevenths(void) {
    PitchSpelling *c4 = spell(PITCH_LETTER_C, 0, 4);
    PitchSpelling *e4 = spell(PITCH_LETTER_E, 0, 4);
    PitchSpelling *g4 = spell(PITCH_LETTER_G, 0, 4);
    const PitchSpelling *triad[] = {c4, e4, g4};
    PitchSpelling *closed[4] = {NULL, NULL, NULL, NULL};
    assert(!standard_tonality_seventh_closed_position(triad, 3, closed));
    assert(!standard_tonality_seventh_closed_position(NULL, 4, closed));
    assert(!standard_tonality_seventh_closed_position(triad, 3, NULL));
    for (int i = 0; i < 4; i++) {
        assert(closed[i] == NULL);
    }
    pitch_spelling_destroy(c4);
    pitch_spelling_destroy(e4);
    pitch_spelling_destroy(g4);
}

int main(void) {
    test_rules_reference_framework();
    test_example_10();
    test_every_quality_on_c();
    test_doubling_and_spacing();
    test_identifies_every_spelled_seventh();
    test_imaginary_root();
    test_rejects_non_sevenths();
    test_closed_position();
    test_closed_position_wraps_the_octave();
    test_closed_position_rejects_non_sevenths();
    printf("test_seventh_identification_rules (standard-tonality): ok\n");
    return 0;
}
