#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "harmony_embellishing_rules.h"
#include "harmony_rule_check.h"
#include "harmony_test_util.h"
#include "harmony_eval_check.h"

/* Every call to the group's rule_holds below also runs the rule through constraint_evaluate. */
HARMONY_EVAL_TABLE(g_eval, "embellishing", HARMONY_EMBELLISHING_RULE_COUNT);
#define harmony_embellishing_rule_holds(which, subject) \
    HARMONY_EVAL_HOLDS(g_eval, harmony_embellishing_constraint, harmony_embellishing_preference, harmony_embellishing_rule_holds, HarmonyGesture, which, subject)

#define EMB(name) HARMONY_EMBELLISHING_##name

#define S BEAT_STRENGTH_STRONG
#define M BEAT_STRENGTH_MEDIUM
#define W BEAT_STRENGTH_WEAK

/* Notes are diatonic steps (do is 0). `ends` says whether the first and last notes
   are chord tones and `middle` whether the embellishing note is. */
static HarmonyGesture gesture3(int a, int b, int c, BeatStrength ba, BeatStrength bb, BeatStrength bc, bool ends,
                               bool middle) {
    return (HarmonyGesture){3, {a, b, c}, {ends, middle, ends}, {ba, bb, bc}};
}

static HarmonyGesture gesture2(int a, int b, BeatStrength ba, BeatStrength bb, bool first, bool second) {
    return (HarmonyGesture){2, {a, b, 0}, {first, second, false}, {ba, bb, W}};
}

static bool holds(HarmonyEmbellishingRule rule, const HarmonyGesture *g) {
    return harmony_embellishing_rule_holds(rule, g);
}

static bool all_hold(const HarmonyGesture *g) {
    for (int i = 0; i < HARMONY_EMBELLISHING_RULE_COUNT; i++) {
        if (!holds((HarmonyEmbellishingRule)i, g)) {
            return false;
        }
    }
    return true;
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(harmony_embellishing_rule, harmony_embellishing_constraint,
                                    HARMONY_EMBELLISHING_RULE_COUNT);
    for (int i = 0; i < HARMONY_EMBELLISHING_RULE_COUNT; i++) {
        ConstraintType type = constraint_type(harmony_embellishing_constraint((HarmonyEmbellishingRule)i));
        assert(type == (i <= EMB(STRONGER_BEAT) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT));
    }
}

static void test_names(void) {
    const char *const expected[HARMONY_EMBELLISHING_RULE_COUNT] = {
        "classifiable",         "stronger-beat",         "chord-tone-ends",       "middle-not-chord-tone",
        "appoggiatura-strong-beat", "escape-tone-weak-beat", "anticipation-weak-beat", "left-downward"};
    for (int i = 0; i < HARMONY_EMBELLISHING_RULE_COUNT; i++) {
        assert(strcmp(harmony_embellishing_rule_name((HarmonyEmbellishingRule)i), expected[i]) == 0);
    }
    assert(harmony_embellishing_rule_name(EMB(RULE_COUNT)) == NULL);
}

static void test_typical_gestures_hold(void) {
    HarmonyGesture good[] = {
        gesture3(0, 1, 2, S, W, M, true, false),    /* passing tone */
        gesture3(2, 1, 0, S, W, S, true, false),    /* descending passing tone */
        gesture3(0, 1, 0, S, W, S, true, false),    /* upper neighbor */
        gesture3(1, 0, 1, S, W, S, true, false),    /* lower neighbor */
        gesture3(0, 3, 2, W, S, W, true, false),    /* appoggiatura, left downward, on a strong beat */
        gesture3(0, 1, -2, S, W, S, true, false),   /* escape tone, left downward, on a weak beat */
        gesture3(3, 3, 2, W, S, W, true, false),    /* suspension on a stronger beat */
        gesture3(3, 3, 4, W, S, W, true, false),    /* retardation on a stronger beat */
        gesture3(2, 2, 2, S, M, S, true, false),    /* pedal tone */
        gesture2(4, 4, W, S, false, true),          /* anticipation on a weaker beat */
    };
    for (size_t i = 0; i < COUNT_OF(good); i++) {
        assert(harmony_gesture_valid(&good[i]));
        assert(all_hold(&good[i]));
        unsigned hard = 99, soft = 99;
        assert(harmony_embellishing_check(&good[i], &hard, &soft));
        assert(hard == 0 && soft == 0);
    }
}

