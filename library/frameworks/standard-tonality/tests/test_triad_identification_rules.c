#include <assert.h>
#include <stdio.h>

#include "framework.h"
#include "spelling_test_util.h"
#include "triad_identification_rules.h"
#include "triad_spelling_rules.h"

typedef StandardTonalityTriadQuality TriadQuality;

#define MAJOR STANDARD_TONALITY_TRIAD_MAJOR
#define MINOR STANDARD_TONALITY_TRIAD_MINOR
#define DIMINISHED STANDARD_TONALITY_TRIAD_DIMINISHED
#define AUGMENTED STANDARD_TONALITY_TRIAD_AUGMENTED

typedef struct {
    PitchLetter letter;
    int accidental;
    int octave;
} Note;

/* Identifies the notes, destroying the spellings it makes. */
static bool identify(const Note *notes, size_t count, PitchLetter *letter, int *accidental, TriadQuality *quality) {
    PitchSpelling *spellings[16];
    for (size_t i = 0; i < count; i++) {
        spellings[i] = spell(notes[i].letter, notes[i].accidental, notes[i].octave);
    }
    bool result = standard_tonality_identify_triad((const PitchSpelling *const *)spellings, count, letter, accidental,
                                                   quality);
    for (size_t i = 0; i < count; i++) {
        pitch_spelling_destroy(spellings[i]);
    }
    return result;
}

static void check_identified(const Note *notes, size_t count, PitchLetter expected_letter, int expected_accidental,
                             TriadQuality expected_quality) {
    PitchLetter letter = PITCH_LETTER_A;
    int accidental = 42;
    TriadQuality quality = (TriadQuality)42;
    assert(identify(notes, count, &letter, &accidental, &quality));
    assert(letter == expected_letter);
    assert(accidental == expected_accidental);
    assert(quality == expected_quality);
}

static void test_rules_reference_framework(void) {
    for (int i = 0; i < STANDARD_TONALITY_TRIAD_IDENTIFICATION_RULE_COUNT; i++) {
        const Rule *rule = standard_tonality_triad_identification_rule((StandardTonalityTriadIdentificationRule)i);
        const Constraint *constraint =
            standard_tonality_triad_identification_constraint((StandardTonalityTriadIdentificationRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), standard_tonality_framework()));
        assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    }
    assert(standard_tonality_triad_identification_rule(STANDARD_TONALITY_TRIAD_IDENTIFICATION_RULE_COUNT) == NULL);
}

/* Example 10: D F-sharp A is D major. */
static void test_example_10(void) {
    Note notes[] = {{PITCH_LETTER_D, 0, 4}, {PITCH_LETTER_F, 1, 4}, {PITCH_LETTER_A, 0, 4}};
    check_identified(notes, 3, PITCH_LETTER_D, 0, MAJOR);
}

/* Example 11: C-sharp E G, both lowered from C-sharp major's, is C-sharp
   diminished. */
static void test_example_11(void) {
    Note notes[] = {{PITCH_LETTER_C, 1, 4}, {PITCH_LETTER_E, 0, 4}, {PITCH_LETTER_G, 0, 4}};
    check_identified(notes, 3, PITCH_LETTER_C, 1, DIMINISHED);
}

static void test_every_quality_on_c(void) {
    Note major[] = {{PITCH_LETTER_C, 0, 4}, {PITCH_LETTER_E, 0, 4}, {PITCH_LETTER_G, 0, 4}};
    Note minor[] = {{PITCH_LETTER_C, 0, 4}, {PITCH_LETTER_E, -1, 4}, {PITCH_LETTER_G, 0, 4}};
    Note diminished[] = {{PITCH_LETTER_C, 0, 4}, {PITCH_LETTER_E, -1, 4}, {PITCH_LETTER_G, -1, 4}};
    Note augmented[] = {{PITCH_LETTER_C, 0, 4}, {PITCH_LETTER_E, 0, 4}, {PITCH_LETTER_G, 1, 4}};
    check_identified(major, 3, PITCH_LETTER_C, 0, MAJOR);
    check_identified(minor, 3, PITCH_LETTER_C, 0, MINOR);
    check_identified(diminished, 3, PITCH_LETTER_C, 0, DIMINISHED);
    check_identified(augmented, 3, PITCH_LETTER_C, 0, AUGMENTED);
}

