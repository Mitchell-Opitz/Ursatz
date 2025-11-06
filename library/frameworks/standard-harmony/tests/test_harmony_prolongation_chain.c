#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "harmony_prolongation_chain.h"
#include "harmony_prolongation_test_util.h"
#include "harmony_rule_check.h"
#include "harmony_eval_check.h"

/* Every call to the group's rule_holds below also runs the rule through constraint_evaluate. */
HARMONY_EVAL_TABLE(g_eval, "chain", HARMONY_CHAIN_RULE_COUNT);
#define harmony_chain_rule_holds(which, subject) \
    HARMONY_EVAL_HOLDS(g_eval, harmony_chain_constraint, harmony_chain_preference, harmony_chain_rule_holds, HarmonyProlongationChain, which, subject)

#define CHAIN(name) HARMONY_CHAIN_##name

/* I V4/3 I6 that begins where example_v65 ends (the soprano on mi, the alto on do, the
   tenor on sol, the bass on do), so the two chain. */
static HarmonyTonicProlongation example_v43_after_v65(HarmonyMode mode) {
    static const int s0[4] = {2, 0, -3, -7};
    static const int s1[4] = {3, -1, -3, -6};
    static const int s2[4] = {2, 0, -3, -5};
    return make_prolongation(mode, I_ROOT, V43, I_FIRST, s0, s1, s2);
}

static HarmonyProlongationChain chain_of(const HarmonyTonicProlongation *links, size_t count) {
    HarmonyProlongationChain chain;
    memset(&chain, 0, sizeof(chain));
    chain.link_count = count;
    for (size_t i = 0; i < count; i++) {
        chain.links[i] = links[i];
    }
    return chain;
}

static bool holds(HarmonyChainRule rule, const HarmonyProlongationChain *chain) {
    return harmony_chain_rule_holds(rule, chain);
}

static bool all_hold(const HarmonyProlongationChain *chain) {
    for (int i = 0; i < HARMONY_CHAIN_RULE_COUNT; i++) {
        if (!holds((HarmonyChainRule)i, chain)) {
            return false;
        }
    }
    return true;
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(harmony_chain_rule, harmony_chain_constraint, HARMONY_CHAIN_RULE_COUNT);
    for (int i = 0; i < HARMONY_CHAIN_RULE_COUNT; i++) {
        assert(constraint_type(harmony_chain_constraint((HarmonyChainRule)i)) == CONSTRAINT_TYPE_HARD);
    }
    assert(strcmp(harmony_chain_rule_name(CHAIN(LINKS_VALID)), "links-valid") == 0);
    assert(strcmp(harmony_chain_rule_name(CHAIN(LINKED)), "linked") == 0);
    assert(strcmp(harmony_chain_rule_name(CHAIN(SAME_MODE)), "same-mode") == 0);
    assert(harmony_chain_rule_name(CHAIN(RULE_COUNT)) == NULL);
}

static void test_chains_of_progressions(void) {
    for (int mode = 0; mode < HARMONY_MODE_COUNT; mode++) {
        HarmonyMode m = (HarmonyMode)mode;
        /* do ti do, then do re mi. */
        HarmonyTonicProlongation two[] = {example_v65(m), example_v43_after_v65(m)};
        /* do re do, do ti do, then do re mi. */
        HarmonyTonicProlongation three[] = {example_v43_to_root(m), example_v65(m), example_v43_after_v65(m)};
        /* do ti do twice (V6/5, then the V6 triad), then do re mi. */
        HarmonyTonicProlongation with_triad[] = {example_v65(m), example_v6_triad(m), example_v43_after_v65(m)};
        HarmonyProlongationChain chains[] = {chain_of(two, 2), chain_of(three, 3), chain_of(with_triad, 3)};
        for (size_t i = 0; i < COUNT_OF(chains); i++) {
            assert(harmony_chain_valid(&chains[i]));
            assert(all_hold(&chains[i]));
            unsigned hard = 99, soft = 99;
            assert(harmony_chain_check(&chains[i], &hard, &soft));
            assert(hard == 0 && soft == 0);
        }
    }
}

static void test_a_single_progression_is_a_chain(void) {
    HarmonyTonicProlongation one[] = {example_v42(HARMONY_MODE_MAJOR)};
    HarmonyProlongationChain chain = chain_of(one, 1);
    assert(harmony_chain_valid(&chain));
    assert(all_hold(&chain));
}

