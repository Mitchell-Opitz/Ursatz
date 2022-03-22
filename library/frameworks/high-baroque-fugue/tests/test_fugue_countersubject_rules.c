#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "fugue_countersubject_rules.h"
#include "fugue_rule_check.h"
#include "fugue_test_util.h"

#define COUNTERSUBJECT(name) FUGUE_COUNTERSUBJECT_##name

static bool holds(FugueCountersubjectRule rule, const FugueExposition *exposition) {
    return fugue_countersubject_rule_holds(rule, exposition);
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(fugue_countersubject_rule, fugue_countersubject_constraint,
                                    FUGUE_COUNTERSUBJECT_RULE_COUNT);
    for (int i = 0; i < FUGUE_COUNTERSUBJECT_RULE_COUNT; i++) {
        ConstraintType type = constraint_type(fugue_countersubject_constraint((FugueCountersubjectRule)i));
        assert(type == (i == COUNTERSUBJECT(RHYTHM_CONSTANT) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT));
    }
}

static void test_names(void) {
    assert(strcmp(fugue_countersubject_rule_name(COUNTERSUBJECT(RHYTHM_CONSTANT)), "rhythm-constant") == 0);
    assert(strcmp(fugue_countersubject_rule_name(COUNTERSUBJECT(INVERTIBLE_INTERVALS)), "invertible-intervals") ==
           0);
    assert(strcmp(fugue_countersubject_rule_name(COUNTERSUBJECT(COMPLEMENTARY_RHYTHM)), "complementary-rhythm") ==
           0);
    assert(strcmp(fugue_countersubject_rule_name(COUNTERSUBJECT(CHANGES_ONLY_WITH_ANSWER)),
                  "changes-only-with-answer") == 0);
    assert(fugue_countersubject_rule_name(COUNTERSUBJECT(RULE_COUNT)) == NULL);
}

static void test_good_exposition(void) {
    FOUR_VOICE_FUGUE(fugue);
    for (int i = 0; i < FUGUE_COUNTERSUBJECT_RULE_COUNT; i++) {
        assert(holds((FugueCountersubjectRule)i, &fugue));
    }
}

static void test_rhythm_constant(void) {
    FOUR_VOICE_FUGUE(fugue);
    /* The second statement of countersubject 2 has seven notes, not eight. */
    fugue_statements[6].note_count = 7;
    assert(!holds(COUNTERSUBJECT(RHYTHM_CONSTANT), &fugue));
    fugue_statements[6].note_count = 8;
    assert(holds(COUNTERSUBJECT(RHYTHM_CONSTANT), &fugue));
}

static void test_invertible_intervals(void) {
    FOUR_VOICE_FUGUE(fugue);

    /* Countersubject 1 against the answer is thirds and seconds only. */
    fugue.statement_count = 3;
    fugue.invertible = true;
    assert(holds(COUNTERSUBJECT(INVERTIBLE_INTERVALS), &fugue));

    /* A fourth against the answer: D below G, which would invert to a fifth. */
    fugue_alto[16].pitch = fugue_d4;
    fugue_alto[17].pitch = fugue_d4;
    assert(!holds(COUNTERSUBJECT(INVERTIBLE_INTERVALS), &fugue));

    /* The rule applies only to a countersubject declared invertible. */
    fugue.invertible = false;
    assert(holds(COUNTERSUBJECT(INVERTIBLE_INTERVALS), &fugue));
}

static void test_invertible_fifths(void) {
    FOUR_VOICE_FUGUE(fugue);
    /* Countersubject 2 in the soprano is a twelfth, a fifth, above the tenor's
       subject at the start. */
    fugue.invertible = true;
    assert(!holds(COUNTERSUBJECT(INVERTIBLE_INTERVALS), &fugue));
}

