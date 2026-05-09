#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "non_chord_tones.h"

/*
 * Non-chord-tone labeling tests: passing, neighbor, appoggiatura, escape, suspension, anticipation and
 * pedal, by voice and by nearest pitch, and the cases that must stay unclassified.
 * Pitches are MIDI numbers (C4 = 60). Chord masks are 12-bit pitch-class sets.
 */

#define MASK(a, b, c) ((1u << (a)) | (1u << (b)) | (1u << (c)))
#define C_MAJOR MASK(0, 4, 7)
#define G_MAJOR MASK(7, 11, 2)
#define F_MAJOR MASK(5, 9, 0)

static CommonPracticeMember note(const char *id, int pitch, bool held, double onset, const char *voice) {
    CommonPracticeMember m;
    memset(&m, 0, sizeof(m));
    m.note_id = id;
    m.pitch = pitch;
    m.held = held;
    m.duration = 0.25;
    m.strength = BEAT_STRENGTH_WEAK;
    m.voice_id = voice;
    m.onset = onset;
    return m;
}

typedef struct {
    CommonPracticeMember members[6];
    size_t count;
} Members;

static CommonPracticeSegment segment_of(const Members *m) {
    CommonPracticeSegment s;
    s.members = m->members;
    s.member_count = m->count;
    s.id = "segment";
    s.beat = 0;
    return s;
}

static CommonPracticeToneLabel *label(const CommonPracticeSegment *segments, const unsigned *masks, size_t count,
                                      size_t index, size_t *out_count) {
    CommonPracticeToneLabel *labels = NULL;
    assert(common_practice_label_leftover_tones(segments, masks, count, index, &labels, out_count) ==
           COMMON_PRACTICE_TONES_OK);
    return labels;
}

static void assert_label(const CommonPracticeToneLabel *l, size_t member_index, bool classified,
                         NonChordTone tone) {
    assert(l->member_index == member_index);
    assert(l->classified == classified);
    if (classified) {
        assert(l->tone == tone);
    }
}

/* Three segments of one voice "s" playing a, b, c under C major; the middle tone b is the leftover. */
static NonChordTone voice_figure(int a, int b, int c, bool expect_classified) {
    Members m0 = {{note("a", a, false, 0.0, "s")}, 1};
    Members m1 = {{note("b", b, false, 0.25, "s")}, 1};
    Members m2 = {{note("c", c, false, 0.5, "s")}, 1};
    CommonPracticeSegment segments[3] = {segment_of(&m0), segment_of(&m1), segment_of(&m2)};
    unsigned masks[3] = {C_MAJOR, C_MAJOR, C_MAJOR};
    size_t count = 0;
    CommonPracticeToneLabel *labels = label(segments, masks, 3, 1, &count);
    assert(count == 1);
    assert(labels[0].classified == expect_classified);
    NonChordTone tone = labels[0].tone;
    free(labels);
    return tone;
}

static void test_passing_tones(void) {
    assert(voice_figure(64, 65, 67, true) == NON_CHORD_TONE_PASSING); /* E F G, up */
    assert(voice_figure(67, 65, 64, true) == NON_CHORD_TONE_PASSING); /* G F E, down */
    assert(voice_figure(60, 62, 64, true) == NON_CHORD_TONE_PASSING); /* C D E: a step of 2 each way */
}

static void test_neighbor_tones(void) {
    assert(voice_figure(64, 65, 64, true) == NON_CHORD_TONE_NEIGHBOR); /* E F E, upper */
    assert(voice_figure(64, 62, 64, true) == NON_CHORD_TONE_NEIGHBOR); /* E D E, lower */
}

static void test_appoggiatura(void) {
    /* Leap up to F4 (C4 to F4, 5 semitones), then step down to E4. */
    assert(voice_figure(60, 65, 64, true) == NON_CHORD_TONE_APPOGGIATURA);
    /* Leap down to F4 from C5 and step up to G4: also an appoggiatura, in the other direction. */
    assert(voice_figure(72, 65, 67, true) == NON_CHORD_TONE_APPOGGIATURA);
}

