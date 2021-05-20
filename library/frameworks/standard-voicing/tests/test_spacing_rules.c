#include <assert.h>
#include <stdio.h>

#include "spacing_rules.h"
#include "voicing_pitch.h"
#include "voicing_test_util.h"

static void test_limits(void) {
    assert(standard_voicing_spacing_limit(VOICE_PART_SOPRANO) == 8);
    assert(standard_voicing_spacing_limit(VOICE_PART_ALTO) == 8);
    assert(standard_voicing_spacing_limit(VOICE_PART_TENOR) == 12);
    assert(standard_voicing_spacing_limit(VOICE_PART_BASS) == 0);
    assert(standard_voicing_spacing_limit(VOICE_PART_COUNT) == 0);
}

static void test_generic_size(void) {
    assert(standard_voicing_generic_size(P(C, 0, 4), P(C, 0, 4)) == 1);
    assert(standard_voicing_generic_size(P(C, 0, 5), P(C, 0, 4)) == 8);
    assert(standard_voicing_generic_size(P(C, 0, 4), P(C, 0, 5)) == 8);
    assert(standard_voicing_generic_size(P(G, 0, 4), P(C, 0, 3)) == 12);
    /* Accidentals do not change a generic size. */
    assert(standard_voicing_generic_size(P(C, 1, 5), P(C, 0, 4)) == 8);
    assert(standard_voicing_generic_size(P(D, -1, 5), P(C, 0, 4)) == 9);
}

static void test_upper_voices_octave(void) {
    assert(standard_voicing_spacing_valid(VOICE_PART_SOPRANO, P(C, 0, 5), P(C, 0, 4)));
    assert(standard_voicing_spacing_valid(VOICE_PART_SOPRANO, P(B, 0, 4), P(C, 0, 4)));
    assert(!standard_voicing_spacing_valid(VOICE_PART_SOPRANO, P(D, 0, 5), P(C, 0, 4)));
    assert(standard_voicing_spacing_valid(VOICE_PART_ALTO, P(C, 0, 4), P(C, 0, 3)));
    assert(!standard_voicing_spacing_valid(VOICE_PART_ALTO, P(D, 0, 4), P(C, 0, 3)));
}

static void test_tenor_bass_twelfth(void) {
    assert(standard_voicing_spacing_valid(VOICE_PART_TENOR, P(G, 0, 4), P(C, 0, 3)));
    assert(standard_voicing_spacing_valid(VOICE_PART_TENOR, P(E, 0, 4), P(C, 0, 3)));
    assert(!standard_voicing_spacing_valid(VOICE_PART_TENOR, P(A, 0, 4), P(C, 0, 3)));
    /* Between the tenor and bass a twelfth is allowed but a thirteenth is not. */
    assert(!standard_voicing_spacing_valid(VOICE_PART_TENOR, P(G, 0, 4), P(B, 0, 2)));
}

static void test_spacing_valid_invalid_input(void) {
    assert(!standard_voicing_spacing_valid(VOICE_PART_BASS, P(C, 0, 4), P(C, 0, 3)));
    assert(!standard_voicing_spacing_valid(VOICE_PART_COUNT, P(C, 0, 4), P(C, 0, 3)));
    assert(!standard_voicing_spacing_valid(VOICE_PART_SOPRANO, NULL, P(C, 0, 3)));
    assert(!standard_voicing_spacing_valid(VOICE_PART_SOPRANO, P(C, 0, 4), NULL));
}

/* Example 15: the soprano and alto are more than an octave apart, as are
   the alto and tenor; then a corrected version. */
static void test_wide_chord(void) {
    StandardVoicingChord wide = make_chord(P(E, 0, 5), P(C, 0, 4), P(G, 0, 2), P(C, 0, 2));
    assert(!standard_voicing_chord_spacing_valid(&wide));
    assert(standard_voicing_first_wide_spacing(&wide) == VOICE_PART_SOPRANO);

    /* Only the alto and tenor are wide: the most common spacing error. */
    StandardVoicingChord alto_tenor = make_chord(P(A, 0, 4), P(B, 0, 3), P(D, 0, 3), P(C, 0, 3));
    assert(!standard_voicing_chord_spacing_valid(&alto_tenor));
    assert(standard_voicing_first_wide_spacing(&alto_tenor) == VOICE_PART_ALTO);

    StandardVoicingChord tenor_bass = make_chord(P(G, 0, 4), P(E, 0, 4), P(C, 0, 4), P(C, 0, 2));
    assert(!standard_voicing_chord_spacing_valid(&tenor_bass));
    assert(standard_voicing_first_wide_spacing(&tenor_bass) == VOICE_PART_TENOR);

    StandardVoicingChord corrected = make_chord(P(E, 0, 4), P(C, 0, 4), P(G, 0, 3), P(C, 0, 3));
    assert(standard_voicing_chord_spacing_valid(&corrected));
    assert(standard_voicing_first_wide_spacing(&corrected) == VOICE_PART_COUNT);
}

static void test_chord_invalid_input(void) {
    assert(!standard_voicing_chord_spacing_valid(NULL));
    assert(standard_voicing_first_wide_spacing(NULL) == VOICE_PART_COUNT);

    StandardVoicingChord incomplete = make_chord(P(E, 0, 4), NULL, P(G, 0, 3), P(C, 0, 3));
    assert(!standard_voicing_chord_spacing_valid(&incomplete));
}

int main(void) {
    test_limits();
    test_generic_size();
    test_upper_voices_octave();
    test_tenor_bass_twelfth();
    test_spacing_valid_invalid_input();
    test_wide_chord();
    test_chord_invalid_input();
    printf("test_spacing_rules (standard-voicing): ok\n");
    return 0;
}