static void test_classifiable(void) {
    HarmonyGesture leaps = gesture3(0, 2, 4, S, W, S, true, false);
    HarmonyGesture two_notes = gesture2(0, 1, W, S, false, true);
    assert(!holds(EMB(CLASSIFIABLE), &leaps));
    assert(!holds(EMB(CLASSIFIABLE), &two_notes));
    unsigned hard = 0, soft = 99;
    assert(harmony_embellishing_check(&leaps, &hard, &soft));
    assert(hard == (1u << EMB(CLASSIFIABLE)));
    assert(soft == 0);
}

static void test_suspension_and_retardation_need_a_stronger_beat(void) {
    /* Stronger than both the note before and the note after. */
    HarmonyGesture equal = gesture3(3, 3, 2, W, W, W, true, false);
    HarmonyGesture stronger_than_one = gesture3(3, 3, 2, W, M, S, true, false);
    HarmonyGesture weaker = gesture3(3, 3, 2, S, W, W, true, false);
    HarmonyGesture retardation_equal = gesture3(3, 3, 4, M, M, M, true, false);
    HarmonyGesture retardation_ok = gesture3(3, 3, 4, W, M, W, true, false);
    assert(!holds(EMB(STRONGER_BEAT), &equal));
    assert(!holds(EMB(STRONGER_BEAT), &stronger_than_one));
    assert(!holds(EMB(STRONGER_BEAT), &weaker));
    assert(!holds(EMB(STRONGER_BEAT), &retardation_equal));
    assert(holds(EMB(STRONGER_BEAT), &retardation_ok));

    /* It is a hard rule, and the only one broken. */
    unsigned hard = 0, soft = 99;
    assert(harmony_embellishing_check(&equal, &hard, &soft));
    assert(hard == (1u << EMB(STRONGER_BEAT)) && soft == 0);

    /* It says nothing of the other tones. */
    HarmonyGesture passing = gesture3(0, 1, 2, W, W, W, true, false);
    assert(holds(EMB(STRONGER_BEAT), &passing));
}

static void test_chord_tone_ends(void) {
    HarmonyGesture ok = gesture3(0, 1, 2, S, W, M, true, false);
    HarmonyGesture first_not = ok;
    first_not.in_chord[0] = false;
    HarmonyGesture last_not = ok;
    last_not.in_chord[2] = false;
    assert(holds(EMB(CHORD_TONE_ENDS), &ok));
    assert(!holds(EMB(CHORD_TONE_ENDS), &first_not));
    assert(!holds(EMB(CHORD_TONE_ENDS), &last_not));
    /* In an anticipation the second note is the chord tone. */
    HarmonyGesture anticipation = gesture2(4, 4, W, S, false, true);
    HarmonyGesture arrival_not = gesture2(4, 4, W, S, false, false);
    assert(holds(EMB(CHORD_TONE_ENDS), &anticipation));
    assert(!holds(EMB(CHORD_TONE_ENDS), &arrival_not));

    /* Advice only: "nearly all" cases. */
    unsigned hard = 99, soft = 0;
    assert(harmony_embellishing_check(&first_not, &hard, &soft));
    assert(hard == 0 && soft == (1u << EMB(CHORD_TONE_ENDS)));
}

static void test_middle_is_usually_not_a_chord_tone(void) {
    HarmonyGesture chord_tone_middle = gesture3(0, 1, 2, S, W, M, true, true);
    assert(!holds(EMB(MIDDLE_NOT_CHORD_TONE), &chord_tone_middle));
    HarmonyGesture ok = gesture3(0, 1, 2, S, W, M, true, false);
    assert(holds(EMB(MIDDLE_NOT_CHORD_TONE), &ok));
    /* In an anticipation, the first note is the embellishing one. */
    HarmonyGesture first_is_chord_tone = gesture2(4, 4, W, S, true, true);
    assert(!holds(EMB(MIDDLE_NOT_CHORD_TONE), &first_is_chord_tone));
    HarmonyGesture anticipation = gesture2(4, 4, W, S, false, true);
    assert(holds(EMB(MIDDLE_NOT_CHORD_TONE), &anticipation));
}

