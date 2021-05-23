#include <assert.h>
#include <stdio.h>

#include "crossing_rules.h"
#include "voicing_test_util.h"

static void test_pair_ordered(void) {
    assert(standard_voicing_pair_ordered(P(D, 0, 4), P(C, 0, 4)));
    assert(!standard_voicing_pair_ordered(P(C, 0, 4), P(D, 0, 4)));
    /* Strictly higher: the same pitch is not in order. */
    assert(!standard_voicing_pair_ordered(P(C, 0, 4), P(C, 0, 4)));
    /* Pitch is by sound: B-sharp 3 sounds as C4. */
    assert(!standard_voicing_pair_ordered(P(C, 0, 4), P(B, 1, 3)));
    assert(standard_voicing_pair_ordered(P(C, 1, 4), P(B, 1, 3)));
    assert(!standard_voicing_pair_ordered(NULL, P(C, 0, 4)));
    assert(!standard_voicing_pair_ordered(P(C, 0, 4), NULL));
}

static void test_ordered_chord(void) {
    StandardVoicingChord chord = make_chord(P(C, 0, 5), P(E, 0, 4), P(G, 0, 3), P(C, 0, 3));
    assert(standard_voicing_chord_uncrossed(&chord));
    assert(standard_voicing_first_crossing(&chord) == VOICE_PART_COUNT);
}

/* Example 6: the alto and tenor voices are crossed. */
static void test_alto_tenor_crossed(void) {
    StandardVoicingChord chord = make_chord(P(G, 0, 4), P(C, 0, 4), P(E, 0, 4), P(C, 0, 3));
    assert(!standard_voicing_chord_uncrossed(&chord));
    assert(standard_voicing_first_crossing(&chord) == VOICE_PART_ALTO);

    StandardVoicingChord corrected = make_chord(P(G, 0, 4), P(E, 0, 4), P(C, 0, 4), P(C, 0, 3));
    assert(standard_voicing_chord_uncrossed(&corrected));
}

static void test_other_crossings(void) {
    StandardVoicingChord soprano = make_chord(P(E, 0, 4), P(G, 0, 4), P(C, 0, 4), P(C, 0, 3));
    assert(standard_voicing_first_crossing(&soprano) == VOICE_PART_SOPRANO);

    StandardVoicingChord tenor = make_chord(P(G, 0, 4), P(E, 0, 4), P(C, 0, 3), P(G, 0, 3));
    assert(standard_voicing_first_crossing(&tenor) == VOICE_PART_TENOR);

    /* Voices on the same pitch are not correctly ordered. */
    StandardVoicingChord unison = make_chord(P(C, 0, 4), P(C, 0, 4), P(G, 0, 3), P(C, 0, 3));
    assert(!standard_voicing_chord_uncrossed(&unison));
    assert(standard_voicing_first_crossing(&unison) == VOICE_PART_SOPRANO);
}

static void test_invalid_input(void) {
    assert(!standard_voicing_chord_uncrossed(NULL));
    assert(standard_voicing_first_crossing(NULL) == VOICE_PART_COUNT);

    StandardVoicingChord incomplete = make_chord(P(G, 0, 4), P(E, 0, 4), NULL, P(C, 0, 3));
    assert(!standard_voicing_chord_uncrossed(&incomplete));
    assert(standard_voicing_first_crossing(&incomplete) == VOICE_PART_ALTO);
}

int main(void) {
    test_pair_ordered();
    test_ordered_chord();
    test_alto_tenor_crossed();
    test_other_crossings();
    test_invalid_input();
    printf("test_crossing_rules (standard-voicing): ok\n");
    return 0;
}
