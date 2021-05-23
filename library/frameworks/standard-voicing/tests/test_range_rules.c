#include <assert.h>
#include <stdio.h>

#include "range_rules.h"
#include "voicing_pitch.h"
#include "voicing_test_util.h"

static void test_range_semitones(void) {
    int low;
    int high;
    assert(standard_voicing_range_semitones(VOICE_PART_SOPRANO, &low, &high) && low == 48 && high == 67);
    assert(standard_voicing_range_semitones(VOICE_PART_ALTO, &low, &high) && low == 43 && high == 62);
    assert(standard_voicing_range_semitones(VOICE_PART_TENOR, &low, &high) && low == 36 && high == 55);
    assert(standard_voicing_range_semitones(VOICE_PART_BASS, &low, &high) && low == 29 && high == 50);

    low = -1;
    assert(!standard_voicing_range_semitones(VOICE_PART_COUNT, &low, &high));
    assert(low == -1);
    assert(!standard_voicing_range_semitones(VOICE_PART_BASS, NULL, &high));
    assert(!standard_voicing_range_semitones(VOICE_PART_BASS, &low, NULL));
}

static void test_range_bounds(void) {
    PitchLetter low_letter;
    PitchLetter high_letter;
    int low_octave;
    int high_octave;

    assert(standard_voicing_range_bounds(VOICE_PART_SOPRANO, &low_letter, &low_octave, &high_letter, &high_octave));
    assert(low_letter == PITCH_LETTER_C && low_octave == 4 && high_letter == PITCH_LETTER_G && high_octave == 5);
    assert(standard_voicing_range_bounds(VOICE_PART_ALTO, &low_letter, &low_octave, &high_letter, &high_octave));
    assert(low_letter == PITCH_LETTER_G && low_octave == 3 && high_letter == PITCH_LETTER_D && high_octave == 5);
    assert(standard_voicing_range_bounds(VOICE_PART_TENOR, &low_letter, &low_octave, &high_letter, &high_octave));
    assert(low_letter == PITCH_LETTER_C && low_octave == 3 && high_letter == PITCH_LETTER_G && high_octave == 4);
    assert(standard_voicing_range_bounds(VOICE_PART_BASS, &low_letter, &low_octave, &high_letter, &high_octave));
    assert(low_letter == PITCH_LETTER_F && low_octave == 2 && high_letter == PITCH_LETTER_D && high_octave == 4);

    assert(!standard_voicing_range_bounds(VOICE_PART_COUNT, &low_letter, &low_octave, &high_letter, &high_octave));
    assert(!standard_voicing_range_bounds(VOICE_PART_BASS, NULL, &low_octave, &high_letter, &high_octave));
}

/* Both ends of each range are included. */
static void test_boundaries(void) {
    assert(standard_voicing_in_range(VOICE_PART_SOPRANO, P(C, 0, 4)));
    assert(standard_voicing_in_range(VOICE_PART_SOPRANO, P(G, 0, 5)));
    assert(!standard_voicing_in_range(VOICE_PART_SOPRANO, P(B, 0, 3)));
    assert(!standard_voicing_in_range(VOICE_PART_SOPRANO, P(A, 0, 5)));

    assert(standard_voicing_in_range(VOICE_PART_ALTO, P(G, 0, 3)));
    assert(standard_voicing_in_range(VOICE_PART_ALTO, P(D, 0, 5)));
    assert(!standard_voicing_in_range(VOICE_PART_ALTO, P(F, 0, 3)));
    assert(!standard_voicing_in_range(VOICE_PART_ALTO, P(E, 0, 5)));

    assert(standard_voicing_in_range(VOICE_PART_TENOR, P(C, 0, 3)));
    assert(standard_voicing_in_range(VOICE_PART_TENOR, P(G, 0, 4)));
    assert(!standard_voicing_in_range(VOICE_PART_TENOR, P(B, 0, 2)));
    assert(!standard_voicing_in_range(VOICE_PART_TENOR, P(A, 0, 4)));

    assert(standard_voicing_in_range(VOICE_PART_BASS, P(F, 0, 2)));
    assert(standard_voicing_in_range(VOICE_PART_BASS, P(D, 0, 4)));
    assert(!standard_voicing_in_range(VOICE_PART_BASS, P(E, 0, 2)));
    assert(!standard_voicing_in_range(VOICE_PART_BASS, P(E, 0, 4)));
}

/* Accidentals move a note across the boundary. */
static void test_accidentals_and_enharmonics(void) {
    /* The sounding pitch counts: B-sharp 3 sounds as C4, C-flat 4 as B3. */
    assert(standard_voicing_in_range(VOICE_PART_SOPRANO, P(B, 1, 3)));
    assert(!standard_voicing_in_range(VOICE_PART_SOPRANO, P(C, -1, 4)));

    assert(standard_voicing_in_range(VOICE_PART_SOPRANO, P(F, 1, 5)));
    assert(!standard_voicing_in_range(VOICE_PART_SOPRANO, P(G, 1, 5)));
    assert(!standard_voicing_in_range(VOICE_PART_BASS, P(F, -1, 2)));
    assert(standard_voicing_in_range(VOICE_PART_BASS, P(E, 1, 2)));
}

/* Example 4: the soprano and bass are out of range; the soprano moves down
   an octave and the bass up an octave. */
static void test_out_of_range_chord(void) {
    StandardVoicingChord out_of_range = make_chord(P(C, 0, 6), P(E, 0, 4), P(G, 0, 3), P(C, 0, 1));
    assert(!standard_voicing_chord_in_range(&out_of_range));
    assert(!standard_voicing_in_range(VOICE_PART_SOPRANO, out_of_range.voice[VOICE_PART_SOPRANO]));
    assert(standard_voicing_in_range(VOICE_PART_ALTO, out_of_range.voice[VOICE_PART_ALTO]));
    assert(standard_voicing_in_range(VOICE_PART_TENOR, out_of_range.voice[VOICE_PART_TENOR]));
    assert(!standard_voicing_in_range(VOICE_PART_BASS, out_of_range.voice[VOICE_PART_BASS]));

    StandardVoicingChord corrected = make_chord(P(C, 0, 5), P(E, 0, 4), P(G, 0, 3), P(C, 0, 3));
    assert(standard_voicing_chord_in_range(&corrected));
}

static void test_invalid_input(void) {
    assert(!standard_voicing_in_range(VOICE_PART_SOPRANO, NULL));
    assert(!standard_voicing_in_range(VOICE_PART_COUNT, P(C, 0, 4)));
    assert(!standard_voicing_chord_in_range(NULL));

    StandardVoicingChord incomplete = make_chord(P(C, 0, 5), NULL, P(G, 0, 3), P(C, 0, 3));
    assert(!standard_voicing_chord_in_range(&incomplete));
}

int main(void) {
    test_range_semitones();
    test_range_bounds();
    test_boundaries();
    test_accidentals_and_enharmonics();
    test_out_of_range_chord();
    test_invalid_input();
    printf("test_range_rules (standard-voicing): ok\n");
    return 0;
}
