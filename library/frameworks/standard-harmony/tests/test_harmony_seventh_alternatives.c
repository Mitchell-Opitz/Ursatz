#include <assert.h>
#include <stdio.h>

#include "harmony_seventh_rules.h"
#include "harmony_test_util.h"

#define SEVENTH(name) HARMONY_SEVENTH_##name

#define SOL 5
#define TI 7
#define RE 2
#define FA 4
#define DO 1
#define MI 3

/* Degrees from the soprano to the bass for V7 and then I; V7 has the leading tone. */
static HarmonySeventhResolution resolution(HarmonyMode mode, int d0, int d1, int d2, int d3, int t0, int t1, int t2,
                                           int t3) {
    return (HarmonySeventhResolution){mode, {d0, d1, d2, d3}, {t0, t1, t2, t3}, true};
}

static bool holds(HarmonySeventhRule rule, const HarmonySeventhResolution *r) {
    return harmony_seventh_rule_holds(rule, r);
}

static bool all_hold(const HarmonySeventhResolution *r) {
    for (int i = 0; i < HARMONY_SEVENTH_RULE_COUNT; i++) {
        if (!holds((HarmonySeventhRule)i, r)) {
            return false;
        }
    }
    return true;
}

static SeventhResolution classify(const HarmonySeventhResolution *r) {
    SeventhResolution kind = (SeventhResolution)-1;
    assert(harmony_seventh_classify(r, &kind));
    return kind;
}

/* True when the tonic chord has the root, the third, and the fifth. */
static bool tonic_is_complete(const HarmonySeventhResolution *r) {
    bool have[8] = {false};
    for (int v = 0; v < VOICE_PART_COUNT; v++) {
        have[r->tonic[v]] = true;
    }
    return have[DO] && have[MI] && have[SOL];
}

static void test_incomplete_v7(void) {
    /* The fifth omitted, the root doubled, the doubled sol staying on sol: a
       complete tonic chord. */
    for (int mode = 0; mode < HARMONY_MODE_COUNT; mode++) {
        HarmonyMode m = (HarmonyMode)mode;
        HarmonySeventhResolution good[] = {
            resolution(m, TI, FA, SOL, SOL, DO, MI, SOL, DO),
            resolution(m, FA, SOL, TI, SOL, MI, SOL, DO, DO),
            resolution(m, SOL, FA, TI, SOL, SOL, MI, DO, DO),
        };
        for (size_t i = 0; i < COUNT_OF(good); i++) {
            assert(all_hold(&good[i]));
            assert(classify(&good[i]) == SEVENTH_RESOLUTION_INCOMPLETE);
            assert(tonic_is_complete(&good[i]));
            assert(seventh_resolution_gives_complete_tonic(classify(&good[i])));
        }
    }
}

static void test_only_the_root_can_be_doubled(void) {
    /* Doubling ti or fa (with the fifth omitted) would make parallel octaves. */
    HarmonySeventhResolution double_ti = resolution(HARMONY_MODE_MAJOR, TI, TI, FA, SOL, DO, DO, MI, DO);
    HarmonySeventhResolution double_fa = resolution(HARMONY_MODE_MAJOR, FA, FA, TI, SOL, MI, MI, DO, DO);
    HarmonySeventhResolution root_and_fifth = resolution(HARMONY_MODE_MAJOR, SOL, TI, RE, SOL, SOL, DO, DO, DO);
    assert(!holds(SEVENTH(V7_NOTES), &double_ti));
    assert(!holds(SEVENTH(V7_NOTES), &double_fa));
    assert(!holds(SEVENTH(V7_NOTES), &root_and_fifth));
    SeventhResolution kind = SEVENTH_RESOLUTION_DEFAULT;
    assert(!harmony_seventh_classify(&double_ti, &kind));
    assert(!harmony_seventh_classify(&double_fa, &kind));
}

