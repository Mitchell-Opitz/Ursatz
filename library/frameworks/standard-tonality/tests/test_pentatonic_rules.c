#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "framework.h"
#include "pentatonic_rules.h"
#include "spelling_test_util.h"

#define NOTES STANDARD_TONALITY_PENTATONIC_NOTE_COUNT

static void destroy_collection(PitchSpelling *notes[], size_t count) {
    for (size_t i = 0; i < count; i++) {
        pitch_spelling_destroy(notes[i]);
    }
}

static void check_collection(StandardTonalityPentatonicForm form, PitchLetter tonic_letter, int tonic_accidental,
                             const PitchLetter letters[NOTES], const int accidentals[NOTES]) {
    PitchSpelling *tonic = spell(tonic_letter, tonic_accidental, 4);
    PitchSpelling *notes[NOTES];
    assert(standard_tonality_build_pentatonic(form, tonic, notes));
    for (size_t i = 0; i < NOTES; i++) {
        assert(pitch_spelling_letter(notes[i]) == letters[i]);
        assert(pitch_spelling_accidental(notes[i]) == accidentals[i]);
    }
    assert(pitch_spelling_equals(notes[0], tonic));
    assert(pitch_spelling_octave(notes[5]) == pitch_spelling_octave(notes[0]) + 1);
    assert(standard_tonality_is_pentatonic(form, (const PitchSpelling *const *)notes, NOTES));
    destroy_collection(notes, NOTES);
    pitch_spelling_destroy(tonic);
}

static void test_rules_reference_framework(void) {
    for (int i = 0; i < STANDARD_TONALITY_PENTATONIC_RULE_COUNT; i++) {
        const Rule *rule = standard_tonality_pentatonic_rule((StandardTonalityPentatonicRule)i);
        const Constraint *constraint = standard_tonality_pentatonic_constraint((StandardTonalityPentatonicRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), standard_tonality_framework()));
        assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    }
    assert(standard_tonality_pentatonic_rule(STANDARD_TONALITY_PENTATONIC_RULE_COUNT) == NULL);
}

/* INTERVALS: major 2nd, major 2nd, minor 3rd, major 2nd, minor 3rd. */
static void test_intervals(void) {
    int expected[] = {2, 2, 3, 2, 3};
    int total = 0;
    for (size_t i = 0; i < 5; i++) {
        assert(standard_tonality_pentatonic_step(STANDARD_TONALITY_PENTATONIC_MAJOR, i) == expected[i]);
        total += expected[i];
    }
    assert(total == 12);
}

/* MINOR_ROTATION: the minor steps are the major steps read from la. */
static void test_minor_is_a_rotation_of_major(void) {
    for (size_t i = 0; i < 5; i++) {
        assert(standard_tonality_pentatonic_step(STANDARD_TONALITY_PENTATONIC_MINOR, i) ==
               standard_tonality_pentatonic_step(STANDARD_TONALITY_PENTATONIC_MAJOR, (i + 4) % 5));
    }
    int expected[] = {3, 2, 2, 3, 2};
    for (size_t i = 0; i < 5; i++) {
        assert(standard_tonality_pentatonic_step(STANDARD_TONALITY_PENTATONIC_MINOR, i) == expected[i]);
    }
}

/* DEGREES: the major scale with 4 and 7 omitted: 1 2 3 5 6. */
static void test_degrees(void) {
    int major[] = {1, 2, 3, 5, 6, 8};
    int minor[] = {1, 3, 4, 5, 7, 8};
    for (size_t i = 0; i < 6; i++) {
        assert(standard_tonality_pentatonic_degree(STANDARD_TONALITY_PENTATONIC_MAJOR, i) == major[i]);
        assert(standard_tonality_pentatonic_degree(STANDARD_TONALITY_PENTATONIC_MINOR, i) == minor[i]);
    }
    /* Omitted from the major collection: 4 and 7. */
    for (size_t i = 0; i < 6; i++) {
        int degree = standard_tonality_pentatonic_degree(STANDARD_TONALITY_PENTATONIC_MAJOR, i);
        assert(degree != 4 && degree != 7);
    }
}

/* SOLFEGE: do re mi sol la do. */
static void test_solfege(void) {
    const char *major[] = {"do", "re", "mi", "sol", "la", "do"};
    const char *minor[] = {"do", "me", "fa", "sol", "te", "do"};
    for (size_t i = 0; i < 6; i++) {
        assert(strcmp(standard_tonality_pentatonic_solfege(STANDARD_TONALITY_PENTATONIC_MAJOR, i), major[i]) == 0);
        assert(strcmp(standard_tonality_pentatonic_solfege(STANDARD_TONALITY_PENTATONIC_MINOR, i), minor[i]) == 0);
    }
    assert(standard_tonality_pentatonic_solfege(STANDARD_TONALITY_PENTATONIC_MAJOR, 6) == NULL);
    assert(standard_tonality_pentatonic_solfege((StandardTonalityPentatonicForm)99, 0) == NULL);
}

