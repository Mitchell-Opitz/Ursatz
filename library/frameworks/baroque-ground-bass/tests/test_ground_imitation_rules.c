#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "ground_imitation_rules.h"
#include "ground_rule_check.h"
#include "ground_test_util.h"

#define IMITATION(name) GROUND_IMITATION_##name

static bool holds(GroundImitationRule rule, const GroundBassPiece *piece) {
    return ground_imitation_rule_holds(rule, piece);
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(ground_imitation_rule, ground_imitation_constraint,
                                    GROUND_IMITATION_RULE_COUNT);
    for (int i = 0; i < GROUND_IMITATION_RULE_COUNT; i++) {
        ConstraintType type = constraint_type(ground_imitation_constraint((GroundImitationRule)i));
        assert(type == (i <= IMITATION(COMES_IMITATES) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT));
    }
}

static void test_names(void) {
    assert(strcmp(ground_imitation_rule_name(IMITATION(COMES_FOLLOWS_DUX)), "comes-follows-dux") == 0);
    assert(strcmp(ground_imitation_rule_name(IMITATION(COMES_IMITATES)), "comes-imitates") == 0);
    assert(strcmp(ground_imitation_rule_name(IMITATION(ENTRIES_ALIGNED)), "entries-aligned") == 0);
    assert(strcmp(ground_imitation_rule_name(IMITATION(VOICES_DIFFER)), "voices-differ") == 0);
    assert(ground_imitation_rule_name(IMITATION(RULE_COUNT)) == NULL);
}

static void test_good_piece(void) {
    GroundFixture f;
    ground_fixture(&f);
    for (int i = 0; i < GROUND_IMITATION_RULE_COUNT; i++) {
        assert(holds((GroundImitationRule)i, &f.piece));
    }
}

static void test_no_comes(void) {
    GroundFixture f;
    ground_fixture(&f);
    f.piece.statement_count = 1;
    for (int i = 0; i < GROUND_IMITATION_RULE_COUNT; i++) {
        assert(holds((GroundImitationRule)i, &f.piece));
    }
    f.piece.statements = NULL;
    f.piece.statement_count = 0;
    for (int i = 0; i < GROUND_IMITATION_RULE_COUNT; i++) {
        assert(holds((GroundImitationRule)i, &f.piece));
    }
}

static void test_comes_follows_dux(void) {
    GroundFixture f;
    ground_fixture(&f);

    /* The comes begins with the dux, not after it. */
    f.statements[1].start = 0;
    assert(ground_piece_valid(&f.piece));
    assert(!holds(IMITATION(COMES_FOLLOWS_DUX), &f.piece));
    f.statements[1].start = 6;
    assert(holds(IMITATION(COMES_FOLLOWS_DUX), &f.piece));

    /* A comes with no dux before it. */
    f.statements[0].role = IMITATIVE_ROLE_COMES;
    assert(!holds(IMITATION(COMES_FOLLOWS_DUX), &f.piece));
}

static void test_comes_imitates(void) {
    GroundFixture f;
    ground_fixture(&f);

    /* The second note of the comes goes down where the dux goes up. */
    put(f.slots[1], 8, 2, P(F, 0, 4));
    assert(!holds(IMITATION(COMES_IMITATES), &f.piece));
}

static void test_comes_keeps_rhythm(void) {
    GroundFixture f;
    ground_fixture(&f);

    /* The comes starts on the long note before its motive. */
    f.statements[1].start = 0;
    assert(ground_piece_valid(&f.piece));
    assert(!holds(IMITATION(COMES_IMITATES), &f.piece));
}

static void test_short_dux(void) {
    GroundFixture f;
    ground_fixture(&f);
    /* A dux of one note is judged on that note's length alone. */
    f.statements[0].note_count = 1;
    f.statements[1].note_count = 1;
    assert(holds(IMITATION(COMES_IMITATES), &f.piece));
}

static void test_entries_aligned(void) {
    GroundFixture f;
    ground_fixture(&f);

    /* Closer imitation: the comes begins on the second quarter, a beat late. */
    f.statements[1].start = 8;
    f.statements[1].note_count = 2;
    assert(ground_piece_valid(&f.piece));
    assert(!holds(IMITATION(ENTRIES_ALIGNED), &f.piece));
    f.piece.slots_per_bar = 2;
    assert(holds(IMITATION(ENTRIES_ALIGNED), &f.piece));
}

static void test_voices_differ(void) {
    GroundFixture f;
    ground_fixture(&f);
    /* The comes is in the dux's own voice. */
    f.statements[1].voice = 0;
    assert(ground_piece_valid(&f.piece));
    assert(!holds(IMITATION(VOICES_DIFFER), &f.piece));
}

static void test_ground_imitated(void) {
    GroundFixture f;
    ground_fixture(&f);
    /* The ground itself is imitated in an upper voice: the bass is the dux. */
    f.statements[0] = (GroundStatement){2, 0, IMITATIVE_ROLE_DUX, 2};
    f.statements[1] = (GroundStatement){1, 6, IMITATIVE_ROLE_COMES, 2};
    /* The comes (an upper voice, one bar later) has a different rhythm: it is
       a motive of quarters, the ground is dotted halves. */
    assert(ground_piece_valid(&f.piece));
    assert(holds(IMITATION(COMES_FOLLOWS_DUX), &f.piece));
    assert(!holds(IMITATION(COMES_IMITATES), &f.piece));
    assert(holds(IMITATION(ENTRIES_ALIGNED), &f.piece));
    assert(holds(IMITATION(VOICES_DIFFER), &f.piece));
}

static void test_invalid_and_check(void) {
    GroundFixture f;
    ground_fixture(&f);
    assert(!holds(IMITATION(RULE_COUNT), &f.piece));
    assert(!holds((GroundImitationRule)-1, &f.piece));
    assert(!holds(IMITATION(COMES_FOLLOWS_DUX), NULL));

    unsigned hard = 7, soft = 7;
    assert(!ground_imitation_check(NULL, &hard, &soft));
    assert(hard == 7 && soft == 7);
    assert(!ground_imitation_check(&f.piece, NULL, &soft));
    assert(!ground_imitation_check(&f.piece, &hard, NULL));

    assert(ground_imitation_check(&f.piece, &hard, &soft));
    assert(hard == 0 && soft == 0);

    /* A comes a beat late: closer imitation, advice only. */
    f.statements[1].start = 8;
    f.statements[1].note_count = 2;
    assert(ground_imitation_check(&f.piece, &hard, &soft));
    assert(hard == 0);
    assert(soft == (1u << IMITATION(ENTRIES_ALIGNED)));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_good_piece();
    test_no_comes();
    test_comes_follows_dux();
    test_comes_imitates();
    test_comes_keeps_rhythm();
    test_short_dux();
    test_entries_aligned();
    test_voices_differ();
    test_ground_imitated();
    test_invalid_and_check();
    printf("test_ground_imitation_rules: ok\n");
    return 0;
}
