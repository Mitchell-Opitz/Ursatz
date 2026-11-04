#include "pitch/midi_pitch.h"
#include "pitch/pitch_arithmetic.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

static void test_pitch_class(void) {
    assert(pitch_class_of_midi(60) == 0);
    assert(pitch_class_of_midi(61) == 1);
    assert(pitch_class_of_midi(127) == 7);
    assert(pitch_class_of_midi(0) == 0);
    assert(pitch_class_of_midi(-1) == 11);
    assert(pitch_class_of_midi(-12) == 0);
    assert(pitch_class_of_midi(-13) == 11);
    for (int midi = -48; midi <= 200; midi++) {
        int pc = pitch_class_of_midi(midi);
        assert(pc >= 0 && pc <= 11);
        assert(pitch_class_of_midi(midi + 12) == pc);
    }
}

static void check_spelling(int midi, int fifths, PitchLetter letter, int accidental, int octave) {
    PitchSpelling *spelling = NULL;
    assert(pitch_spelling_of_midi(midi, fifths, &spelling));
    assert(pitch_spelling_letter(spelling) == letter);
    assert(pitch_spelling_accidental(spelling) == accidental);
    assert(pitch_spelling_octave(spelling) == octave);
    pitch_spelling_destroy(spelling);
}

static void test_spelling_sharps_and_flats(void) {
    check_spelling(60, 0, PITCH_LETTER_C, 0, 4);
    check_spelling(61, 0, PITCH_LETTER_C, 1, 4);
    check_spelling(61, 3, PITCH_LETTER_C, 1, 4);
    check_spelling(61, -1, PITCH_LETTER_D, -1, 4);
    check_spelling(70, 0, PITCH_LETTER_A, 1, 4);
    check_spelling(70, -2, PITCH_LETTER_B, -1, 4);
    check_spelling(66, 0, PITCH_LETTER_F, 1, 4);
    check_spelling(66, -7, PITCH_LETTER_G, -1, 4);
    check_spelling(71, -3, PITCH_LETTER_B, 0, 4);
    check_spelling(72, 0, PITCH_LETTER_C, 0, 5);
}

static void test_spelling_octaves(void) {
    check_spelling(0, 0, PITCH_LETTER_C, 0, -1);
    check_spelling(21, 0, PITCH_LETTER_A, 0, 0);
    check_spelling(127, 0, PITCH_LETTER_G, 0, 9);
    check_spelling(-1, 0, PITCH_LETTER_B, 0, -2);
    check_spelling(-12, 0, PITCH_LETTER_C, 0, -2);
}

static void test_spelling_matches_pitch_class(void) {
    for (int fifths = -1; fifths <= 1; fifths += 2) {
        for (int midi = 0; midi <= 127; midi++) {
            PitchSpelling *spelling = NULL;
            assert(pitch_spelling_of_midi(midi, fifths, &spelling));
            assert(pitch_spelling_midi_number(spelling) == midi);
            pitch_spelling_destroy(spelling);
        }
    }
}

static void test_spelling_null_out(void) {
    assert(!pitch_spelling_of_midi(60, 0, NULL));
}

static void check_signature(int fifths, size_t count, PitchLetter first, PitchLetter last, int accidental) {
    KeySignature *signature = NULL;
    assert(key_signature_from_fifths(fifths, &signature));
    assert(key_signature_entry_count(signature) == count);
    if (count > 0) {
        assert(key_signature_letter_at(signature, 0) == first);
        assert(key_signature_letter_at(signature, count - 1) == last);
        for (size_t i = 0; i < count; i++) {
            assert(key_signature_accidental_at(signature, i) == accidental);
        }
    }
    key_signature_destroy(signature);
}

static void test_key_signature_from_fifths(void) {
    check_signature(0, 0, PITCH_LETTER_C, PITCH_LETTER_C, 0);
    check_signature(1, 1, PITCH_LETTER_F, PITCH_LETTER_F, 1);
    check_signature(2, 2, PITCH_LETTER_F, PITCH_LETTER_C, 1);
    check_signature(7, 7, PITCH_LETTER_F, PITCH_LETTER_B, 1);
    check_signature(-1, 1, PITCH_LETTER_B, PITCH_LETTER_B, -1);
    check_signature(-3, 3, PITCH_LETTER_B, PITCH_LETTER_A, -1);
    check_signature(-7, 7, PITCH_LETTER_B, PITCH_LETTER_F, -1);
}

static void test_key_signature_rejects(void) {
    KeySignature *signature = NULL;
    assert(!key_signature_from_fifths(8, &signature));
    assert(!key_signature_from_fifths(-8, &signature));
    assert(signature == NULL);
    assert(!key_signature_from_fifths(0, NULL));
}

static void test_tonic_names(void) {
    static const char *const NAMES[12] = {"C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
    for (int pc = 0; pc < 12; pc++) {
        assert(strcmp(key_tonic_name(pc), NAMES[pc]) == 0);
        assert(strcmp(key_tonic_name(pc + 12), NAMES[pc]) == 0);
        assert(strcmp(key_tonic_name(pc - 12), NAMES[pc]) == 0);
    }
}

int main(void) {
    test_pitch_class();
    test_spelling_sharps_and_flats();
    test_spelling_octaves();
    test_spelling_matches_pitch_class();
    test_spelling_null_out();
    test_key_signature_from_fifths();
    test_key_signature_rejects();
    test_tonic_names();
    return 0;
}