/* OCTAVE_EQUIVALENCE, Example 12: doublings and open spacing do not change
   the identification, alone or together, in any order. */
static void test_doubling_and_spacing(void) {
    /* Octave doublings. */
    Note doubled[] = {{PITCH_LETTER_C, 0, 3}, {PITCH_LETTER_C, 0, 4}, {PITCH_LETTER_E, 0, 4},
                      {PITCH_LETTER_G, 0, 4}, {PITCH_LETTER_C, 0, 5}};
    check_identified(doubled, 5, PITCH_LETTER_C, 0, MAJOR);

    /* Open spacing: wide intervals between the notes. */
    Note open_spacing[] = {{PITCH_LETTER_C, 0, 3}, {PITCH_LETTER_G, 0, 4}, {PITCH_LETTER_E, 0, 6}};
    check_identified(open_spacing, 3, PITCH_LETTER_C, 0, MAJOR);

    /* Both, and out of order (the highest note first). */
    Note both[] = {{PITCH_LETTER_E, 0, 6}, {PITCH_LETTER_C, 0, 2}, {PITCH_LETTER_G, 0, 3}, {PITCH_LETTER_E, 0, 4},
                   {PITCH_LETTER_C, 0, 5}};
    check_identified(both, 5, PITCH_LETTER_C, 0, MAJOR);

    /* The same for a minor triad with an inverted order. */
    Note minor_inverted[] = {{PITCH_LETTER_G, 0, 3}, {PITCH_LETTER_E, -1, 4}, {PITCH_LETTER_C, 0, 5}};
    check_identified(minor_inverted, 3, PITCH_LETTER_C, 0, MINOR);
}

/* Property: every quality on every root, and any octave placement of its
   notes, is identified as it was spelled. */
static void test_identifies_every_spelled_triad(void) {
    PitchLetter letters[] = {PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F,
                             PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_B};
    int identified = 0;
    for (size_t l = 0; l < 7; l++) {
        for (int accidental = -2; accidental <= 2; accidental++) {
            PitchSpelling *root = spell(letters[l], accidental, 4);
            for (int q = 0; q < STANDARD_TONALITY_TRIAD_QUALITY_COUNT; q++) {
                PitchSpelling *triad[3];
                if (!standard_tonality_spell_triad(root, (TriadQuality)q, triad)) {
                    continue;
                }

                PitchLetter letter;
                int found_accidental;
                TriadQuality quality;
                assert(standard_tonality_identify_triad((const PitchSpelling *const *)triad, 3, &letter,
                                                        &found_accidental, &quality));
                assert(letter == letters[l] && found_accidental == accidental && quality == (TriadQuality)q);

                /* Move each note up a couple of octaves and shuffle. */
                PitchSpelling *moved[3];
                moved[0] = spell(pitch_spelling_letter(triad[2]), pitch_spelling_accidental(triad[2]),
                                 pitch_spelling_octave(triad[2]) + 2);
                moved[1] = spell(pitch_spelling_letter(triad[0]), pitch_spelling_accidental(triad[0]),
                                 pitch_spelling_octave(triad[0]));
                moved[2] = spell(pitch_spelling_letter(triad[1]), pitch_spelling_accidental(triad[1]),
                                 pitch_spelling_octave(triad[1]) + 1);
                assert(standard_tonality_identify_triad((const PitchSpelling *const *)moved, 3, &letter,
                                                        &found_accidental, &quality));
                assert(letter == letters[l] && found_accidental == accidental && quality == (TriadQuality)q);

                /* The written steps agree wherever the root's key exists. */
                PitchLetter key_letter;
                int key_accidental;
                TriadQuality key_quality;
                if (standard_tonality_identify_triad_by_key_signature((const PitchSpelling *const *)triad, 3,
                                                                      &key_letter, &key_accidental, &key_quality)) {
                    assert(key_letter == letter && key_accidental == found_accidental && key_quality == quality);
                }

                for (size_t i = 0; i < 3; i++) {
                    pitch_spelling_destroy(triad[i]);
                    pitch_spelling_destroy(moved[i]);
                }
                identified++;
            }
            pitch_spelling_destroy(root);
        }
    }
    assert(identified > 100);
}

/* A root with an imaginary key signature is still identified: G
   double-sharp, B double-sharp, D double-sharp is a major triad, though
   G double-sharp has no major scale. The written steps give no answer. */
