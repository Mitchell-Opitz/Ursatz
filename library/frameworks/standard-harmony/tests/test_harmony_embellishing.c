#include <assert.h>
#include <stdio.h>

#include "harmony_embellishing.h"
#include "harmony_test_util.h"

#define S BEAT_STRENGTH_STRONG
#define M BEAT_STRENGTH_MEDIUM
#define W BEAT_STRENGTH_WEAK

/* Notes are diatonic steps (do is 0, re is 1, ti below do is -1). Classification
   looks only at the motion, so the beats and chord membership here are a fixed
   plain pattern. */
static HarmonyGesture gesture3(int a, int b, int c) {
    return (HarmonyGesture){3, {a, b, c}, {true, false, true}, {S, W, M}};
}

static HarmonyGesture gesture2(int a, int b) {
    return (HarmonyGesture){2, {a, b, 0}, {false, true, false}, {W, S, W}};
}

static NonChordTone classify(HarmonyGesture g) {
    NonChordTone tone = NON_CHORD_TONE_COUNT;
    assert(harmony_embellishing_classify(&g, &tone));
    return tone;
}

static bool classifies(HarmonyGesture g) {
    NonChordTone tone = NON_CHORD_TONE_PEDAL;
    return harmony_embellishing_classify(&g, &tone);
}

static void test_passing_tones(void) {
    assert(classify(gesture3(0, 1, 2)) == NON_CHORD_TONE_PASSING);   /* ascending */
    assert(classify(gesture3(2, 1, 0)) == NON_CHORD_TONE_PASSING);   /* descending */
    assert(classify(gesture3(-1, 0, 1)) == NON_CHORD_TONE_PASSING);  /* across do */
    assert(classify(gesture3(9, 8, 7)) == NON_CHORD_TONE_PASSING);
}

static void test_neighbor_tones(void) {
    assert(classify(gesture3(0, 1, 0)) == NON_CHORD_TONE_NEIGHBOR);   /* upper */
    assert(classify(gesture3(1, 0, 1)) == NON_CHORD_TONE_NEIGHBOR);   /* lower */
    assert(classify(gesture3(4, 3, 4)) == NON_CHORD_TONE_NEIGHBOR);
}

static void test_appoggiaturas(void) {
    /* Approached by leap, left by step in the opposite direction. */
    assert(classify(gesture3(0, 3, 2)) == NON_CHORD_TONE_APPOGGIATURA);  /* leap up, step down */
    assert(classify(gesture3(4, 0, 1)) == NON_CHORD_TONE_APPOGGIATURA);  /* leap down, step up */
    assert(classify(gesture3(0, 7, 6)) == NON_CHORD_TONE_APPOGGIATURA);  /* an octave leap */
    assert(classify(gesture3(0, 2, 1)) == NON_CHORD_TONE_APPOGGIATURA);  /* a leap of a third */
}

static void test_escape_tones(void) {
    /* Approached by step, left by leap in the opposite direction. */
    assert(classify(gesture3(0, 1, -2)) == NON_CHORD_TONE_ESCAPE_TONE);  /* step up, leap down */
    assert(classify(gesture3(1, 0, 3)) == NON_CHORD_TONE_ESCAPE_TONE);   /* step down, leap up */
    assert(classify(gesture3(0, 1, -1)) == NON_CHORD_TONE_ESCAPE_TONE);  /* a leap of a third */
}

static void test_static_note_tones(void) {
    assert(classify(gesture3(3, 3, 2)) == NON_CHORD_TONE_SUSPENSION);   /* held, then step down */
    assert(classify(gesture3(3, 3, 4)) == NON_CHORD_TONE_RETARDATION);  /* held, then step up */
    assert(classify(gesture3(2, 2, 2)) == NON_CHORD_TONE_PEDAL);        /* static, static */
    assert(classify(gesture2(4, 4)) == NON_CHORD_TONE_ANTICIPATION);    /* the same pitch twice */
    assert(classify(gesture2(-3, -3)) == NON_CHORD_TONE_ANTICIPATION);
}

static void test_other_motion_is_not_classified(void) {
    assert(!classifies(gesture3(0, 1, 3)));  /* step, then leap the same way */
    assert(!classifies(gesture3(0, 2, 3)));  /* leap, then step the same way */
    assert(!classifies(gesture3(0, 2, 4)));  /* two leaps the same way */
    assert(!classifies(gesture3(0, 3, 0)));  /* two leaps, opposite ways */
    assert(!classifies(gesture3(0, 0, 3)));  /* held, then leap up */
    assert(!classifies(gesture3(0, 0, -2))); /* held, then leap down */
    assert(!classifies(gesture2(0, 1)));     /* two different notes */
    assert(!classifies(gesture2(0, 3)));
}

static void test_beats_and_chord_tones_do_not_change_the_kind(void) {
    HarmonyGesture g = gesture3(0, 1, 2);
    g.in_chord[1] = true;
    g.in_chord[0] = false;
    g.beat[0] = W;
    g.beat[1] = S;
    g.beat[2] = W;
    assert(classify(g) == NON_CHORD_TONE_PASSING);
}

static void test_invalid_gesture(void) {
    NonChordTone tone = NON_CHORD_TONE_PEDAL;
    HarmonyGesture one = gesture3(0, 1, 2);
    one.note_count = 1;
    HarmonyGesture four = gesture3(0, 1, 2);
    four.note_count = 4;
    HarmonyGesture none = gesture3(0, 1, 2);
    none.note_count = 0;
    HarmonyGesture bad_beat = gesture3(0, 1, 2);
    bad_beat.beat[1] = BEAT_STRENGTH_COUNT;
    HarmonyGesture bad_beat_unused = gesture2(4, 4);
    bad_beat_unused.beat[2] = BEAT_STRENGTH_COUNT; /* a third beat is not read */
    HarmonyGesture *bad[] = {&one, &four, &none, &bad_beat};
    for (size_t i = 0; i < COUNT_OF(bad); i++) {
        assert(!harmony_gesture_valid(bad[i]));
        assert(!harmony_embellishing_classify(bad[i], &tone));
    }
    assert(tone == NON_CHORD_TONE_PEDAL);
    assert(harmony_gesture_valid(&bad_beat_unused));
    assert(!harmony_gesture_valid(NULL));
    assert(!harmony_embellishing_classify(NULL, &tone));
    HarmonyGesture ok = gesture3(0, 1, 2);
    assert(!harmony_embellishing_classify(&ok, NULL));
}

int main(void) {
    test_passing_tones();
    test_neighbor_tones();
    test_appoggiaturas();
    test_escape_tones();
    test_static_note_tones();
    test_other_motion_is_not_classified();
    test_beats_and_chord_tones_do_not_change_the_kind();
    test_invalid_gesture();
    printf("test_harmony_embellishing: ok\n");
    return 0;
}
