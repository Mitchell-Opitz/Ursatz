#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "fugue_entry_rules.h"
#include "fugue_rule_check.h"
#include "fugue_test_util.h"

#define ENTRY(name) FUGUE_ENTRY_##name

static bool holds(FugueEntryRule rule, const FugueExposition *exposition) {
    return fugue_entry_rule_holds(rule, exposition);
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(fugue_entry_rule, fugue_entry_constraint, FUGUE_ENTRY_RULE_COUNT);
    for (int i = 0; i < FUGUE_ENTRY_RULE_COUNT; i++) {
        ConstraintType type = constraint_type(fugue_entry_constraint((FugueEntryRule)i));
        assert(type == (i <= ENTRY(ENTRIES_ALIGNED) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT));
    }
}

static void test_names(void) {
    assert(strcmp(fugue_entry_rule_name(ENTRY(FIRST_IS_SUBJECT)), "first-is-subject") == 0);
    assert(strcmp(fugue_entry_rule_name(ENTRY(ENTRIES_ALTERNATE)), "entries-alternate") == 0);
    assert(strcmp(fugue_entry_rule_name(ENTRY(EACH_VOICE_ENTERS_ONCE)), "each-voice-enters-once") == 0);
    assert(strcmp(fugue_entry_rule_name(ENTRY(ENTRIES_ALIGNED)), "entries-aligned") == 0);
    assert(strcmp(fugue_entry_rule_name(ENTRY(NEW_VOICE_AT_EXTREME)), "new-voice-at-extreme") == 0);
    assert(strcmp(fugue_entry_rule_name(ENTRY(COUNTERSUBJECT_FOLLOWS)), "countersubject-follows") == 0);
    assert(fugue_entry_rule_name(ENTRY(RULE_COUNT)) == NULL);
}

static void test_good_exposition(void) {
    FOUR_VOICE_FUGUE(fugue);
    assert(fugue_exposition_valid(&fugue));
    for (int i = 0; i < FUGUE_ENTRY_RULE_COUNT; i++) {
        assert(holds((FugueEntryRule)i, &fugue));
    }
    assert(fugue_exposition_entry_count(&fugue) == 4);
    assert(fugue_exposition_redundant_entries(&fugue) == 0);
}

static void test_redundant_entries(void) {
    FOUR_VOICE_FUGUE(fugue);
    /* A fifth entry, in the alto again. */
    fugue.statement_count = 8;
    assert(fugue_exposition_valid(&fugue));
    assert(fugue_exposition_entry_count(&fugue) == 5);
    assert(fugue_exposition_redundant_entries(&fugue) == 1);
    /* The redundant entry is not part of the first round, but it is aligned. */
    assert(holds(ENTRY(EACH_VOICE_ENTERS_ONCE), &fugue));
    assert(holds(ENTRY(ENTRIES_ALIGNED), &fugue));
    assert(fugue_exposition_entry_count(NULL) == 0);
    assert(fugue_exposition_redundant_entries(NULL) == 0);
}

static void test_first_is_subject(void) {
    FOUR_VOICE_FUGUE(fugue);
    fugue_statements[0].role = FUGUE_ROLE_ANSWER;
    assert(!holds(ENTRY(FIRST_IS_SUBJECT), &fugue));

    /* No entry at all. */
    fugue_statements[0].role = FUGUE_ROLE_FREE_COUNTERPOINT;
    fugue_statements[1].role = FUGUE_ROLE_FREE_COUNTERPOINT;
    fugue_statements[3].role = FUGUE_ROLE_FREE_COUNTERPOINT;
    fugue_statements[5].role = FUGUE_ROLE_FREE_COUNTERPOINT;
    assert(!holds(ENTRY(FIRST_IS_SUBJECT), &fugue));
}

static void test_entries_alternate(void) {
    FOUR_VOICE_FUGUE(fugue);
    fugue_statements[3].role = FUGUE_ROLE_ANSWER;
    assert(!holds(ENTRY(ENTRIES_ALTERNATE), &fugue));
    fugue_statements[3].role = FUGUE_ROLE_SUBJECT;
    assert(holds(ENTRY(ENTRIES_ALTERNATE), &fugue));

    /* Past the first round the entries are free. */
    fugue.statement_count = 8;
    fugue_statements[7].role = FUGUE_ROLE_SUBJECT;
    assert(holds(ENTRY(ENTRIES_ALTERNATE), &fugue));
}

static void test_each_voice_enters_once(void) {
    FOUR_VOICE_FUGUE(fugue);
    /* The tenor's entry moved to the alto, which sounds a note there. */
    fugue_statements[3].voice = 1;
    assert(!holds(ENTRY(EACH_VOICE_ENTERS_ONCE), &fugue));
}

