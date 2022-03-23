#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "fugue_answer_rules.h"
#include "fugue_rule_check.h"
#include "fugue_test_util.h"

#define ANSWER(name) FUGUE_ANSWER_##name

static bool holds(FugueAnswerRule rule, const FugueExposition *exposition) {
    return fugue_answer_rule_holds(rule, exposition);
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(fugue_answer_rule, fugue_answer_constraint, FUGUE_ANSWER_RULE_COUNT);
    for (int i = 0; i < FUGUE_ANSWER_RULE_COUNT; i++) {
        ConstraintType type = constraint_type(fugue_answer_constraint((FugueAnswerRule)i));
        assert(type == (i <= ANSWER(RHYTHM_PRESERVED) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT));
    }
}

static void test_names(void) {
    assert(strcmp(fugue_answer_rule_name(ANSWER(AT_FIFTH)), "answer-at-fifth") == 0);
    assert(strcmp(fugue_answer_rule_name(ANSWER(TONIC_DOMINANT_START)), "tonic-dominant-start") == 0);
    assert(strcmp(fugue_answer_rule_name(ANSWER(NOT_FREE)), "answer-not-free") == 0);
    assert(strcmp(fugue_answer_rule_name(ANSWER(RHYTHM_PRESERVED)), "rhythm-preserved") == 0);
    assert(strcmp(fugue_answer_rule_name(ANSWER(MINIMAL_ADJUSTMENT)), "minimal-adjustment") == 0);
    assert(fugue_answer_rule_name(ANSWER(RULE_COUNT)) == NULL);
}

static void test_good_exposition(void) {
    FOUR_VOICE_FUGUE(fugue);
    for (int i = 0; i < FUGUE_ANSWER_RULE_COUNT; i++) {
        assert(holds((FugueAnswerRule)i, &fugue));
    }
    ImitationAnswer kind;
    assert(fugue_answer_kind(&fugue, 1, &kind));
    assert(kind == IMITATION_ANSWER_REAL);
    assert(fugue_answer_kind(&fugue, 5, &kind));
    assert(kind == IMITATION_ANSWER_REAL);
}

static void test_answer_at_fifth(void) {
    const PitchSpelling *tonic = P(C, 0, 4);
    const PitchSpelling *subject[] = {P(C, 0, 4), P(D, 0, 4), P(E, 0, 4), P(C, 0, 4)};

    /* A fifth above, and a fourth below. */
    const PitchSpelling *fifth[] = {P(G, 0, 4), P(A, 0, 4), P(B, 0, 4), P(G, 0, 4)};
    AnswerFixture up;
    answer_fixture(&up, subject, fifth, 4, NULL, tonic);
    assert(holds(ANSWER(AT_FIFTH), &up.exposition));

    const PitchSpelling *fourth[] = {P(G, 0, 3), P(A, 0, 3), P(B, 0, 3), P(G, 0, 3)};
    AnswerFixture down;
    answer_fixture(&down, subject, fourth, 4, NULL, tonic);
    assert(holds(ANSWER(AT_FIFTH), &down.exposition));

    /* A third above is no answer. */
    const PitchSpelling *third[] = {P(E, 0, 4), P(F, 0, 4), P(G, 0, 4), P(E, 0, 4)};
    AnswerFixture bad;
    answer_fixture(&bad, subject, third, 4, NULL, tonic);
    assert(!holds(ANSWER(AT_FIFTH), &bad.exposition));

    /* Nor is an octave. */
    const PitchSpelling *octave[] = {P(C, 0, 5), P(D, 0, 5), P(E, 0, 5), P(C, 0, 5)};
    AnswerFixture same;
    answer_fixture(&same, subject, octave, 4, NULL, tonic);
    assert(!holds(ANSWER(AT_FIFTH), &same.exposition));
}

