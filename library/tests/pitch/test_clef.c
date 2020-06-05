#include "pitch/clef.h"

#include <assert.h>
#include <stddef.h>

static void test_create_rejects_invalid_sign(void) {
    Clef *clef = NULL;
    assert(clef_create((ClefSign)99, 2, &clef) == CLEF_ERR_INVALID_SIGN);
    assert(clef == NULL);
}

static void test_create_rejects_invalid_line(void) {
    Clef *clef = NULL;
    assert(clef_create(CLEF_SIGN_G, 0, &clef) == CLEF_ERR_INVALID_LINE);
    assert(clef_create(CLEF_SIGN_G, 6, &clef) == CLEF_ERR_INVALID_LINE);
    assert(clef == NULL);
}

static void test_presets(void) {
    Clef *treble = NULL, *bass = NULL, *alto = NULL, *tenor = NULL;
    assert(clef_create_treble(&treble) == CLEF_OK);
    assert(clef_create_bass(&bass) == CLEF_OK);
    assert(clef_create_alto(&alto) == CLEF_OK);
    assert(clef_create_tenor(&tenor) == CLEF_OK);

    assert(clef_sign(treble) == CLEF_SIGN_G && clef_line(treble) == 2);
    assert(clef_sign(bass) == CLEF_SIGN_F && clef_line(bass) == 4);
    assert(clef_sign(alto) == CLEF_SIGN_C && clef_line(alto) == 3);
    assert(clef_sign(tenor) == CLEF_SIGN_C && clef_line(tenor) == 4);

    assert(clef_reference_letter(treble) == PITCH_LETTER_G && clef_reference_octave(treble) == 4);
    assert(clef_reference_letter(bass) == PITCH_LETTER_F && clef_reference_octave(bass) == 3);
    assert(clef_reference_letter(alto) == PITCH_LETTER_C && clef_reference_octave(alto) == 4);
    assert(clef_reference_letter(tenor) == PITCH_LETTER_C && clef_reference_octave(tenor) == 4);

    clef_destroy(treble);
    clef_destroy(bass);
    clef_destroy(alto);
    clef_destroy(tenor);
}

static void test_equality(void) {
    Clef *a = NULL, *b = NULL, *c = NULL;
    assert(clef_create(CLEF_SIGN_C, 3, &a) == CLEF_OK);
    assert(clef_create_alto(&b) == CLEF_OK);
    assert(clef_create_tenor(&c) == CLEF_OK);

    assert(clef_equals(a, b));
    assert(!clef_equals(a, c));
    assert(!clef_equals(a, NULL));

    clef_destroy(a);
    clef_destroy(b);
    clef_destroy(c);
}

static void test_octave_shift(void) {
    Clef *plain = NULL, *down = NULL, *up = NULL, *down_again = NULL;
    assert(clef_create_treble(&plain) == CLEF_OK);
    assert(clef_create_shifted(CLEF_SIGN_G, 2, -1, &down) == CLEF_OK);
    assert(clef_create_shifted(CLEF_SIGN_G, 2, 1, &up) == CLEF_OK);
    assert(clef_create_shifted(CLEF_SIGN_G, 2, -1, &down_again) == CLEF_OK);

    assert(clef_octave_shift(plain) == 0);
    assert(clef_octave_shift(down) == -1);
    assert(clef_octave_shift(up) == 1);

    /* Shift is part of value equality. */
    assert(!clef_equals(plain, down));
    assert(!clef_equals(down, up));
    assert(clef_equals(down, down_again));

    /* clef_create is shift 0. */
    Clef *created = NULL;
    assert(clef_create(CLEF_SIGN_G, 2, &created) == CLEF_OK);
    assert(clef_equals(plain, created));

    clef_destroy(plain);
    clef_destroy(down);
    clef_destroy(up);
    clef_destroy(down_again);
    clef_destroy(created);
}

static void test_invalid_octave_shift(void) {
    Clef *clef = NULL;
    assert(clef_create_shifted(CLEF_SIGN_G, 2, 2, &clef) == CLEF_ERR_INVALID_OCTAVE_SHIFT);
    assert(clef_create_shifted(CLEF_SIGN_G, 2, -2, &clef) == CLEF_ERR_INVALID_OCTAVE_SHIFT);
    assert(clef == NULL);
}

static void test_neutral(void) {
    Clef *neutral = NULL, *treble = NULL, *other = NULL;
    assert(clef_create_neutral(&neutral) == CLEF_OK);
    assert(clef_create_treble(&treble) == CLEF_OK);
    assert(clef_create_neutral(&other) == CLEF_OK);

    assert(clef_sign(neutral) == CLEF_SIGN_NEUTRAL);
    assert(!clef_is_pitched(neutral));
    assert(clef_is_pitched(treble));
    assert(clef_octave_shift(neutral) == 0);
    assert(clef_equals(neutral, other));
    assert(!clef_equals(neutral, treble));

    /* Neutral is built only with clef_create_neutral. */
    Clef *rejected = NULL;
    assert(clef_create(CLEF_SIGN_NEUTRAL, 3, &rejected) == CLEF_ERR_INVALID_SIGN);
    assert(clef_create_shifted(CLEF_SIGN_NEUTRAL, 3, 1, &rejected) == CLEF_ERR_INVALID_SIGN);
    assert(rejected == NULL);

    clef_destroy(neutral);
    clef_destroy(treble);
    clef_destroy(other);
}

static void test_other_c_clefs(void) {
    Clef *soprano = NULL, *mezzo = NULL, *baritone = NULL;
    assert(clef_create_soprano(&soprano) == CLEF_OK);
    assert(clef_create_mezzo_soprano(&mezzo) == CLEF_OK);
    assert(clef_create_baritone(&baritone) == CLEF_OK);

    assert(clef_sign(soprano) == CLEF_SIGN_C && clef_line(soprano) == 1);
    assert(clef_sign(mezzo) == CLEF_SIGN_C && clef_line(mezzo) == 2);
    assert(clef_sign(baritone) == CLEF_SIGN_C && clef_line(baritone) == 5);
    assert(clef_reference_letter(baritone) == PITCH_LETTER_C && clef_reference_octave(baritone) == 4);

    clef_destroy(soprano);
    clef_destroy(mezzo);
    clef_destroy(baritone);
}

int main(void) {
    test_create_rejects_invalid_sign();
    test_create_rejects_invalid_line();
    test_presets();
    test_equality();
    test_octave_shift();
    test_invalid_octave_shift();
    test_neutral();
    test_other_c_clefs();
    return 0;
}
