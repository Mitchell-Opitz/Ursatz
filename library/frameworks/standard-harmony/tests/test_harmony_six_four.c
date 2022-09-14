#include <assert.h>
#include <stdio.h>

#include "harmony_six_four_test_util.h"

static SixFourType classify(const HarmonySixFour *s) {
    SixFourType type = SIX_FOUR_TYPE_COUNT;
    assert(harmony_six_four_classify(s, &type));
    return type;
}

static bool classifies(const HarmonySixFour *s) {
    SixFourType type = SIX_FOUR_TYPE_ARPEGGIATING;
    return harmony_six_four_classify(s, &type);
}

static void test_the_worked_examples(void) {
    for (int mode = 0; mode < HARMONY_MODE_COUNT; mode++) {
        HarmonyMode m = (HarmonyMode)mode;
        HarmonySixFour passing = example_passing_tonic(m);
        HarmonySixFour backward = example_passing_backward(m);
        HarmonySixFour predominant = example_passing_predominant(m);
        HarmonySixFour neighbor = example_neighbor_tonic(m);
        HarmonySixFour still_soprano = example_neighbor_static_soprano(m);
        HarmonySixFour dominant = example_neighbor_dominant(m);
        HarmonySixFour arpeggiating = example_arpeggiating(m);
        HarmonySixFour cadential = example_cadential(m);
        assert(classify(&passing) == SIX_FOUR_TYPE_PASSING);
        assert(classify(&backward) == SIX_FOUR_TYPE_PASSING);
        assert(classify(&predominant) == SIX_FOUR_TYPE_PASSING);
        assert(classify(&neighbor) == SIX_FOUR_TYPE_NEIGHBOR);
        assert(classify(&still_soprano) == SIX_FOUR_TYPE_NEIGHBOR);
        assert(classify(&dominant) == SIX_FOUR_TYPE_NEIGHBOR);
        assert(classify(&arpeggiating) == SIX_FOUR_TYPE_ARPEGGIATING);
        assert(classify(&cadential) == SIX_FOUR_TYPE_CADENTIAL);
    }
}

static void test_the_middle_chord_is_a_six_four(void) {
    HarmonySixFour s = example_passing_tonic(HARMONY_MODE_MAJOR);
    assert(harmony_six_four_middle_is_six_four(&s));

    /* A root position chord, or a first inversion one. */
    s.chords[1] = CH(5);
    assert(!harmony_six_four_middle_is_six_four(&s));
    assert(!classifies(&s));
    s.chords[1] = CHI(5, 1);
    assert(!harmony_six_four_middle_is_six_four(&s));
    /* A seventh chord is not the triad. */
    s.chords[1] = CH7I(5, 2);
    assert(!harmony_six_four_middle_is_six_four(&s));

    /* A second inversion triad whose root is not a fourth above the bass: over re it must be V. */
    s = example_passing_tonic(HARMONY_MODE_MAJOR);
    s.chords[1] = CHI(1, 2);
    assert(!harmony_six_four_middle_is_six_four(&s));
    assert(!classifies(&s));
}

static void test_passing_needs_the_same_function(void) {
    /* I V6/4 V6, a tonic and a dominant: not a passing 6/4. */
    HarmonySixFour s = example_passing_tonic(HARMONY_MODE_MAJOR);
    s.chords[2] = CHI(5, 1);
    assert(!classifies(&s));
    /* A predominant on one side only. */
    s = example_passing_predominant(HARMONY_MODE_MAJOR);
    s.chords[2] = CH(1);
    assert(!classifies(&s));
}

static void test_passing_bass_goes_one_way_by_step(void) {
    /* Up then down, or a leap, is not a passing tone. */
    HarmonySixFour turns = example_passing_tonic(HARMONY_MODE_MAJOR);
    turns.steps[2][VOICE_PART_BASS] = -7;
    assert(!classifies(&turns));
    HarmonySixFour leaps = example_passing_tonic(HARMONY_MODE_MAJOR);
    leaps.steps[2][VOICE_PART_BASS] = -4;
    assert(!classifies(&leaps));
}

static void test_neighbor_needs_the_same_root_position_chord(void) {
    HarmonySixFour s = example_neighbor_tonic(HARMONY_MODE_MAJOR);
    s.chords[2] = CHI(1, 1);
    assert(!classifies(&s));
    s = example_neighbor_tonic(HARMONY_MODE_MAJOR);
    s.chords[2] = CH(5);
    assert(!classifies(&s));
    s = example_neighbor_tonic(HARMONY_MODE_MAJOR);
    s.chords[0] = CHI(1, 1);
    assert(!classifies(&s));
}

static void test_arpeggiating_needs_the_upper_voices_to_sustain(void) {
    HarmonySixFour s = example_arpeggiating(HARMONY_MODE_MAJOR);
    s.steps[1][VOICE_PART_SOPRANO] = 5;
    assert(!classifies(&s));
    /* The chord has to stay the same. */
    s = example_arpeggiating(HARMONY_MODE_MAJOR);
    s.chords[2] = CH(4);
    assert(!classifies(&s));
}

static void test_invalid_subject(void) {
    SixFourType type = SIX_FOUR_TYPE_NEIGHBOR;
    HarmonySixFour bad_mode = example_neighbor_tonic(HARMONY_MODE_COUNT);
    HarmonySixFour bad_chord = example_neighbor_tonic(HARMONY_MODE_MAJOR);
    bad_chord.chords[0] = CH(9);
    HarmonySixFour bad_beat = example_neighbor_tonic(HARMONY_MODE_MAJOR);
    bad_beat.beat[2] = BEAT_STRENGTH_COUNT;
    HarmonySixFour *bad[] = {&bad_mode, &bad_chord, &bad_beat};
    for (size_t i = 0; i < COUNT_OF(bad); i++) {
        assert(!harmony_six_four_valid(bad[i]));
        assert(!harmony_six_four_middle_is_six_four(bad[i]));
        assert(!harmony_six_four_classify(bad[i], &type));
    }
    assert(type == SIX_FOUR_TYPE_NEIGHBOR);
    assert(!harmony_six_four_valid(NULL));
    HarmonySixFour ok = example_neighbor_tonic(HARMONY_MODE_MAJOR);
    assert(!harmony_six_four_classify(&ok, NULL));
}

int main(void) {
    test_the_worked_examples();
    test_the_middle_chord_is_a_six_four();
    test_passing_needs_the_same_function();
    test_passing_bass_goes_one_way_by_step();
    test_neighbor_needs_the_same_root_position_chord();
    test_arpeggiating_needs_the_upper_voices_to_sustain();
    test_invalid_subject();
    printf("test_harmony_six_four: ok\n");
    return 0;
}
