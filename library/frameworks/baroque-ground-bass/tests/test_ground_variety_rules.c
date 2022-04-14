#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "ground_rule_check.h"
#include "ground_test_util.h"
#include "ground_variety_rules.h"

#define VARIETY(name) GROUND_VARIETY_##name

static bool holds(GroundVarietyRule rule, const GroundBassPiece *piece) {
    return ground_variety_rule_holds(rule, piece);
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(ground_variety_rule, ground_variety_constraint, GROUND_VARIETY_RULE_COUNT);
    for (int i = 0; i < GROUND_VARIETY_RULE_COUNT; i++) {
        assert(constraint_type(ground_variety_constraint((GroundVarietyRule)i)) == CONSTRAINT_TYPE_SOFT);
    }
}

static void test_names(void) {
    assert(strcmp(ground_variety_rule_name(VARIETY(UPPER_PARTS_VARY)), "upper-parts-vary") == 0);
    assert(strcmp(ground_variety_rule_name(VARIETY(UPPER_RHYTHM_VARIES)), "upper-rhythm-varies") == 0);
    assert(strcmp(ground_variety_rule_name(VARIETY(EVERY_ITERATION_IMITATES)), "every-iteration-imitates") == 0);
    assert(ground_variety_rule_name(VARIETY(RULE_COUNT)) == NULL);
}

static void test_good_piece(void) {
    GroundFixture f;
    ground_fixture(&f);
    for (int i = 0; i < GROUND_VARIETY_RULE_COUNT; i++) {
        assert(holds((GroundVarietyRule)i, &f.piece));
    }
}

static void test_upper_parts_vary(void) {
    GroundFixture f;
    ground_fixture(&f);
    /* The second iteration repeats the upper parts of the first. */
    for (size_t v = 0; v < 2; v++) {
        for (size_t s = 0; s < TEST_ITERATION; s++) {
            f.slots[v][TEST_ITERATION + s] = f.slots[v][s];
        }
    }
    assert(ground_piece_valid(&f.piece));
    assert(!holds(VARIETY(UPPER_PARTS_VARY), &f.piece));
}

static void test_upper_rhythm_varies(void) {
    GroundFixture f;
    ground_fixture(&f);
    /* The bar of eighth notes in the third iteration becomes one long note, so
       every iteration is at the same metrical level. */
    put(f.slots[0], 54, 6, P(C, 0, 5));
    assert(ground_piece_valid(&f.piece));
    assert(!holds(VARIETY(UPPER_RHYTHM_VARIES), &f.piece));
}

static void test_every_iteration_imitates(void) {
    GroundFixture f;
    ground_fixture(&f);

    /* The last iteration has no dux. */
    f.piece.statement_count = 6;
    assert(!holds(VARIETY(EVERY_ITERATION_IMITATES), &f.piece));
    f.piece.statement_count = FIXTURE_STATEMENTS;

    /* A comes is not a dux. */
    f.statements[6].role = IMITATIVE_ROLE_COMES;
    assert(!holds(VARIETY(EVERY_ITERATION_IMITATES), &f.piece));
    f.statements[6].role = IMITATIVE_ROLE_DUX;

    f.piece.statements = NULL;
    f.piece.statement_count = 0;
    assert(!holds(VARIETY(EVERY_ITERATION_IMITATES), &f.piece));
}

static void test_single_iteration(void) {
    GroundFixture f;
    ground_fixture(&f);
    f.piece.slots_per_iteration = FIXTURE_SLOTS;
    for (int i = 0; i < GROUND_VARIETY_RULE_COUNT; i++) {
        assert(holds((GroundVarietyRule)i, &f.piece));
    }
}

static void test_invalid_and_check(void) {
    GroundFixture f;
    ground_fixture(&f);
    assert(!holds(VARIETY(RULE_COUNT), &f.piece));
    assert(!holds((GroundVarietyRule)-1, &f.piece));
    assert(!holds(VARIETY(UPPER_PARTS_VARY), NULL));

    unsigned hard = 7, soft = 7;
    assert(!ground_variety_check(NULL, &hard, &soft));
    assert(hard == 7 && soft == 7);
    assert(!ground_variety_check(&f.piece, NULL, &soft));
    assert(!ground_variety_check(&f.piece, &hard, NULL));

    assert(ground_variety_check(&f.piece, &hard, &soft));
    assert(hard == 0 && soft == 0);

    f.piece.statement_count = 6;
    assert(ground_variety_check(&f.piece, &hard, &soft));
    assert(hard == 0);
    assert(soft == (1u << VARIETY(EVERY_ITERATION_IMITATES)));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_good_piece();
    test_upper_parts_vary();
    test_upper_rhythm_varies();
    test_every_iteration_imitates();
    test_single_iteration();
    test_invalid_and_check();
    printf("test_ground_variety_rules: ok\n");
    return 0;
}