/* SPELLING: the source's B-flat major pentatonic is B-flat C D F G B-flat. */
static void test_b_flat_major_pentatonic(void) {
    PitchLetter letters[NOTES] = {PITCH_LETTER_B, PITCH_LETTER_C, PITCH_LETTER_D,
                                  PITCH_LETTER_F, PITCH_LETTER_G, PITCH_LETTER_B};
    int accidentals[NOTES] = {-1, 0, 0, 0, 0, -1};
    check_collection(STANDARD_TONALITY_PENTATONIC_MAJOR, PITCH_LETTER_B, -1, letters, accidentals);
}

/* Example 10b: a D pentatonic collection. */
static void test_d_and_c_major_pentatonic(void) {
    PitchLetter d_letters[NOTES] = {PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F,
                                    PITCH_LETTER_A, PITCH_LETTER_B, PITCH_LETTER_D};
    int d_accidentals[NOTES] = {0, 0, 1, 0, 0, 0}; /* F-sharp */
    check_collection(STANDARD_TONALITY_PENTATONIC_MAJOR, PITCH_LETTER_D, 0, d_letters, d_accidentals);

    PitchLetter c_letters[NOTES] = {PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_E,
                                    PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_C};
    int c_accidentals[NOTES] = {0, 0, 0, 0, 0, 0};
    check_collection(STANDARD_TONALITY_PENTATONIC_MAJOR, PITCH_LETTER_C, 0, c_letters, c_accidentals);
}

static void test_minor_pentatonic(void) {
    PitchLetter a_letters[NOTES] = {PITCH_LETTER_A, PITCH_LETTER_C, PITCH_LETTER_D,
                                    PITCH_LETTER_E, PITCH_LETTER_G, PITCH_LETTER_A};
    int a_accidentals[NOTES] = {0, 0, 0, 0, 0, 0};
    check_collection(STANDARD_TONALITY_PENTATONIC_MINOR, PITCH_LETTER_A, 0, a_letters, a_accidentals);

    PitchLetter e_flat_letters[NOTES] = {PITCH_LETTER_E, PITCH_LETTER_G, PITCH_LETTER_A,
                                         PITCH_LETTER_B, PITCH_LETTER_D, PITCH_LETTER_E};
    int e_flat_accidentals[NOTES] = {-1, -1, -1, -1, -1, -1};
    check_collection(STANDARD_TONALITY_PENTATONIC_MINOR, PITCH_LETTER_E, -1, e_flat_letters, e_flat_accidentals);
}

/* MINOR_ROTATION: A minor pentatonic is C major pentatonic begun on A. */
static void test_a_minor_is_c_major_from_la(void) {
    PitchSpelling *c = spell(PITCH_LETTER_C, 0, 4);
    PitchSpelling *a = spell(PITCH_LETTER_A, 0, 4);
    PitchSpelling *major[NOTES];
    PitchSpelling *minor[NOTES];
    assert(standard_tonality_build_pentatonic(STANDARD_TONALITY_PENTATONIC_MAJOR, c, major));
    assert(standard_tonality_build_pentatonic(STANDARD_TONALITY_PENTATONIC_MINOR, a, minor));
    for (size_t i = 0; i < 5; i++) {
        const PitchSpelling *m = major[(i + 4) % 5];
        assert(pitch_spelling_letter(minor[i]) == pitch_spelling_letter(m));
        assert(pitch_spelling_accidental(minor[i]) == pitch_spelling_accidental(m));
    }
    destroy_collection(major, NOTES);
    destroy_collection(minor, NOTES);
    pitch_spelling_destroy(c);
    pitch_spelling_destroy(a);
}

/* BLACK_KEYS: F-sharp (or G-flat) major pentatonic is exactly the black
   keys; so is E-flat minor. C major is not. */
static void test_black_keys(void) {
    PitchSpelling *tonics[] = {spell(PITCH_LETTER_F, 1, 4), spell(PITCH_LETTER_G, -1, 4)};
    for (size_t t = 0; t < 2; t++) {
        PitchSpelling *notes[NOTES];
        assert(standard_tonality_build_pentatonic(STANDARD_TONALITY_PENTATONIC_MAJOR, tonics[t], notes));
        assert(standard_tonality_all_black_keys((const PitchSpelling *const *)notes, NOTES));
        destroy_collection(notes, NOTES);
        pitch_spelling_destroy(tonics[t]);
    }

    PitchSpelling *e_flat = spell(PITCH_LETTER_E, -1, 4);
    PitchSpelling *notes[NOTES];
    assert(standard_tonality_build_pentatonic(STANDARD_TONALITY_PENTATONIC_MINOR, e_flat, notes));
    assert(standard_tonality_all_black_keys((const PitchSpelling *const *)notes, NOTES));
    destroy_collection(notes, NOTES);
    pitch_spelling_destroy(e_flat);

    PitchSpelling *c = spell(PITCH_LETTER_C, 0, 4);
    assert(standard_tonality_build_pentatonic(STANDARD_TONALITY_PENTATONIC_MAJOR, c, notes));
    assert(!standard_tonality_all_black_keys((const PitchSpelling *const *)notes, NOTES));
    destroy_collection(notes, NOTES);
    pitch_spelling_destroy(c);

    assert(!standard_tonality_all_black_keys(NULL, NOTES));
    assert(!standard_tonality_all_black_keys((const PitchSpelling *const *)notes, 0));
}

