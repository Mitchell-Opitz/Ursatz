#include "pitch/pitch_spelling.h"

#include <assert.h>
#include <stddef.h>

static void test_create_rejects_invalid_letter(void) {
    PitchSpelling *spelling = NULL;
    PitchSpellingResult r = pitch_spelling_create((PitchLetter)99, 0, 4, &spelling);
    assert(r == PITCH_SPELLING_ERR_INVALID_LETTER);
    assert(spelling == NULL);
}

static void test_create_accepts_valid(void) {
    PitchSpelling *spelling = NULL;
    PitchSpellingResult r = pitch_spelling_create(PITCH_LETTER_C, 1, 4, &spelling);
    assert(r == PITCH_SPELLING_OK);
    assert(spelling != NULL);
    assert(pitch_spelling_letter(spelling) == PITCH_LETTER_C);
    assert(pitch_spelling_accidental(spelling) == 1);
    assert(pitch_spelling_octave(spelling) == 4);
    pitch_spelling_destroy(spelling);
}

/* Property: round-trip through the constructor/accessors preserves the
   exact (letter, accidental, octave) triple -- no enharmonic collapse. */
static void test_round_trips_without_enharmonic_collapse(void) {
    PitchLetter letters[] = {PITCH_LETTER_A, PITCH_LETTER_B, PITCH_LETTER_C, PITCH_LETTER_D,
                              PITCH_LETTER_E, PITCH_LETTER_F, PITCH_LETTER_G};

    for (size_t i = 0; i < sizeof(letters) / sizeof(letters[0]); i++) {
        for (int accidental = -2; accidental <= 2; accidental++) {
            PitchSpelling *spelling = NULL;
            pitch_spelling_create(letters[i], accidental, 3, &spelling);

            assert(pitch_spelling_letter(spelling) == letters[i]);
            assert(pitch_spelling_accidental(spelling) == accidental);
            assert(pitch_spelling_octave(spelling) == 3);

            pitch_spelling_destroy(spelling);
        }
    }
}

/* C# and Db are NOT value-equal even though they may sound identical
   under some tunings -- this is intentional. */
static void test_enharmonic_equivalents_not_equal(void) {
    PitchSpelling *c_sharp = NULL;
    PitchSpelling *d_flat = NULL;
    pitch_spelling_create(PITCH_LETTER_C, 1, 4, &c_sharp);
    pitch_spelling_create(PITCH_LETTER_D, -1, 4, &d_flat);

    assert(!pitch_spelling_equals(c_sharp, d_flat));

    pitch_spelling_destroy(c_sharp);
    pitch_spelling_destroy(d_flat);
}

static void test_equality_independent_instances(void) {
    PitchSpelling *a = NULL;
    PitchSpelling *b = NULL;
    pitch_spelling_create(PITCH_LETTER_F, 0, 5, &a);
    pitch_spelling_create(PITCH_LETTER_F, 0, 5, &b);

    assert(a != b);
    assert(pitch_spelling_equals(a, b));

    pitch_spelling_destroy(a);
    pitch_spelling_destroy(b);
}

static void test_inequality_different_octave(void) {
    PitchSpelling *a = NULL;
    PitchSpelling *b = NULL;
    pitch_spelling_create(PITCH_LETTER_F, 0, 4, &a);
    pitch_spelling_create(PITCH_LETTER_F, 0, 5, &b);

    assert(!pitch_spelling_equals(a, b));

    pitch_spelling_destroy(a);
    pitch_spelling_destroy(b);
}

int main(void) {
    test_create_rejects_invalid_letter();
    test_create_accepts_valid();
    test_round_trips_without_enharmonic_collapse();
    test_enharmonic_equivalents_not_equal();
    test_equality_independent_instances();
    test_inequality_different_octave();
    return 0;
}