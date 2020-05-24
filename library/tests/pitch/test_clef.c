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

int main(void) {
    test_create_rejects_invalid_sign();
    test_create_rejects_invalid_line();
    test_presets();
    test_equality();
    return 0;
}
