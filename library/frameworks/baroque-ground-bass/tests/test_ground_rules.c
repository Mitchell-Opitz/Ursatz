#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "ground_rules.h"
#include "ground_test_util.h"

static void test_group_names(void) {
    assert(strcmp(ground_group_name(GROUND_GROUP_GROUND), "ground") == 0);
    assert(strcmp(ground_group_name(GROUND_GROUP_VARIETY), "variety") == 0);
    assert(strcmp(ground_group_name(GROUND_GROUP_IMITATION), "imitation") == 0);
    assert(ground_group_name(GROUND_GROUP_COUNT) == NULL);
    assert(ground_group_name((GroundGroup)-1) == NULL);
}

static void test_good_piece(void) {
    GroundFixture f;
    ground_fixture(&f);
    GroundResult result;
    assert(ground_check(&f.piece, &result));
    assert(result.hard_violations == 0 && result.soft_violations == 0);
    assert(ground_valid(&f.piece));
}

static void test_each_group_reports(void) {
    GroundResult result;

    /* Two bass notes of the third iteration are altered: the ground group. */
    GroundFixture ground;
    ground_fixture(&ground);
    put(ground.slots[2], 54, 6, P(F, 1, 2));
    put(ground.slots[2], 60, 6, P(E, 0, 2));
    assert(ground_check(&ground.piece, &result));
    assert(result.hard_violations == (1u << GROUND_GROUP_GROUND));
    assert(!ground_valid(&ground.piece));

    /* The last iteration has no dux: advice from the variety group. */
    GroundFixture variety;
    ground_fixture(&variety);
    variety.piece.statement_count = 6;
    assert(ground_check(&variety.piece, &result));
    assert(result.hard_violations == 0);
    assert(result.soft_violations == (1u << GROUND_GROUP_VARIETY));
    assert(ground_valid(&variety.piece));

    /* A comes with no dux before it: the imitation group. */
    GroundFixture imitation;
    ground_fixture(&imitation);
    imitation.statements[0].role = IMITATIVE_ROLE_COMES;
    assert(ground_check(&imitation.piece, &result));
    assert(result.hard_violations & (1u << GROUND_GROUP_IMITATION));
    assert(!ground_valid(&imitation.piece));
}

static void test_triple_meter_form(void) {
    GroundFixture f;
    ground_fixture(&f);
    f.piece.type = OSTINATO_TYPE_PASSACAGLIA;
    assert(ground_valid(&f.piece));
    f.piece.beats_per_bar = 4;
    GroundResult result;
    assert(ground_check(&f.piece, &result));
    assert(result.hard_violations == (1u << GROUND_GROUP_GROUND));
}

static void test_invalid_input(void) {
    GroundFixture f;
    ground_fixture(&f);
    GroundResult result = {7, 7};
    assert(!ground_check(NULL, &result));
    assert(!ground_check(&f.piece, NULL));
    assert(!ground_valid(NULL));

    GroundBassPiece one_voice = f.piece;
    one_voice.voice_count = 1;
    assert(!ground_check(&one_voice, &result));
    assert(!ground_valid(&one_voice));
    assert(result.hard_violations == 7 && result.soft_violations == 7);
}

int main(void) {
    test_group_names();
    test_good_piece();
    test_each_group_reports();
    test_triple_meter_form();
    test_invalid_input();
    printf("test_ground_rules: ok\n");
    return 0;
}