static void test_leading_tone_drop(void) {
    /* ti in the alto or tenor of a complete V7 drops to sol: a complete tonic. */
    for (int mode = 0; mode < HARMONY_MODE_COUNT; mode++) {
        HarmonyMode m = (HarmonyMode)mode;
        HarmonySeventhResolution good[] = {
            resolution(m, RE, TI, FA, SOL, DO, SOL, MI, DO), /* alto */
            resolution(m, FA, RE, TI, SOL, MI, DO, SOL, DO), /* tenor */
        };
        for (size_t i = 0; i < COUNT_OF(good); i++) {
            assert(all_hold(&good[i]));
            assert(classify(&good[i]) == SEVENTH_RESOLUTION_LEADING_TONE_DROP);
            assert(tonic_is_complete(&good[i]));
            assert(seventh_resolution_gives_complete_tonic(classify(&good[i])));
        }
    }
}

static void test_leading_tone_drop_limits(void) {
    /* Not in the soprano, where it would be noticed. */
    HarmonySeventhResolution soprano = resolution(HARMONY_MODE_MAJOR, TI, RE, FA, SOL, SOL, DO, MI, DO);
    assert(!holds(SEVENTH(ACTIVE_NOTES_RESOLVE), &soprano));
    /* Never to mi, which is further away than sol. */
    HarmonySeventhResolution to_mi = resolution(HARMONY_MODE_MAJOR, RE, TI, FA, SOL, DO, MI, MI, DO);
    assert(!holds(SEVENTH(ACTIVE_NOTES_RESOLVE), &to_mi));
    /* Only from a complete V7: an incomplete one has no re, and drops no ti. */
    HarmonySeventhResolution incomplete = resolution(HARMONY_MODE_MAJOR, FA, TI, SOL, SOL, MI, SOL, SOL, DO);
    assert(!holds(SEVENTH(ACTIVE_NOTES_RESOLVE), &incomplete));
    /* The other active notes still resolve by their tendencies. */
    HarmonySeventhResolution fa_stays = resolution(HARMONY_MODE_MAJOR, RE, TI, FA, SOL, DO, SOL, SOL, DO);
    assert(!holds(SEVENTH(ACTIVE_NOTES_RESOLVE), &fa_stays));
}

static void test_default_is_classified_default(void) {
    HarmonySeventhResolution default_resolution = resolution(HARMONY_MODE_MAJOR, FA, TI, RE, SOL, MI, DO, DO, DO);
    assert(classify(&default_resolution) == SEVENTH_RESOLUTION_DEFAULT);
    assert(!tonic_is_complete(&default_resolution));
    assert(!seventh_resolution_gives_complete_tonic(classify(&default_resolution)));

    /* The avoided re to mi breaks no hard rule, so it is still the default. */
    HarmonySeventhResolution re_to_mi = resolution(HARMONY_MODE_MAJOR, FA, TI, RE, SOL, MI, DO, MI, DO);
    assert(classify(&re_to_mi) == SEVENTH_RESOLUTION_DEFAULT);
}

static void test_classify_refuses_a_broken_resolution(void) {
    SeventhResolution kind = SEVENTH_RESOLUTION_INCOMPLETE;
    HarmonySeventhResolution fa_up = resolution(HARMONY_MODE_MAJOR, FA, TI, RE, SOL, SOL, DO, DO, DO);
    HarmonySeventhResolution minor_te = resolution(HARMONY_MODE_MINOR, FA, TI, RE, SOL, MI, DO, DO, DO);
    minor_te.dominant_has_leading_tone = false;
    HarmonySeventhResolution invalid = resolution(HARMONY_MODE_MAJOR, 0, TI, RE, SOL, MI, DO, DO, DO);
    assert(!harmony_seventh_classify(&fa_up, &kind));
    assert(!harmony_seventh_classify(&minor_te, &kind));
    assert(!harmony_seventh_classify(&invalid, &kind));
    assert(!harmony_seventh_classify(NULL, &kind));
    assert(!harmony_seventh_classify(&invalid, NULL));
    assert(kind == SEVENTH_RESOLUTION_INCOMPLETE);
}

int main(void) {
    test_incomplete_v7();
    test_only_the_root_can_be_doubled();
    test_leading_tone_drop();
    test_leading_tone_drop_limits();
    test_default_is_classified_default();
    test_classify_refuses_a_broken_resolution();
    printf("test_harmony_seventh_alternatives: ok\n");
    return 0;
}