/* Property: any note can begin a major pentatonic collection. */
static void test_built_collections_follow_the_pattern(void) {
    PitchLetter letters[] = {PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F,
                             PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_B};
    for (int form = 0; form < STANDARD_TONALITY_PENTATONIC_FORM_COUNT; form++) {
        for (size_t l = 0; l < 7; l++) {
            for (int accidental = -1; accidental <= 1; accidental++) {
                PitchSpelling *tonic = spell(letters[l], accidental, 4);
                PitchSpelling *notes[NOTES];
                assert(standard_tonality_build_pentatonic((StandardTonalityPentatonicForm)form, tonic, notes));
                assert(standard_tonality_is_pentatonic((StandardTonalityPentatonicForm)form,
                                                       (const PitchSpelling *const *)notes, NOTES));
                destroy_collection(notes, NOTES);
                pitch_spelling_destroy(tonic);
            }
        }
    }
}

static void test_wrong_collections_are_rejected(void) {
    PitchSpelling *tonic = spell(PITCH_LETTER_C, 0, 4);
    PitchSpelling *major[NOTES];
    assert(standard_tonality_build_pentatonic(STANDARD_TONALITY_PENTATONIC_MAJOR, tonic, major));
    assert(!standard_tonality_is_pentatonic(STANDARD_TONALITY_PENTATONIC_MINOR, (const PitchSpelling *const *)major,
                                            NOTES));
    destroy_collection(major, NOTES);

    /* Right sounds, wrong letters: C D F-flat G A C. */
    PitchSpelling *misspelled[NOTES] = {spell(PITCH_LETTER_C, 0, 4), spell(PITCH_LETTER_D, 0, 4),
                                        spell(PITCH_LETTER_F, -1, 4), spell(PITCH_LETTER_G, 0, 4),
                                        spell(PITCH_LETTER_A, 0, 4), spell(PITCH_LETTER_C, 0, 5)};
    assert(!standard_tonality_is_pentatonic(STANDARD_TONALITY_PENTATONIC_MAJOR,
                                            (const PitchSpelling *const *)misspelled, NOTES));
    destroy_collection(misspelled, NOTES);

    /* A seven-note scale is not a pentatonic collection. */
    const PitchSpelling *one[] = {tonic};
    assert(!standard_tonality_is_pentatonic(STANDARD_TONALITY_PENTATONIC_MAJOR, one, 1));
    assert(!standard_tonality_is_pentatonic(STANDARD_TONALITY_PENTATONIC_MAJOR, NULL, NOTES));
    assert(!standard_tonality_is_pentatonic((StandardTonalityPentatonicForm)99, NULL, NOTES));
    const PitchSpelling *with_null[NOTES] = {tonic, tonic, tonic, tonic, tonic, NULL};
    assert(!standard_tonality_is_pentatonic(STANDARD_TONALITY_PENTATONIC_MAJOR, with_null, NOTES));
    pitch_spelling_destroy(tonic);
}

static void test_build_rejects_invalid_and_oversized_scales(void) {
    PitchSpelling *g_double_sharp = spell(PITCH_LETTER_G, 2, 4); /* its major scale needs F triple-sharp */
    PitchSpelling *notes[NOTES] = {NULL};
    assert(!standard_tonality_build_pentatonic(STANDARD_TONALITY_PENTATONIC_MAJOR, g_double_sharp, notes));
    for (size_t i = 0; i < NOTES; i++) {
        assert(notes[i] == NULL);
    }
    assert(!standard_tonality_build_pentatonic((StandardTonalityPentatonicForm)99, g_double_sharp, notes));
    assert(!standard_tonality_build_pentatonic(STANDARD_TONALITY_PENTATONIC_MAJOR, NULL, notes));
    pitch_spelling_destroy(g_double_sharp);
}

int main(void) {
    test_rules_reference_framework();
    test_intervals();
    test_minor_is_a_rotation_of_major();
    test_degrees();
    test_solfege();
    test_b_flat_major_pentatonic();
    test_d_and_c_major_pentatonic();
    test_minor_pentatonic();
    test_a_minor_is_c_major_from_la();
    test_black_keys();
    test_built_collections_follow_the_pattern();
    test_wrong_collections_are_rejected();
    test_build_rejects_invalid_and_oversized_scales();
    printf("test_pentatonic_rules (standard-tonality): ok\n");
    return 0;
}