static void test_imaginary_root(void) {
    Note notes[] = {{PITCH_LETTER_G, 2, 4}, {PITCH_LETTER_B, 2, 4}, {PITCH_LETTER_D, 2, 5}};
    check_identified(notes, 3, PITCH_LETTER_G, 2, MAJOR);

    PitchSpelling *spellings[3] = {spell(PITCH_LETTER_G, 2, 4), spell(PITCH_LETTER_B, 2, 4), spell(PITCH_LETTER_D, 2, 5)};
    PitchLetter letter = PITCH_LETTER_A;
    int accidental = 42;
    TriadQuality quality = (TriadQuality)42;
    assert(!standard_tonality_identify_triad_by_key_signature((const PitchSpelling *const *)spellings, 3, &letter,
                                                              &accidental, &quality));
    assert(letter == PITCH_LETTER_A && accidental == 42);
    for (size_t i = 0; i < 3; i++) {
        pitch_spelling_destroy(spellings[i]);
    }

    /* E double-flat G-flat B double-flat: the source's imaginary-key root. */
    Note e_double_flat[] = {{PITCH_LETTER_E, -2, 4}, {PITCH_LETTER_G, -1, 4}, {PITCH_LETTER_B, -2, 4}};
    check_identified(e_double_flat, 3, PITCH_LETTER_E, -2, MAJOR);
}

static void test_rejects_non_triads(void) {
    PitchLetter letter = PITCH_LETTER_D;
    int accidental = 42;
    TriadQuality quality = (TriadQuality)42;

    Note two[] = {{PITCH_LETTER_C, 0, 4}, {PITCH_LETTER_E, 0, 4}};
    Note one_name[] = {{PITCH_LETTER_C, 0, 3}, {PITCH_LETTER_C, 0, 4}, {PITCH_LETTER_C, 0, 5}};
    Note four_names[] = {{PITCH_LETTER_C, 0, 4}, {PITCH_LETTER_E, 0, 4}, {PITCH_LETTER_G, 0, 4},
                         {PITCH_LETTER_B, 0, 4}};
    Note not_thirds[] = {{PITCH_LETTER_C, 0, 4}, {PITCH_LETTER_D, 0, 4}, {PITCH_LETTER_E, 0, 4}};
    Note no_quality[] = {{PITCH_LETTER_C, 0, 4}, {PITCH_LETTER_E, 0, 4}, {PITCH_LETTER_G, -1, 4}}; /* ma3 d5 */
    Note enharmonic[] = {{PITCH_LETTER_C, 1, 4}, {PITCH_LETTER_D, -1, 4}, {PITCH_LETTER_G, 0, 4}};

    assert(!identify(two, 2, &letter, &accidental, &quality));
    assert(!identify(one_name, 3, &letter, &accidental, &quality));
    assert(!identify(four_names, 4, &letter, &accidental, &quality));
    assert(!identify(not_thirds, 3, &letter, &accidental, &quality));
    assert(!identify(no_quality, 3, &letter, &accidental, &quality));
    assert(!identify(enharmonic, 3, &letter, &accidental, &quality));
    assert(letter == PITCH_LETTER_D && accidental == 42 && quality == (TriadQuality)42);

    PitchSpelling *c4 = spell(PITCH_LETTER_C, 0, 4);
    const PitchSpelling *with_null[3] = {c4, NULL, c4};
    assert(!standard_tonality_identify_triad(NULL, 3, &letter, &accidental, &quality));
    assert(!standard_tonality_identify_triad(with_null, 3, &letter, &accidental, &quality));
    assert(!standard_tonality_identify_triad(with_null, 0, &letter, &accidental, &quality));
    assert(!standard_tonality_identify_triad(with_null, 3, NULL, &accidental, &quality));
    assert(!standard_tonality_identify_triad(with_null, 3, &letter, NULL, &quality));
    assert(!standard_tonality_identify_triad(with_null, 3, &letter, &accidental, NULL));
    pitch_spelling_destroy(c4);
}

/* CLOSED_POSITION: stack without doublings, root first, the root in its
   lowest octave. */
