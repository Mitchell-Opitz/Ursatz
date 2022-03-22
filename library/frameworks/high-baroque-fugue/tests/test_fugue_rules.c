#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "fugue_rules.h"
#include "fugue_test_util.h"

static void test_group_names(void) {
    assert(strcmp(fugue_group_name(FUGUE_GROUP_ENTRIES), "entries") == 0);
    assert(strcmp(fugue_group_name(FUGUE_GROUP_ANSWER), "answer") == 0);
    assert(strcmp(fugue_group_name(FUGUE_GROUP_COUNTERSUBJECT), "countersubject") == 0);
    assert(fugue_group_name(FUGUE_GROUP_COUNT) == NULL);
    assert(fugue_group_name((FugueGroup)-1) == NULL);
}

static void test_good_exposition(void) {
    FOUR_VOICE_FUGUE(fugue);
    /* An exposition in four voices, ASTB, with real answers and two
       countersubjects: it breaks no rule. */
    FugueResult result;
    assert(fugue_check(&fugue, &result));
    assert(result.hard_violations == 0 && result.soft_violations == 0);
    assert(fugue_valid(&fugue));
}

static void test_each_group_reports(void) {
    /* The first entry is an answer: the entries group breaks a hard rule. */
    FOUR_VOICE_FUGUE(entries);
    entries_statements[0].role = FUGUE_ROLE_ANSWER;
    FugueResult result;
    assert(fugue_check(&entries, &result));
    assert(result.hard_violations & (1u << FUGUE_GROUP_ENTRIES));
    assert(!fugue_valid(&entries));

    /* The bass answers on A, a sixth from the subject. Only the answer group
       breaks a hard rule. */
    FOUR_VOICE_FUGUE(answer);
    const PitchSpelling *a2 = answer_a2;
    for (int i = 48; i < 52; i++) {
        answer_bass[i].pitch = a2;
    }
    assert(fugue_check(&answer, &result));
    assert(result.hard_violations == (1u << FUGUE_GROUP_ANSWER));

    /* A countersubject of the wrong length breaks the countersubject group. */
    FOUR_VOICE_FUGUE(countersubject);
    countersubject_statements[6].note_count = 7;
    assert(fugue_check(&countersubject, &result));
    assert(result.hard_violations == (1u << FUGUE_GROUP_COUNTERSUBJECT));
}

static void test_soft_violations_do_not_invalidate(void) {
    FOUR_VOICE_FUGUE(fugue);
    /* The countersubject is declared invertible, but fifths appear against the
       tenor's subject: advice only. */
    fugue.invertible = true;
    FugueResult result;
    assert(fugue_check(&fugue, &result));
    assert(result.hard_violations == 0);
    assert(result.soft_violations & (1u << FUGUE_GROUP_COUNTERSUBJECT));
    assert(fugue_valid(&fugue));
}

static void test_invalid_input(void) {
    FOUR_VOICE_FUGUE(fugue);
    FugueResult result = {7, 7};
    assert(!fugue_check(NULL, &result));
    assert(!fugue_check(&fugue, NULL));
    assert(!fugue_valid(NULL));

    FugueExposition one_voice = fugue;
    one_voice.voice_count = 1;
    assert(!fugue_check(&one_voice, &result));
    assert(!fugue_valid(&one_voice));
    assert(result.hard_violations == 7 && result.soft_violations == 7);
}

int main(void) {
    test_group_names();
    test_good_exposition();
    test_each_group_reports();
    test_soft_violations_do_not_invalidate();
    test_invalid_input();
    printf("test_fugue_rules: ok\n");
    return 0;
}