static void test_entries_aligned(void) {
    const PitchSpelling *c4 = P(C, 0, 4);
    const PitchSpelling *d4 = P(D, 0, 4);
    const PitchSpelling *e4 = P(E, 0, 4);
    const PitchSpelling *g4 = P(G, 0, 4);
    const PitchSpelling *a4 = P(A, 0, 4);
    const PitchSpelling *b4 = P(B, 0, 4);
    /* The answer enters twelve slots in: half a bar late. */
    StandardCounterpointSlot upper[] = {HALF(c4), HALF(d4), HALF(e4), HALF(c4), R16()};
    StandardCounterpointSlot lower[] = {R8(), REST4(), HALF(g4), HALF(a4), HALF(b4), HALF(g4), REST4()};
    StandardCounterpointSlotLine voices[] = {LINE_OF(upper), LINE_OF(lower)};
    FugueStatement statements[] = {{0, 0, FUGUE_ROLE_SUBJECT, 4, 0}, {1, 12, FUGUE_ROLE_ANSWER, 4, 0}};
    EXPOSITION_OF(fugue, voices, statements);
    assert(fugue_exposition_valid(&fugue));
    assert(!holds(ENTRY(ENTRIES_ALIGNED), &fugue));
    fugue.slots_per_bar = 4;
    assert(holds(ENTRY(ENTRIES_ALIGNED), &fugue));
}

static void test_new_voice_at_extreme(void) {
    /* Three voices: the first enters on top, the second at the bottom, and the
       inner voice enters last. */
    const PitchSpelling *c4 = P(C, 0, 4);
    const PitchSpelling *g3 = P(G, 0, 3);
    StandardCounterpointSlot top[] = {HALF(c4), HALF(c4), HALF(c4), HALF(c4), HALF(c4), HALF(c4), HALF(c4),
                                      HALF(c4), HALF(c4), HALF(c4), HALF(c4), HALF(c4)};
    StandardCounterpointSlot inner[] = {R16(), R16(), HALF(c4), HALF(c4), HALF(c4), HALF(c4)};
    StandardCounterpointSlot bottom[] = {R16(), HALF(g3), HALF(g3), HALF(g3), HALF(g3), HALF(g3), HALF(g3),
                                         HALF(g3), HALF(g3)};
    StandardCounterpointSlotLine voices[] = {LINE_OF(top), LINE_OF(inner), LINE_OF(bottom)};
    FugueStatement statements[] = {{0, 0, FUGUE_ROLE_SUBJECT, 4, 0},
                                   {2, 16, FUGUE_ROLE_ANSWER, 4, 0},
                                   {1, 32, FUGUE_ROLE_SUBJECT, 4, 0}};
    EXPOSITION_OF(fugue, voices, statements);
    assert(fugue_exposition_valid(&fugue));
    assert(!holds(ENTRY(NEW_VOICE_AT_EXTREME), &fugue));

}

static void test_countersubject_follows(void) {
    FOUR_VOICE_FUGUE(fugue);
    /* The alto's countersubject becomes free counterpoint. */
    fugue_statements[2].role = FUGUE_ROLE_FREE_COUNTERPOINT;
    assert(!holds(ENTRY(COUNTERSUBJECT_FOLLOWS), &fugue));
    fugue_statements[2].role = FUGUE_ROLE_COUNTERSUBJECT;
    assert(holds(ENTRY(COUNTERSUBJECT_FOLLOWS), &fugue));

    /* The countersubject begins after the next entry has begun. */
    fugue_statements[4].start = 36;
    fugue_statements[4].note_count = 7;
    assert(!holds(ENTRY(COUNTERSUBJECT_FOLLOWS), &fugue));
}

static void test_invalid_and_check(void) {
    FOUR_VOICE_FUGUE(fugue);
    assert(!holds(ENTRY(RULE_COUNT), &fugue));
    assert(!holds((FugueEntryRule)-1, &fugue));
    assert(!holds(ENTRY(FIRST_IS_SUBJECT), NULL));

    unsigned hard = 7, soft = 7;
    assert(!fugue_entry_check(NULL, &hard, &soft));
    assert(hard == 7 && soft == 7);
    assert(!fugue_entry_check(&fugue, NULL, &soft));
    assert(!fugue_entry_check(&fugue, &hard, NULL));

    assert(fugue_entry_check(&fugue, &hard, &soft));
    assert(hard == 0 && soft == 0);

    fugue_statements[0].role = FUGUE_ROLE_ANSWER;
    assert(fugue_entry_check(&fugue, &hard, &soft));
    assert(hard & (1u << ENTRY(FIRST_IS_SUBJECT)));
    assert(hard & (1u << ENTRY(ENTRIES_ALTERNATE)));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_good_exposition();
    test_redundant_entries();
    test_first_is_subject();
    test_entries_alternate();
    test_each_voice_enters_once();
    test_entries_aligned();
    test_new_voice_at_extreme();
    test_countersubject_follows();
    test_invalid_and_check();
    printf("test_fugue_entry_rules: ok\n");
    return 0;
}