static void test_appoggiatura_beat_and_direction(void) {
    HarmonyGesture weak_beat = gesture3(0, 3, 2, S, W, S, true, false);
    HarmonyGesture equal_beat = gesture3(0, 3, 2, M, M, M, true, false);
    HarmonyGesture good = gesture3(0, 3, 2, W, S, W, true, false);
    assert(!holds(EMB(APPOGGIATURA_STRONG_BEAT), &weak_beat));
    assert(!holds(EMB(APPOGGIATURA_STRONG_BEAT), &equal_beat));
    assert(holds(EMB(APPOGGIATURA_STRONG_BEAT), &good));
    assert(holds(EMB(LEFT_DOWNWARD), &good));
    /* Left upward is less common (leap down, then step up). */
    HarmonyGesture upward = gesture3(4, 0, 1, W, S, W, true, false);
    assert(holds(EMB(APPOGGIATURA_STRONG_BEAT), &upward));
    assert(!holds(EMB(LEFT_DOWNWARD), &upward));
    /* Soft: the appoggiatura on a weak beat breaks nothing hard. */
    unsigned hard = 99, soft = 0;
    assert(harmony_embellishing_check(&weak_beat, &hard, &soft));
    assert(hard == 0 && soft == (1u << EMB(APPOGGIATURA_STRONG_BEAT)));
}

static void test_escape_tone_beat_and_direction(void) {
    HarmonyGesture strong_beat = gesture3(0, 1, -2, W, S, W, true, false);
    HarmonyGesture good = gesture3(0, 1, -2, S, W, S, true, false);
    assert(!holds(EMB(ESCAPE_TONE_WEAK_BEAT), &strong_beat));
    assert(holds(EMB(ESCAPE_TONE_WEAK_BEAT), &good));
    assert(holds(EMB(LEFT_DOWNWARD), &good));
    /* Left upward is less common (step down, leap up). */
    HarmonyGesture upward = gesture3(1, 0, 3, S, W, S, true, false);
    assert(holds(EMB(ESCAPE_TONE_WEAK_BEAT), &upward));
    assert(!holds(EMB(LEFT_DOWNWARD), &upward));
}

static void test_anticipation_beat(void) {
    HarmonyGesture good = gesture2(4, 4, W, S, false, true);
    HarmonyGesture same = gesture2(4, 4, M, M, false, true);
    HarmonyGesture stronger_first = gesture2(4, 4, S, W, false, true);
    assert(holds(EMB(ANTICIPATION_WEAK_BEAT), &good));
    assert(!holds(EMB(ANTICIPATION_WEAK_BEAT), &same));
    assert(!holds(EMB(ANTICIPATION_WEAK_BEAT), &stronger_first));
}

static void test_rules_about_one_tone_ignore_the_others(void) {
    /* A passing tone on a strong beat, left upward: none of the beat or direction
       rules of other tones apply. */
    HarmonyGesture passing = gesture3(0, 1, 2, W, S, W, true, false);
    assert(holds(EMB(APPOGGIATURA_STRONG_BEAT), &passing));
    assert(holds(EMB(ESCAPE_TONE_WEAK_BEAT), &passing));
    assert(holds(EMB(ANTICIPATION_WEAK_BEAT), &passing));
    assert(holds(EMB(LEFT_DOWNWARD), &passing));
    assert(holds(EMB(STRONGER_BEAT), &passing));
}

static void test_invalid_gesture(void) {
    HarmonyGesture bad_count = gesture3(0, 1, 2, S, W, M, true, false);
    bad_count.note_count = 4;
    HarmonyGesture bad_beat = gesture3(0, 1, 2, S, W, M, true, false);
    bad_beat.beat[0] = BEAT_STRENGTH_COUNT;
    HarmonyGesture *bad[] = {&bad_count, &bad_beat};
    unsigned hard = 7, soft = 7;
    for (size_t i = 0; i < COUNT_OF(bad); i++) {
        for (int rule = 0; rule < HARMONY_EMBELLISHING_RULE_COUNT; rule++) {
            assert(!holds((HarmonyEmbellishingRule)rule, bad[i]));
        }
        assert(!harmony_embellishing_check(bad[i], &hard, &soft));
    }
    assert(hard == 7 && soft == 7);
    HarmonyGesture ok = gesture3(0, 1, 2, S, W, M, true, false);
    assert(!holds(EMB(RULE_COUNT), &ok));
    assert(!harmony_embellishing_check(&ok, NULL, &soft));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_typical_gestures_hold();
    test_classifiable();
    test_suspension_and_retardation_need_a_stronger_beat();
    test_chord_tone_ends();
    test_middle_is_usually_not_a_chord_tone();
    test_appoggiatura_beat_and_direction();
    test_escape_tone_beat_and_direction();
    test_anticipation_beat();
    test_rules_about_one_tone_ignore_the_others();
    test_invalid_gesture();
    printf("test_harmony_embellishing_rules: ok\n");
    return 0;
}
