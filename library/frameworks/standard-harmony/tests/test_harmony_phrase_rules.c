#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "harmony_phrase_rules.h"
#include "harmony_rule_check.h"
#include "harmony_test_util.h"

#define PHRASE(name) HARMONY_PHRASE_##name

static HarmonyPhrase phrase_of(HarmonyMode mode, const HarmonyChord *chords, size_t count) {
    return (HarmonyPhrase){mode, chords, count};
}

static bool holds(HarmonyPhraseRule rule, HarmonyMode mode, const HarmonyChord *chords, size_t count) {
    HarmonyPhrase phrase = phrase_of(mode, chords, count);
    return harmony_phrase_rule_holds(rule, &phrase);
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(harmony_phrase_rule, harmony_phrase_constraint, HARMONY_PHRASE_RULE_COUNT);
    for (int i = 0; i < HARMONY_PHRASE_RULE_COUNT; i++) {
        ConstraintType type = constraint_type(harmony_phrase_constraint((HarmonyPhraseRule)i));
        bool hard = i == PHRASE(TONIC_THEN_DOMINANT) || i == PHRASE(IV_BEFORE_II);
        assert(type == (hard ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT));
    }
}

static void test_names(void) {
    assert(strcmp(harmony_phrase_rule_name(PHRASE(TONIC_THEN_DOMINANT)), "tonic-then-dominant") == 0);
    assert(strcmp(harmony_phrase_rule_name(PHRASE(NO_BACKWARD_MOTION)), "no-backward-motion") == 0);
    assert(strcmp(harmony_phrase_rule_name(PHRASE(WEAK_PREDOMINANT_LEADS_ON)), "weak-predominant-leads-on") == 0);
    assert(strcmp(harmony_phrase_rule_name(PHRASE(IV_BEFORE_II)), "iv-before-ii") == 0);
    assert(harmony_phrase_rule_name(PHRASE(RULE_COUNT)) == NULL);
}

static void test_phrase_model_layouts_hold(void) {
    /* (a) the minimum: tonic, dominant. (b) the common layout, with and without
       the final tonic. */
    const HarmonyChord minimum[] = {CH(1), CH(5)};
    const HarmonyChord common[] = {CH(1), CH(4), CH(5), CH(1)};
    const HarmonyChord no_final_tonic[] = {CH(1), CH(2), CH(5)};
    const HarmonyChord weak_then_strong[] = {CH(1), CH(6), CH(4), CH(5), CH(1)};
    for (int mode = 0; mode < HARMONY_MODE_COUNT; mode++) {
        for (int i = 0; i < HARMONY_PHRASE_RULE_COUNT; i++) {
            assert(holds((HarmonyPhraseRule)i, (HarmonyMode)mode, minimum, COUNT_OF(minimum)));
            assert(holds((HarmonyPhraseRule)i, (HarmonyMode)mode, common, COUNT_OF(common)));
            assert(holds((HarmonyPhraseRule)i, (HarmonyMode)mode, no_final_tonic, COUNT_OF(no_final_tonic)));
            assert(holds((HarmonyPhraseRule)i, (HarmonyMode)mode, weak_then_strong, COUNT_OF(weak_then_strong)));
        }
    }
}

static void test_tonic_then_dominant(void) {
    const HarmonyChord no_dominant[] = {CH(1), CH(4), CH(1)};
    const HarmonyChord no_tonic[] = {CH(4), CH(5)};
    const HarmonyChord dominant_first[] = {CH(5), CH(1)};
    const HarmonyChord tonic_in_the_middle[] = {CH(4), CH(1), CH(5)};
    const HarmonyChord leading_tone_is_dominant[] = {CH(1), LEADING_TONE_CHORD};
    assert(!holds(PHRASE(TONIC_THEN_DOMINANT), HARMONY_MODE_MAJOR, no_dominant, COUNT_OF(no_dominant)));
    assert(!holds(PHRASE(TONIC_THEN_DOMINANT), HARMONY_MODE_MAJOR, no_tonic, COUNT_OF(no_tonic)));
    assert(!holds(PHRASE(TONIC_THEN_DOMINANT), HARMONY_MODE_MAJOR, dominant_first, COUNT_OF(dominant_first)));
    assert(holds(PHRASE(TONIC_THEN_DOMINANT), HARMONY_MODE_MAJOR, tonic_in_the_middle, COUNT_OF(tonic_in_the_middle)));
    assert(holds(PHRASE(TONIC_THEN_DOMINANT), HARMONY_MODE_MINOR, leading_tone_is_dominant,
                 COUNT_OF(leading_tone_is_dominant)));
    /* In minor, VII (no leading-tone mark) is a predominant, not a dominant. */
    const HarmonyChord subtonic[] = {CH(1), CH(7)};
    assert(!holds(PHRASE(TONIC_THEN_DOMINANT), HARMONY_MODE_MINOR, subtonic, COUNT_OF(subtonic)));
}

