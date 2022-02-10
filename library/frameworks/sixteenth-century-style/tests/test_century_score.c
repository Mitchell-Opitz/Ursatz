#include <assert.h>
#include <stdio.h>

#include "century_score.h"
#include "century_test_util.h"

static void test_valid_score(void) {
    const PitchSpelling *pitches[] = {P(C, 0, 4), P(D, 0, 4)};
    MELODY(score, pitches, NULL, false);
    assert(sixteenth_century_score_valid(&score));
    assert(sixteenth_century_score_slot_count(&score) == 8);
}

static void test_invalid_scores(void) {
    assert(!sixteenth_century_score_valid(NULL));
    assert(sixteenth_century_score_slot_count(NULL) == 0);

    const PitchSpelling *pitches[] = {P(C, 0, 4)};
    MELODY(base, pitches, NULL, false);

    SixteenthCenturyScore no_voices = base;
    no_voices.voice_count = 0;
    assert(!sixteenth_century_score_valid(&no_voices));

    SixteenthCenturyScore odd_tactus = base;
    odd_tactus.slots_per_tactus = 3;
    assert(!sixteenth_century_score_valid(&odd_tactus));
    odd_tactus.slots_per_tactus = 0;
    assert(!sixteenth_century_score_valid(&odd_tactus));

    /* Voices of different lengths. */
    StandardCounterpointSlot a[] = {HALF(P(C, 0, 4))};
    StandardCounterpointSlot b[] = {WHOLE(P(C, 0, 3))};
    StandardCounterpointSlotLine uneven[] = {{a, COUNT_OF(a)}, {b, COUNT_OF(b)}};
    SCORE_OF(uneven_score, uneven);
    assert(!sixteenth_century_score_valid(&uneven_score));

    /* A tie that does not continue the same pitch, and a tie first. */
    StandardCounterpointSlot wrong_tie[] = {ATK(P(C, 0, 4)), TIE(P(D, 0, 4))};
    StandardCounterpointSlotLine wrong_tie_line[] = {{wrong_tie, COUNT_OF(wrong_tie)}};
    SCORE_OF(wrong_tie_score, wrong_tie_line);
    assert(!sixteenth_century_score_valid(&wrong_tie_score));

    StandardCounterpointSlot tie_first[] = {TIE(P(C, 0, 4)), ATK(P(C, 0, 4))};
    StandardCounterpointSlotLine tie_first_line[] = {{tie_first, COUNT_OF(tie_first)}};
    SCORE_OF(tie_first_score, tie_first_line);
    assert(!sixteenth_century_score_valid(&tie_first_score));

    /* A tie after a rest, an attack with no pitch, and a rest with one. */
    StandardCounterpointSlot after_rest[] = {REST(), TIE(P(C, 0, 4))};
    StandardCounterpointSlotLine after_rest_line[] = {{after_rest, COUNT_OF(after_rest)}};
    SCORE_OF(after_rest_score, after_rest_line);
    assert(!sixteenth_century_score_valid(&after_rest_score));

    StandardCounterpointSlot no_pitch[] = {{STANDARD_COUNTERPOINT_SLOT_ATTACK, NULL}};
    StandardCounterpointSlotLine no_pitch_line[] = {{no_pitch, COUNT_OF(no_pitch)}};
    SCORE_OF(no_pitch_score, no_pitch_line);
    assert(!sixteenth_century_score_valid(&no_pitch_score));

    StandardCounterpointSlot rest_pitch[] = {{STANDARD_COUNTERPOINT_SLOT_REST, P(C, 0, 4)}};
    StandardCounterpointSlotLine rest_pitch_line[] = {{rest_pitch, COUNT_OF(rest_pitch)}};
    SCORE_OF(rest_pitch_score, rest_pitch_line);
    assert(!sixteenth_century_score_valid(&rest_pitch_score));
}

static void test_positions(void) {
    const PitchSpelling *pitches[] = {P(C, 0, 4)};
    MELODY(score, pitches, NULL, false);
    assert(sixteenth_century_position_at(&score, 0) == SIXTEENTH_CENTURY_POSITION_STRONG);
    assert(sixteenth_century_position_at(&score, 1) == SIXTEENTH_CENTURY_POSITION_SUB);
    assert(sixteenth_century_position_at(&score, 2) == SIXTEENTH_CENTURY_POSITION_SUB);
    assert(sixteenth_century_position_at(&score, 4) == SIXTEENTH_CENTURY_POSITION_WEAK);
    assert(sixteenth_century_position_at(&score, 8) == SIXTEENTH_CENTURY_POSITION_STRONG);
    assert(sixteenth_century_position_at(&score, 12) == SIXTEENTH_CENTURY_POSITION_WEAK);
}

static void test_pitch_and_attacks(void) {
    const PitchSpelling *c4 = P(C, 0, 4);
    StandardCounterpointSlot slots[] = {REST(), ATK(c4), TIE(c4), REST()};
    StandardCounterpointSlotLine voices[] = {{slots, COUNT_OF(slots)}};
    SCORE_OF(score, voices);
    assert(sixteenth_century_pitch_at(&score, 0, 0) == NULL);
    assert(sixteenth_century_pitch_at(&score, 0, 1) == c4);
    assert(sixteenth_century_pitch_at(&score, 0, 2) == c4);
    assert(sixteenth_century_pitch_at(&score, 0, 3) == NULL);
    assert(sixteenth_century_pitch_at(&score, 0, 4) == NULL);
    assert(sixteenth_century_pitch_at(&score, 1, 0) == NULL);
    assert(!sixteenth_century_attacks_at(&score, 0, 0));
    assert(sixteenth_century_attacks_at(&score, 0, 1));
    assert(!sixteenth_century_attacks_at(&score, 0, 2));
    assert(!sixteenth_century_attacks_at(&score, 0, 9));
}

static void test_voice_notes(void) {
    const PitchSpelling *c4 = P(C, 0, 4);
    const PitchSpelling *d4 = P(D, 0, 4);
    StandardCounterpointSlot slots[] = {REST(), ATK(c4), TIE(c4), TIE(c4), ATK(d4), ATK(d4)};
    StandardCounterpointSlotLine voices[] = {{slots, COUNT_OF(slots)}};
    SCORE_OF(score, voices);

    SixteenthCenturyNote notes[8];
    size_t count = 0;
    assert(sixteenth_century_voice_notes(&score, 0, notes, 8, &count));
    assert(count == 3);
    assert(notes[0].start == 1 && notes[0].length == 3 && notes[0].pitch == c4);
    assert(notes[1].start == 4 && notes[1].length == 1 && notes[1].pitch == d4);
    assert(notes[2].start == 5 && notes[2].length == 1 && notes[2].pitch == d4);

    assert(!sixteenth_century_voice_notes(&score, 0, notes, 2, &count));
    assert(!sixteenth_century_voice_notes(&score, 1, notes, 8, &count));
    assert(!sixteenth_century_voice_notes(&score, 0, NULL, 8, &count));
    assert(!sixteenth_century_voice_notes(&score, 0, notes, 8, NULL));
}

int main(void) {
    test_valid_score();
    test_invalid_scores();
    test_positions();
    test_pitch_and_attacks();
    test_voice_notes();
    printf("test_century_score: ok\n");
    return 0;
}