static void test_complementary_rhythm(void) {
    /* A subject and a countersubject that move together, in half notes. */
    const PitchSpelling *c4 = P(C, 0, 4);
    const PitchSpelling *d4 = P(D, 0, 4);
    const PitchSpelling *e4 = P(E, 0, 4);
    const PitchSpelling *a3 = P(A, 0, 3);
    const PitchSpelling *b3 = P(B, 0, 3);
    const PitchSpelling *c3 = P(C, 0, 3);
    StandardCounterpointSlot upper[] = {HALF(c4), HALF(d4), HALF(e4), HALF(c4)};
    StandardCounterpointSlot together[] = {HALF(a3), HALF(b3), HALF(c4), HALF(a3)};
    StandardCounterpointSlot apart[] = {QUARTER(a3), QUARTER(a3), QUARTER(b3), QUARTER(b3), QUARTER(c4),
                                        QUARTER(c4), QUARTER(a3), QUARTER(c3)};
    StandardCounterpointSlotLine together_voices[] = {LINE_OF(upper), LINE_OF(together)};
    StandardCounterpointSlotLine apart_voices[] = {LINE_OF(upper), LINE_OF(apart)};
    FugueStatement statements[] = {{0, 0, FUGUE_ROLE_SUBJECT, 4, 0}, {1, 0, FUGUE_ROLE_COUNTERSUBJECT, 4, 0}};
    EXPOSITION_OF(fugue_together, together_voices, statements);
    assert(fugue_exposition_valid(&fugue_together));
    assert(!holds(COUNTERSUBJECT(COMPLEMENTARY_RHYTHM), &fugue_together));

    FugueStatement apart_statements[] = {{0, 0, FUGUE_ROLE_SUBJECT, 4, 0}, {1, 0, FUGUE_ROLE_COUNTERSUBJECT, 8, 0}};
    EXPOSITION_OF(fugue_apart, apart_voices, apart_statements);
    assert(fugue_exposition_valid(&fugue_apart));
    assert(holds(COUNTERSUBJECT(COMPLEMENTARY_RHYTHM), &fugue_apart));
}

static void test_changes_only_with_answer(void) {
    FOUR_VOICE_FUGUE(fugue);
    /* Make the tenor's countersubject differ from the soprano's: a leap up to
       C3 where the soprano's stays by step. The answer is a real one, so no
       change is explained. */
    fugue_tenor[52].pitch = fugue_c3;
    fugue_tenor[53].pitch = fugue_c3;
    assert(!holds(COUNTERSUBJECT(CHANGES_ONLY_WITH_ANSWER), &fugue));
}

static void test_invalid_and_check(void) {
    FOUR_VOICE_FUGUE(fugue);
    assert(!holds(COUNTERSUBJECT(RULE_COUNT), &fugue));
    assert(!holds((FugueCountersubjectRule)-1, &fugue));
    assert(!holds(COUNTERSUBJECT(RHYTHM_CONSTANT), NULL));

    unsigned hard = 7, soft = 7;
    assert(!fugue_countersubject_check(NULL, &hard, &soft));
    assert(hard == 7 && soft == 7);
    assert(!fugue_countersubject_check(&fugue, NULL, &soft));
    assert(!fugue_countersubject_check(&fugue, &hard, NULL));

    assert(fugue_countersubject_check(&fugue, &hard, &soft));
    assert(hard == 0 && soft == 0);

    fugue_statements[6].note_count = 7;
    assert(fugue_countersubject_check(&fugue, &hard, &soft));
    assert(hard == (1u << COUNTERSUBJECT(RHYTHM_CONSTANT)));
    assert(soft & (1u << COUNTERSUBJECT(CHANGES_ONLY_WITH_ANSWER)));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_good_exposition();
    test_rhythm_constant();
    test_invertible_intervals();
    test_invertible_fifths();
    test_complementary_rhythm();
    test_changes_only_with_answer();
    test_invalid_and_check();
    printf("test_fugue_countersubject_rules: ok\n");
    return 0;
}