static void test_closed_position(void) {
    PitchSpelling *spellings[] = {spell(PITCH_LETTER_F, 1, 3), spell(PITCH_LETTER_D, 0, 5), spell(PITCH_LETTER_A, 0, 4),
                                  spell(PITCH_LETTER_D, 0, 3)};
    PitchSpelling *closed[3];
    assert(standard_tonality_triad_closed_position((const PitchSpelling *const *)spellings, 4, closed));
    assert(pitch_spelling_letter(closed[0]) == PITCH_LETTER_D && pitch_spelling_octave(closed[0]) == 3);
    assert(pitch_spelling_letter(closed[1]) == PITCH_LETTER_F && pitch_spelling_accidental(closed[1]) == 1 &&
           pitch_spelling_octave(closed[1]) == 3);
    assert(pitch_spelling_letter(closed[2]) == PITCH_LETTER_A && pitch_spelling_octave(closed[2]) == 3);

    /* It is a stack in thirds of the same quality. */
    TriadQuality quality = (TriadQuality)42;
    assert(standard_tonality_triad_stack_quality(closed[0], closed[1], closed[2], &quality));
    assert(quality == MAJOR);
    for (size_t i = 0; i < 3; i++) {
        pitch_spelling_destroy(closed[i]);
    }
    for (size_t i = 0; i < 4; i++) {
        pitch_spelling_destroy(spellings[i]);
    }
}

static void test_closed_position_wraps_the_octave(void) {
    PitchSpelling *spellings[] = {spell(PITCH_LETTER_B, 0, 3), spell(PITCH_LETTER_D, 1, 5), spell(PITCH_LETTER_F, 1, 4)};
    PitchSpelling *closed[3];
    assert(standard_tonality_triad_closed_position((const PitchSpelling *const *)spellings, 3, closed));
    assert(pitch_spelling_octave(closed[0]) == 3);
    assert(pitch_spelling_octave(closed[1]) == 4); /* D-sharp is in the next octave number */
    assert(pitch_spelling_octave(closed[2]) == 4);
    for (size_t i = 0; i < 3; i++) {
        pitch_spelling_destroy(closed[i]);
        pitch_spelling_destroy(spellings[i]);
    }
}

static void test_closed_position_rejects_non_triads(void) {
    PitchSpelling *c4 = spell(PITCH_LETTER_C, 0, 4);
    PitchSpelling *d4 = spell(PITCH_LETTER_D, 0, 4);
    PitchSpelling *e4 = spell(PITCH_LETTER_E, 0, 4);
    const PitchSpelling *not_thirds[] = {c4, d4, e4};
    PitchSpelling *closed[3] = {NULL, NULL, NULL};
    assert(!standard_tonality_triad_closed_position(not_thirds, 3, closed));
    assert(!standard_tonality_triad_closed_position(not_thirds, 3, NULL));
    assert(closed[0] == NULL);
    pitch_spelling_destroy(c4);
    pitch_spelling_destroy(d4);
    pitch_spelling_destroy(e4);
}

static void test_lowest_note(void) {
    PitchSpelling *e5 = spell(PITCH_LETTER_E, -1, 5);
    PitchSpelling *c4 = spell(PITCH_LETTER_C, 0, 4);
    PitchSpelling *g4 = spell(PITCH_LETTER_G, 0, 4);
    const PitchSpelling *notes[] = {e5, c4, g4};
    PitchLetter letter = PITCH_LETTER_A;
    int accidental = 42;
    assert(standard_tonality_lowest_note(notes, 3, &letter, &accidental));
    assert(letter == PITCH_LETTER_C && accidental == 0);

    const PitchSpelling *with_null[] = {c4, NULL};
    assert(!standard_tonality_lowest_note(with_null, 2, &letter, &accidental));
    assert(!standard_tonality_lowest_note(NULL, 2, &letter, &accidental));
    assert(!standard_tonality_lowest_note(notes, 0, &letter, &accidental));
    assert(!standard_tonality_lowest_note(notes, 3, NULL, &accidental));
    assert(!standard_tonality_lowest_note(notes, 3, &letter, NULL));
    pitch_spelling_destroy(e5);
    pitch_spelling_destroy(c4);
    pitch_spelling_destroy(g4);
}

int main(void) {
    test_rules_reference_framework();
    test_example_10();
    test_example_11();
    test_every_quality_on_c();
    test_doubling_and_spacing();
    test_identifies_every_spelled_triad();
    test_imaginary_root();
    test_rejects_non_triads();
    test_closed_position();
    test_closed_position_wraps_the_octave();
    test_closed_position_rejects_non_triads();
    test_lowest_note();
    printf("test_triad_identification_rules (standard-tonality): ok\n");
    return 0;
}
