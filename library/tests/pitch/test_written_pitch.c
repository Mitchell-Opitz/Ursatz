#include "pitch/written_pitch.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>

static PitchSpelling *make_spelling(PitchLetter letter, int accidental, int octave) {
    PitchSpelling *spelling = NULL;
    pitch_spelling_create(letter, accidental, octave, &spelling);
    return spelling;
}

static void test_create_rejects_null_spelling(void) {
    WrittenPitch *written_pitch = NULL;
    WrittenPitchResult r = written_pitch_create(NULL, &written_pitch);
    assert(r == WRITTEN_PITCH_ERR_NULL_SPELLING);
    assert(written_pitch == NULL);
}

static void test_create_accepts_valid(void) {
    WrittenPitch *written_pitch = NULL;
    WrittenPitchResult r = written_pitch_create(make_spelling(PITCH_LETTER_C, 1, 4), &written_pitch);
    assert(r == WRITTEN_PITCH_OK);
    assert(written_pitch != NULL);

    const PitchSpelling *spelling = written_pitch_spelling(written_pitch);
    assert(pitch_spelling_letter(spelling) == PITCH_LETTER_C);
    assert(pitch_spelling_accidental(spelling) == 1);
    assert(pitch_spelling_octave(spelling) == 4);

    written_pitch_destroy(written_pitch);
}

static void test_equals_same_spelling(void) {
    WrittenPitch *a = NULL;
    WrittenPitch *b = NULL;
    written_pitch_create(make_spelling(PITCH_LETTER_F, 0, 5), &a);
    written_pitch_create(make_spelling(PITCH_LETTER_F, 0, 5), &b);

    assert(a != b);
    assert(written_pitch_equals(a, b));

    written_pitch_destroy(a);
    written_pitch_destroy(b);
}

static void test_inequals_different_spelling(void) {
    WrittenPitch *a = NULL;
    WrittenPitch *b = NULL;
    written_pitch_create(make_spelling(PITCH_LETTER_F, 0, 5), &a);
    written_pitch_create(make_spelling(PITCH_LETTER_F, 0, 6), &b);

    assert(!written_pitch_equals(a, b));

    written_pitch_destroy(a);
    written_pitch_destroy(b);
}

/* Enharmonic non-collapse carries through from PitchSpelling: C# and Db
   remain distinct WrittenPitch values. */
static void test_inequals_enharmonic_spellings(void) {
    WrittenPitch *c_sharp = NULL;
    WrittenPitch *d_flat = NULL;
    written_pitch_create(make_spelling(PITCH_LETTER_C, 1, 4), &c_sharp);
    written_pitch_create(make_spelling(PITCH_LETTER_D, -1, 4), &d_flat);

    assert(!written_pitch_equals(c_sharp, d_flat));

    written_pitch_destroy(c_sharp);
    written_pitch_destroy(d_flat);
}

int main(void) {
    test_create_rejects_null_spelling();
    test_create_accepts_valid();
    test_equals_same_spelling();
    test_inequals_different_spelling();
    test_inequals_enharmonic_spellings();
    printf("test_written_pitch: ok\n");
    return 0;
}