static void test_tonic_dominant_start(void) {
    const PitchSpelling *tonic = P(C, 0, 4);

    /* A subject on the tonic is answered on the dominant... */
    const PitchSpelling *on_tonic[] = {P(C, 0, 4), P(D, 0, 4), P(E, 0, 4), P(C, 0, 4)};
    const PitchSpelling *on_dominant[] = {P(G, 0, 4), P(A, 0, 4), P(B, 0, 4), P(G, 0, 4)};
    AnswerFixture good;
    answer_fixture(&good, on_tonic, on_dominant, 4, NULL, tonic);
    assert(holds(ANSWER(TONIC_DOMINANT_START), &good.exposition));

    /* ...not on the tonic again. */
    const PitchSpelling *octave[] = {P(C, 0, 5), P(D, 0, 5), P(E, 0, 5), P(C, 0, 5)};
    AnswerFixture again;
    answer_fixture(&again, on_tonic, octave, 4, NULL, tonic);
    assert(!holds(ANSWER(TONIC_DOMINANT_START), &again.exposition));

    /* A subject on sol is answered on do, not transposed up to re. */
    const PitchSpelling *on_sol[] = {P(G, 0, 4), P(A, 0, 4), P(B, 0, 4), P(C, 0, 5)};
    const PitchSpelling *on_do[] = {P(C, 0, 5), P(D, 0, 5), P(E, 0, 5), P(F, 0, 5)};
    AnswerFixture sol_do;
    answer_fixture(&sol_do, on_sol, on_do, 4, NULL, tonic);
    assert(holds(ANSWER(TONIC_DOMINANT_START), &sol_do.exposition));
    assert(holds(ANSWER(AT_FIFTH), &sol_do.exposition));

    const PitchSpelling *on_re[] = {P(D, 0, 5), P(E, 0, 5), P(F, 1, 5), P(G, 0, 5)};
    AnswerFixture sol_re;
    answer_fixture(&sol_re, on_sol, on_re, 4, NULL, tonic);
    assert(!holds(ANSWER(TONIC_DOMINANT_START), &sol_re.exposition));
    assert(holds(ANSWER(AT_FIFTH), &sol_re.exposition));

    /* Another degree, or an unknown tonic, is not judged. */
    const PitchSpelling *on_mi[] = {P(E, 0, 4), P(F, 0, 4), P(G, 0, 4), P(E, 0, 4)};
    const PitchSpelling *on_ti[] = {P(B, 0, 4), P(C, 0, 5), P(D, 0, 5), P(B, 0, 4)};
    AnswerFixture other;
    answer_fixture(&other, on_mi, on_ti, 4, NULL, tonic);
    assert(holds(ANSWER(TONIC_DOMINANT_START), &other.exposition));
    AnswerFixture unknown;
    answer_fixture(&unknown, on_tonic, octave, 4, NULL, NULL);
    assert(holds(ANSWER(TONIC_DOMINANT_START), &unknown.exposition));
}

static void test_tonal_answer(void) {
    const PitchSpelling *tonic = P(C, 0, 4);
    /* A fifth up in the subject is answered by a fourth up. */
    const PitchSpelling *subject[] = {P(C, 0, 4), P(G, 0, 4), P(A, 0, 4), P(G, 0, 4)};
    const PitchSpelling *tonal[] = {P(G, 0, 4), P(C, 0, 5), P(D, 0, 5), P(C, 0, 5)};
    AnswerFixture fugue;
    answer_fixture(&fugue, subject, tonal, 4, NULL, tonic);
    ImitationAnswer kind;
    assert(fugue_answer_kind(&fugue.exposition, 1, &kind));
    assert(kind == IMITATION_ANSWER_TONAL);
    assert(holds(ANSWER(AT_FIFTH), &fugue.exposition));
    assert(holds(ANSWER(TONIC_DOMINANT_START), &fugue.exposition));
    assert(holds(ANSWER(NOT_FREE), &fugue.exposition));
    assert(holds(ANSWER(MINIMAL_ADJUSTMENT), &fugue.exposition));

    /* One altered interval is too many for a limit of none. */
    FugueLimits strict = {0};
    fugue.exposition.limits = &strict;
    assert(!holds(ANSWER(MINIMAL_ADJUSTMENT), &fugue.exposition));
}