static void test_escape_tone(void) {
    /* Step up to F4 from E4, then leap down to C4 (opposite to the step). */
    assert(voice_figure(64, 65, 60, true) == NON_CHORD_TONE_ESCAPE_TONE);
}

static void test_a_leap_out_in_the_same_direction_as_the_step_is_unclassified(void) {
    /* Step up to F4 and leap on up to A4: no NonChordTone value for it. */
    voice_figure(64, 65, 69, false);
}

static void test_a_repeated_pitch_and_a_double_leap_are_unclassified(void) {
    voice_figure(65, 65, 64, false); /* approached by a repeated note */
    voice_figure(60, 65, 72, false); /* leap in, leap out */
    voice_figure(64, 65, 65, false); /* left by a repeated note */
}

static void test_two_passing_tones_in_one_segment_are_both_labeled(void) {
    /* Two voices pass through the same beat: s moves G5 F5 E5 and a moves E4 F4 G4, under C major. */
    Members m0 = {{note("s0", 79, false, 0.0, "s"), note("a0", 64, false, 0.0, "a")}, 2};
    Members m1 = {{note("s1", 77, false, 0.25, "s"), note("a1", 65, false, 0.25, "a")}, 2};
    Members m2 = {{note("s2", 76, false, 0.5, "s"), note("a2", 67, false, 0.5, "a")}, 2};
    CommonPracticeSegment segments[3] = {segment_of(&m0), segment_of(&m1), segment_of(&m2)};
    unsigned masks[3] = {C_MAJOR, C_MAJOR, C_MAJOR};
    size_t count = 0;
    CommonPracticeToneLabel *labels = label(segments, masks, 3, 1, &count);
    assert(count == 2);
    assert_label(&labels[0], 0, true, NON_CHORD_TONE_PASSING);
    assert_label(&labels[1], 1, true, NON_CHORD_TONE_PASSING);
    free(labels);
}

static void test_suspension_4_3(void) {
    /* Over F major (F A C) the voice has F4; the chord changes to C and F4 is held, then falls to E4. */
    Members m0 = {{note("f", 65, false, 0.0, "s"), note("bass0", 53, false, 0.0, "b")}, 2};
    Members m1 = {{note("f", 65, true, 0.0, "s"), note("bass1", 48, false, 0.25, "b")}, 2};
    Members m2 = {{note("e", 64, false, 0.5, "s"), note("bass1", 48, true, 0.25, "b")}, 2};
    CommonPracticeSegment segments[3] = {segment_of(&m0), segment_of(&m1), segment_of(&m2)};
    unsigned masks[3] = {F_MAJOR, C_MAJOR, C_MAJOR};
    size_t count = 0;
    CommonPracticeToneLabel *labels = label(segments, masks, 3, 1, &count);
    assert(count == 1);
    assert_label(&labels[0], 0, true, NON_CHORD_TONE_SUSPENSION);
    free(labels);
}

static void test_a_held_tone_that_does_not_fall_is_unclassified(void) {
    /* Held F4 that rises to G4 would be a retardation, which is not labeled. */
    Members m0 = {{note("f", 65, false, 0.0, "s")}, 1};
    Members m1 = {{note("f", 65, true, 0.0, "s")}, 1};
    Members m2 = {{note("g", 67, false, 0.5, "s")}, 1};
    CommonPracticeSegment segments[3] = {segment_of(&m0), segment_of(&m1), segment_of(&m2)};
    unsigned masks[3] = {F_MAJOR, C_MAJOR, C_MAJOR};
    size_t count = 0;
    CommonPracticeToneLabel *labels = label(segments, masks, 3, 1, &count);
    assert(count == 1);
    assert_label(&labels[0], 0, false, NON_CHORD_TONE_PASSING);
    free(labels);
}