static void test_links_must_be_valid(void) {
    /* The first progression has parallel octaves into its V. */
    HarmonyTonicProlongation bad = example_v65(HARMONY_MODE_MAJOR);
    bad.steps[1][VOICE_PART_TENOR] = -1;
    HarmonyTonicProlongation links[] = {bad, example_v43_after_v65(HARMONY_MODE_MAJOR)};
    HarmonyProlongationChain chain = chain_of(links, 2);
    assert(!holds(CHAIN(LINKS_VALID), &chain));
    assert(holds(CHAIN(LINKED), &chain));
    unsigned hard = 0, soft = 99;
    assert(harmony_chain_check(&chain, &hard, &soft));
    assert(hard == (1u << CHAIN(LINKS_VALID)) && soft == 0);
}

static void test_links_must_join(void) {
    HarmonyMode m = HARMONY_MODE_MAJOR;
    /* The next progression starts on a different note. */
    HarmonyTonicProlongation moved = example_v43_after_v65(m);
    moved.steps[0][VOICE_PART_ALTO] = 2;
    HarmonyTonicProlongation moved_links[] = {example_v65(m), moved};
    HarmonyProlongationChain moved_chain = chain_of(moved_links, 2);
    assert(!holds(CHAIN(LINKED), &moved_chain));

    /* On a different beat strength. */
    HarmonyTonicProlongation beat = example_v43_after_v65(m);
    beat.beat[0] = BEAT_STRENGTH_WEAK;
    HarmonyTonicProlongation beat_links[] = {example_v65(m), beat};
    HarmonyProlongationChain beat_chain = chain_of(beat_links, 2);
    assert(!holds(CHAIN(LINKED), &beat_chain));

    /* A progression that ends on I6 does not chain: every one begins on root position I. */
    HarmonyTonicProlongation ends_on_first[] = {example_v43_to_first(m), example_v43_after_v65(m)};
    HarmonyProlongationChain first_chain = chain_of(ends_on_first, 2);
    assert(!holds(CHAIN(LINKED), &first_chain));

    /* A different chord: the next progression's first chord is not the last chord. */
    HarmonyTonicProlongation other_chord = example_v43_after_v65(m);
    other_chord.chords[0] = CH7(1);
    HarmonyTonicProlongation chord_links[] = {example_v65(m), other_chord};
    HarmonyProlongationChain chord_chain = chain_of(chord_links, 2);
    assert(!holds(CHAIN(LINKED), &chord_chain));

    /* A progression that is not first or last is checked too. */
    HarmonyTonicProlongation first_then_gap[] = {example_v65(m), example_v43_after_v65(m), example_v65(m)};
    HarmonyProlongationChain gap_chain = chain_of(first_then_gap, 3);
    assert(!holds(CHAIN(LINKED), &gap_chain));
}

static void test_same_mode(void) {
    HarmonyTonicProlongation links[] = {example_v65(HARMONY_MODE_MAJOR), example_v43_after_v65(HARMONY_MODE_MINOR)};
    HarmonyProlongationChain chain = chain_of(links, 2);
    assert(!holds(CHAIN(SAME_MODE), &chain));
    assert(holds(CHAIN(LINKED), &chain));
    assert(holds(CHAIN(LINKS_VALID), &chain));
}

static void test_invalid_chain(void) {
    HarmonyProlongationChain empty;
    memset(&empty, 0, sizeof(empty));
    HarmonyProlongationChain too_long = chain_of((HarmonyTonicProlongation[]){example_v65(HARMONY_MODE_MAJOR)}, 1);
    too_long.link_count = HARMONY_PROLONGATION_CHAIN_MAX + 1;
    HarmonyTonicProlongation bad_link = example_v65(HARMONY_MODE_COUNT);
    HarmonyProlongationChain bad = chain_of(&bad_link, 1);
    HarmonyProlongationChain *invalid[] = {&empty, &too_long, &bad};
    unsigned hard = 7, soft = 7;
    for (size_t i = 0; i < COUNT_OF(invalid); i++) {
        assert(!harmony_chain_valid(invalid[i]));
        for (int rule = 0; rule < HARMONY_CHAIN_RULE_COUNT; rule++) {
            assert(!holds((HarmonyChainRule)rule, invalid[i]));
        }
        assert(!harmony_chain_check(invalid[i], &hard, &soft));
    }
    assert(hard == 7 && soft == 7);
    assert(!harmony_chain_valid(NULL));
    HarmonyTonicProlongation one[] = {example_v65(HARMONY_MODE_MAJOR)};
    HarmonyProlongationChain ok = chain_of(one, 1);
    assert(!holds(CHAIN(RULE_COUNT), &ok));
}

int main(void) {
    test_rules_reference_framework();
    test_chains_of_progressions();
    test_a_single_progression_is_a_chain();
    test_links_must_be_valid();
    test_links_must_join();
    test_same_mode();
    test_invalid_chain();
    printf("test_harmony_prolongation_chain: ok\n");
    return 0;
}