static void test_free_answer(void) {
    const PitchSpelling *tonic = P(C, 0, 4);
    const PitchSpelling *subject[] = {P(C, 0, 4), P(D, 0, 4), P(E, 0, 4), P(C, 0, 4)};
    const PitchSpelling *turned[] = {P(G, 0, 4), P(F, 0, 4), P(E, 0, 4), P(D, 0, 4)};
    AnswerFixture fugue;
    answer_fixture(&fugue, subject, turned, 4, NULL, tonic);
    ImitationAnswer kind;
    assert(fugue_answer_kind(&fugue.exposition, 1, &kind));
    assert(kind == IMITATION_ANSWER_FREE);
    assert(!holds(ANSWER(NOT_FREE), &fugue.exposition));
}

static void test_rhythm_preserved(void) {
    const PitchSpelling *tonic = P(C, 0, 4);
    const PitchSpelling *subject[] = {P(C, 0, 4), P(D, 0, 4), P(E, 0, 4), P(C, 0, 4)};
    const PitchSpelling *answer[] = {P(G, 0, 4), P(A, 0, 4), P(B, 0, 4), P(G, 0, 4)};
    const size_t same[] = {4, 4, 4, 4};
    AnswerFixture good;
    answer_fixture(&good, subject, answer, 4, same, tonic);
    assert(holds(ANSWER(RHYTHM_PRESERVED), &good.exposition));

    /* The second note of the answer is a whole note. */
    const size_t longer[] = {4, 8, 4, 4};
    AnswerFixture bad;
    answer_fixture(&bad, subject, answer, 4, longer, tonic);
    assert(!holds(ANSWER(RHYTHM_PRESERVED), &bad.exposition));
}

static void test_no_answer_and_no_subject(void) {
    FOUR_VOICE_FUGUE(fugue);

    /* A subject alone has nothing to answer it. */
    fugue.statement_count = 1;
    for (int i = 0; i < FUGUE_ANSWER_RULE_COUNT; i++) {
        assert(holds((FugueAnswerRule)i, &fugue));
    }
    fugue.statement_count = 7;

    /* An answer with no subject to answer. */
    fugue_statements[0].role = FUGUE_ROLE_FREE_COUNTERPOINT;
    fugue_statements[3].role = FUGUE_ROLE_FREE_COUNTERPOINT;
    assert(!holds(ANSWER(AT_FIFTH), &fugue));
    ImitationAnswer kind = IMITATION_ANSWER_FREE;
    assert(!fugue_answer_kind(&fugue, 1, &kind));
}

static void test_kind_arguments(void) {
    FOUR_VOICE_FUGUE(fugue);
    ImitationAnswer kind = IMITATION_ANSWER_FREE;
    assert(!fugue_answer_kind(&fugue, 0, &kind));
    assert(!fugue_answer_kind(&fugue, 2, &kind));
    assert(!fugue_answer_kind(&fugue, 7, &kind));
    assert(!fugue_answer_kind(&fugue, 1, NULL));
    assert(!fugue_answer_kind(NULL, 1, &kind));
    assert(kind == IMITATION_ANSWER_FREE);
}

static void test_invalid_and_check(void) {
    FOUR_VOICE_FUGUE(fugue);
    assert(!holds(ANSWER(RULE_COUNT), &fugue));
    assert(!holds((FugueAnswerRule)-1, &fugue));
    assert(!holds(ANSWER(AT_FIFTH), NULL));

    unsigned hard = 7, soft = 7;
    assert(!fugue_answer_check(NULL, &hard, &soft));
    assert(hard == 7 && soft == 7);
    assert(!fugue_answer_check(&fugue, NULL, &soft));
    assert(!fugue_answer_check(&fugue, &hard, NULL));

    assert(fugue_answer_check(&fugue, &hard, &soft));
    assert(hard == 0 && soft == 0);
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_good_exposition();
    test_answer_at_fifth();
    test_tonic_dominant_start();
    test_tonal_answer();
    test_free_answer();
    test_rhythm_preserved();
    test_no_answer_and_no_subject();
    test_kind_arguments();
    test_invalid_and_check();
    printf("test_fugue_answer_rules: ok\n");
    return 0;
}