static void test_anticipation(void) {
    /* Under C major the voice sounds B4 early, and it sounds again in the next segment, where it is a
       tone of G major. */
    Members m0 = {{note("b0", 71, false, 0.0, "s")}, 1};
    Members m1 = {{note("b1", 71, false, 0.25, "s")}, 1};
    CommonPracticeSegment segments[2] = {segment_of(&m0), segment_of(&m1)};
    unsigned masks[2] = {C_MAJOR, G_MAJOR};
    size_t count = 0;
    CommonPracticeToneLabel *labels = label(segments, masks, 2, 0, &count);
    assert(count == 1);
    assert_label(&labels[0], 0, true, NON_CHORD_TONE_ANTICIPATION);
    free(labels);

    /* B4 is not a tone of the next chord (C major again): no anticipation. */
    unsigned same[2] = {C_MAJOR, C_MAJOR};
    labels = label(segments, same, 2, 0, &count);
    assert(count == 1);
    assert_label(&labels[0], 0, false, NON_CHORD_TONE_PASSING);
    free(labels);
}

static void test_pedal(void) {
    /* C3 is the bass of three segments under G major chords: a pedal in each. */
    Members m0 = {{note("pedal", 48, false, 0.0, "b"), note("t0", 67, false, 0.0, "s")}, 2};
    Members m1 = {{note("pedal", 48, true, 0.0, "b"), note("t1", 71, false, 0.25, "s")}, 2};
    Members m2 = {{note("pedal", 48, true, 0.0, "b"), note("t2", 74, false, 0.5, "s")}, 2};
    CommonPracticeSegment segments[3] = {segment_of(&m0), segment_of(&m1), segment_of(&m2)};
    unsigned masks[3] = {G_MAJOR, G_MAJOR, G_MAJOR};
    for (size_t i = 0; i < 3; i++) {
        size_t count = 0;
        CommonPracticeToneLabel *labels = label(segments, masks, 3, i, &count);
        assert(count == 1);
        assert_label(&labels[0], 0, true, NON_CHORD_TONE_PEDAL);
        free(labels);
    }

    /* Held through only two segments is not a pedal. */
    CommonPracticeSegment two[2] = {segments[0], segments[1]};
    unsigned two_masks[2] = {G_MAJOR, G_MAJOR};
    size_t count = 0;
    CommonPracticeToneLabel *labels = label(two, two_masks, 2, 0, &count);
    assert(count == 1);
    assert_label(&labels[0], 0, false, NON_CHORD_TONE_PASSING);
    free(labels);
}

static void test_without_a_voice_the_nearest_pitch_is_the_neighbor(void) {
    /* No voice ids: F4 sits between E4 (the nearest in the segment before) and G4 (the one attacked after)
       over a held C3: passing. */
    Members m0 = {{note("c3", 48, false, 0.0, NULL), note("e4", 64, false, 0.0, NULL)}, 2};
    Members m1 = {{note("c3", 48, true, 0.0, NULL), note("f4", 65, false, 0.25, NULL)}, 2};
    Members m2 = {{note("c3", 48, true, 0.0, NULL), note("g4", 67, false, 0.5, NULL)}, 2};
    CommonPracticeSegment segments[3] = {segment_of(&m0), segment_of(&m1), segment_of(&m2)};
    unsigned masks[3] = {C_MAJOR, C_MAJOR, C_MAJOR};
    size_t count = 0;
    CommonPracticeToneLabel *labels = label(segments, masks, 3, 1, &count);
    assert(count == 1);
    assert_label(&labels[0], 1, true, NON_CHORD_TONE_PASSING);
    free(labels);
}