static void test_no_backward_motion(void) {
    const HarmonyChord dominant_to_predominant[] = {CH(1), CH(5), CH(4)};
    const HarmonyChord predominant_to_tonic[] = {CH(1), CH(4), CH(1), CH(5)};
    const HarmonyChord dominant_to_tonic[] = {CH(1), CH(5), CH(1)};
    const HarmonyChord staying[] = {CH(1), CH(1), CH(4), CH(2), CH(5), CH(5)};
    assert(!holds(PHRASE(NO_BACKWARD_MOTION), HARMONY_MODE_MAJOR, dominant_to_predominant,
                  COUNT_OF(dominant_to_predominant)));
    assert(!holds(PHRASE(NO_BACKWARD_MOTION), HARMONY_MODE_MAJOR, predominant_to_tonic,
                  COUNT_OF(predominant_to_tonic)));
    assert(holds(PHRASE(NO_BACKWARD_MOTION), HARMONY_MODE_MAJOR, dominant_to_tonic, COUNT_OF(dominant_to_tonic)));
    assert(holds(PHRASE(NO_BACKWARD_MOTION), HARMONY_MODE_MAJOR, staying, COUNT_OF(staying)));
}

static void test_weak_predominant_leads_on(void) {
    const HarmonyChord weak_to_dominant[] = {CH(1), CH(6), CH(5)};
    const HarmonyChord weak_to_weak[] = {CH(1), CH(3), CH(6), CH(4), CH(5)};
    const HarmonyChord weak_ends_phrase[] = {CH(1), CH(6)};
    const HarmonyChord strong_to_dominant[] = {CH(1), CH(4), CH(5)};
    assert(!holds(PHRASE(WEAK_PREDOMINANT_LEADS_ON), HARMONY_MODE_MAJOR, weak_to_dominant,
                  COUNT_OF(weak_to_dominant)));
    assert(holds(PHRASE(WEAK_PREDOMINANT_LEADS_ON), HARMONY_MODE_MAJOR, weak_to_weak, COUNT_OF(weak_to_weak)));
    assert(holds(PHRASE(WEAK_PREDOMINANT_LEADS_ON), HARMONY_MODE_MAJOR, weak_ends_phrase,
                 COUNT_OF(weak_ends_phrase)));
    assert(holds(PHRASE(WEAK_PREDOMINANT_LEADS_ON), HARMONY_MODE_MAJOR, strong_to_dominant,
                 COUNT_OF(strong_to_dominant)));
    /* In minor, VII is a weak predominant too. */
    const HarmonyChord subtonic_to_dominant[] = {CH(1), CH(7), CH(5)};
    assert(!holds(PHRASE(WEAK_PREDOMINANT_LEADS_ON), HARMONY_MODE_MINOR, subtonic_to_dominant,
                  COUNT_OF(subtonic_to_dominant)));
}