static void test_without_a_voice_two_equally_near_neighbors_are_ambiguous(void) {
    /* F4 with D4 and A4 equally near before it (3 and 4... use E4 +/- : D4 is 3 away, G4... ): the tone is
       65; the previous segment has 63 and 67, both 2 away on opposite sides. */
    Members m0 = {{note("lo", 63, false, 0.0, NULL), note("hi", 67, false, 0.0, NULL)}, 2};
    Members m1 = {{note("t", 65, false, 0.25, NULL)}, 1};
    Members m2 = {{note("n", 64, false, 0.5, NULL)}, 1};
    CommonPracticeSegment segments[3] = {segment_of(&m0), segment_of(&m1), segment_of(&m2)};
    unsigned masks[3] = {C_MAJOR, C_MAJOR, C_MAJOR};
    size_t count = 0;
    CommonPracticeToneLabel *labels = label(segments, masks, 3, 1, &count);
    assert(count == 1);
    assert_label(&labels[0], 0, false, NON_CHORD_TONE_PASSING);
    free(labels);
}

static void test_a_tone_with_no_neighbors_is_unclassified(void) {
    Members m0 = {{note("only", 65, false, 0.0, "s")}, 1};
    CommonPracticeSegment segments[1] = {segment_of(&m0)};
    unsigned masks[1] = {C_MAJOR};
    size_t count = 0;
    CommonPracticeToneLabel *labels = label(segments, masks, 1, 0, &count);
    assert(count == 1);
    assert_label(&labels[0], 0, false, NON_CHORD_TONE_PASSING);
    free(labels);
}

static void test_chord_tones_are_not_labeled_and_a_segment_with_no_chord_has_none(void) {
    Members m0 = {{note("c", 60, false, 0.0, "s"), note("e", 64, false, 0.0, "a")}, 2};
    CommonPracticeSegment segments[1] = {segment_of(&m0)};
    unsigned masks[1] = {C_MAJOR};
    size_t count = 7;
    CommonPracticeToneLabel *labels = label(segments, masks, 1, 0, &count);
    assert(count == 0 && labels == NULL);

    unsigned no_chord[1] = {0};
    labels = label(segments, no_chord, 1, 0, &count);
    assert(count == 0 && labels == NULL);
}

static void test_rejects_bad_input(void) {
    Members m0 = {{note("c", 60, false, 0.0, "s")}, 1};
    CommonPracticeSegment segments[1] = {segment_of(&m0)};
    unsigned masks[1] = {C_MAJOR};
    CommonPracticeToneLabel *labels = NULL;
    size_t count = 0;
    assert(common_practice_label_leftover_tones(NULL, masks, 1, 0, &labels, &count) ==
           COMMON_PRACTICE_TONES_ERR_NULL_ARGUMENT);
    assert(common_practice_label_leftover_tones(segments, NULL, 1, 0, &labels, &count) ==
           COMMON_PRACTICE_TONES_ERR_NULL_ARGUMENT);
    assert(common_practice_label_leftover_tones(segments, masks, 1, 1, &labels, &count) ==
           COMMON_PRACTICE_TONES_ERR_BAD_INDEX);
}

int main(void) {
    test_passing_tones();
    test_neighbor_tones();
    test_appoggiatura();
    test_escape_tone();
    test_a_leap_out_in_the_same_direction_as_the_step_is_unclassified();
    test_a_repeated_pitch_and_a_double_leap_are_unclassified();
    test_two_passing_tones_in_one_segment_are_both_labeled();
    test_suspension_4_3();
    test_a_held_tone_that_does_not_fall_is_unclassified();
    test_anticipation();
    test_pedal();
    test_without_a_voice_the_nearest_pitch_is_the_neighbor();
    test_without_a_voice_two_equally_near_neighbors_are_ambiguous();
    test_a_tone_with_no_neighbors_is_unclassified();
    test_chord_tones_are_not_labeled_and_a_segment_with_no_chord_has_none();
    test_rejects_bad_input();
    printf("test_non_chord_tones: ok\n");
    return 0;
}