static void test_iv_before_ii(void) {
    /* IV then ii (or ii6) is the order; ii then IV is not, in either mode. */
    for (int mode = 0; mode < HARMONY_MODE_COUNT; mode++) {
        HarmonyMode m = (HarmonyMode)mode;
        const HarmonyChord iv_then_ii[] = {CH(1), CH(4), CH(2), CH(5), CH(1)};
        const HarmonyChord iv_then_ii6[] = {CH(1), CH(4), CHI(2, 1), CH(5), CH(1)};
        const HarmonyChord ii_then_iv[] = {CH(1), CH(2), CH(4), CH(5), CH(1)};
        const HarmonyChord ii6_then_iv[] = {CH(1), CHI(2, 1), CH(4), CH(5), CH(1)};
        assert(holds(PHRASE(IV_BEFORE_II), m, iv_then_ii, COUNT_OF(iv_then_ii)));
        assert(holds(PHRASE(IV_BEFORE_II), m, iv_then_ii6, COUNT_OF(iv_then_ii6)));
        assert(!holds(PHRASE(IV_BEFORE_II), m, ii_then_iv, COUNT_OF(ii_then_iv)));
        assert(!holds(PHRASE(IV_BEFORE_II), m, ii6_then_iv, COUNT_OF(ii6_then_iv)));
    }
    /* Either alone, or the two in separate runs of predominants, break nothing. */
    const HarmonyChord only_iv[] = {CH(1), CH(4), CH(5)};
    const HarmonyChord only_ii[] = {CH(1), CH(2), CH(5)};
    const HarmonyChord separate_runs[] = {CH(1), CH(2), CH(5), CH(1), CH(4), CH(5)};
    assert(holds(PHRASE(IV_BEFORE_II), HARMONY_MODE_MAJOR, only_iv, COUNT_OF(only_iv)));
    assert(holds(PHRASE(IV_BEFORE_II), HARMONY_MODE_MAJOR, only_ii, COUNT_OF(only_ii)));
    assert(holds(PHRASE(IV_BEFORE_II), HARMONY_MODE_MAJOR, separate_runs, COUNT_OF(separate_runs)));
    /* A weak predominant in between does not end the run. */
    const HarmonyChord weak_between[] = {CH(1), CH(2), CH(6), CH(4), CH(5)};
    assert(!holds(PHRASE(IV_BEFORE_II), HARMONY_MODE_MAJOR, weak_between, COUNT_OF(weak_between)));

    /* It is a hard rule: ii then IV breaks it, and nothing else in the phrase. */
    const HarmonyChord ii_then_iv[] = {CH(1), CH(2), CH(4), CH(5)};
    HarmonyPhrase phrase = phrase_of(HARMONY_MODE_MAJOR, ii_then_iv, COUNT_OF(ii_then_iv));
    unsigned hard = 0, soft = 99;
    assert(harmony_phrase_check(&phrase, &hard, &soft));
    assert(hard == (1u << PHRASE(IV_BEFORE_II)) && soft == 0);
}

static void test_check_reports_hard_and_soft(void) {
    const HarmonyChord good[] = {CH(1), CH(4), CH(5), CH(1)};
    const HarmonyChord soft_only[] = {CH(1), CH(6), CH(5)};
    const HarmonyChord hard_and_soft[] = {CH(5), CH(4)};
    HarmonyPhrase phrase = phrase_of(HARMONY_MODE_MAJOR, good, COUNT_OF(good));
    unsigned hard = 99, soft = 99;
    assert(harmony_phrase_check(&phrase, &hard, &soft));
    assert(hard == 0 && soft == 0);

    phrase = phrase_of(HARMONY_MODE_MAJOR, soft_only, COUNT_OF(soft_only));
    assert(harmony_phrase_check(&phrase, &hard, &soft));
    assert(hard == 0 && soft == (1u << PHRASE(WEAK_PREDOMINANT_LEADS_ON)));

    phrase = phrase_of(HARMONY_MODE_MAJOR, hard_and_soft, COUNT_OF(hard_and_soft));
    assert(harmony_phrase_check(&phrase, &hard, &soft));
    assert(hard == (1u << PHRASE(TONIC_THEN_DOMINANT)) && soft == (1u << PHRASE(NO_BACKWARD_MOTION)));
}

static void test_invalid_phrase(void) {
    const HarmonyChord bad_chord[] = {CH(1), CH(9)};
    const HarmonyChord ok[] = {CH(1), CH(5)};
    HarmonyPhrase phrases[] = {
        phrase_of(HARMONY_MODE_COUNT, ok, COUNT_OF(ok)),
        phrase_of(HARMONY_MODE_MAJOR, NULL, 2),
        phrase_of(HARMONY_MODE_MAJOR, ok, 0),
        phrase_of(HARMONY_MODE_MAJOR, bad_chord, COUNT_OF(bad_chord)),
    };
    unsigned hard = 7, soft = 7;
    for (size_t i = 0; i < COUNT_OF(phrases); i++) {
        assert(!harmony_phrase_valid(&phrases[i]));
        for (int rule = 0; rule < HARMONY_PHRASE_RULE_COUNT; rule++) {
            assert(!harmony_phrase_rule_holds((HarmonyPhraseRule)rule, &phrases[i]));
        }
        assert(!harmony_phrase_check(&phrases[i], &hard, &soft));
    }
    assert(hard == 7 && soft == 7);
    assert(!harmony_phrase_valid(NULL));
    assert(!holds(PHRASE(RULE_COUNT), HARMONY_MODE_MAJOR, ok, COUNT_OF(ok)));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_phrase_model_layouts_hold();
    test_tonic_then_dominant();
    test_no_backward_motion();
    test_weak_predominant_leads_on();
    test_iv_before_ii();
    test_check_reports_hard_and_soft();
    test_invalid_phrase();
    printf("test_harmony_phrase_rules: ok\n");
    return 0;
}
