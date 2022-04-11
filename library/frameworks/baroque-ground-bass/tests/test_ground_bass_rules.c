#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "ground_bass_rules.h"
#include "ground_rule_check.h"
#include "ground_test_util.h"

#define BASS(name) GROUND_BASS_##name

static bool holds(GroundBassRule rule, const GroundBassPiece *piece) {
    return ground_bass_rule_holds(rule, piece);
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(ground_bass_rule, ground_bass_constraint, GROUND_BASS_RULE_COUNT);
    for (int i = 0; i < GROUND_BASS_RULE_COUNT; i++) {
        ConstraintType type = constraint_type(ground_bass_constraint((GroundBassRule)i));
        assert(type == (i <= BASS(TRIPLE_METER) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT));
    }
}

static void test_names(void) {
    assert(strcmp(ground_bass_rule_name(BASS(ITERATIONS_WHOLE)), "iterations-whole") == 0);
    assert(strcmp(ground_bass_rule_name(BASS(GROUND_REPEATS)), "ground-repeats") == 0);
    assert(strcmp(ground_bass_rule_name(BASS(BASS_LOWEST)), "bass-lowest") == 0);
    assert(strcmp(ground_bass_rule_name(BASS(TRIPLE_METER)), "triple-meter") == 0);
    assert(strcmp(ground_bass_rule_name(BASS(EXACT_REPEAT)), "exact-repeat") == 0);
    assert(ground_bass_rule_name(BASS(RULE_COUNT)) == NULL);
}

static void test_good_piece(void) {
    GroundFixture f;
    ground_fixture(&f);
    for (int i = 0; i < GROUND_BASS_RULE_COUNT; i++) {
        assert(holds((GroundBassRule)i, &f.piece));
    }
}

static void test_iterations_whole(void) {
    GroundFixture f;
    ground_fixture(&f);
    f.piece.slots_per_iteration = 25;
    assert(!holds(BASS(ITERATIONS_WHOLE), &f.piece));
    f.piece.slots_per_iteration = 48;
    assert(holds(BASS(ITERATIONS_WHOLE), &f.piece));
}

static void test_altered_notes(void) {
    GroundFixture f;
    ground_fixture(&f);
    size_t altered = 99;
    assert(ground_bass_altered_notes(&f.piece, 0, &altered) && altered == 0);
    assert(ground_bass_altered_notes(&f.piece, 3, &altered) && altered == 0);
    altered = 99;
    assert(!ground_bass_altered_notes(&f.piece, 4, &altered));
    assert(!ground_bass_altered_notes(&f.piece, 0, NULL));
    assert(!ground_bass_altered_notes(NULL, 0, &altered));
    assert(altered == 99);

    /* The second note of iteration 2 is F sharp. */
    put(f.slots[2], 54, 6, P(F, 1, 2));
    assert(ground_bass_altered_notes(&f.piece, 2, &altered) && altered == 1);
    assert(ground_bass_altered_notes(&f.piece, 1, &altered) && altered == 0);

    /* The first bar of iteration 3 is split into two notes: another rhythm. */
    put(f.slots[2], 72, 3, P(G, 0, 2));
    put(f.slots[2], 75, 3, P(G, 0, 2));
    assert(ground_bass_altered_notes(&f.piece, 3, &altered) && altered == 5);
}

static void test_ground_repeats(void) {
    GroundFixture f;
    ground_fixture(&f);

    /* One altered note is "nearly" the ground, but not exact. */
    put(f.slots[2], 54, 6, P(F, 1, 2));
    assert(holds(BASS(GROUND_REPEATS), &f.piece));
    assert(!holds(BASS(EXACT_REPEAT), &f.piece));

    /* Two are too many for the default limit. */
    put(f.slots[2], 60, 6, P(E, 0, 2));
    assert(!holds(BASS(GROUND_REPEATS), &f.piece));

    GroundLimits two = {2};
    f.piece.limits = &two;
    assert(holds(BASS(GROUND_REPEATS), &f.piece));
    GroundLimits none = {0};
    f.piece.limits = &none;
    assert(!holds(BASS(GROUND_REPEATS), &f.piece));
}

static void test_changed_rhythm(void) {
    GroundFixture f;
    ground_fixture(&f);
    put(f.slots[2], 72, 3, P(G, 0, 2));
    put(f.slots[2], 75, 3, P(G, 0, 2));
    assert(!holds(BASS(GROUND_REPEATS), &f.piece));
}

static void test_bass_lowest(void) {
    GroundFixture f;
    ground_fixture(&f);
    /* The first upper note sounds below the bass. */
    put(f.slots[0], 0, 2, P(C, 0, 2));
    assert(!holds(BASS(BASS_LOWEST), &f.piece));
}

static void test_triple_meter(void) {
    GroundFixture f;
    ground_fixture(&f);
    f.piece.type = OSTINATO_TYPE_CHACONNE;
    assert(holds(BASS(TRIPLE_METER), &f.piece));
    f.piece.beats_per_bar = 2;
    assert(!holds(BASS(TRIPLE_METER), &f.piece));
    f.piece.type = OSTINATO_TYPE_PASSACAGLIA;
    assert(!holds(BASS(TRIPLE_METER), &f.piece));
    f.piece.beats_per_bar = 3;
    assert(holds(BASS(TRIPLE_METER), &f.piece));

    /* Other kinds are free in meter. */
    f.piece.type = OSTINATO_TYPE_GROUND_BASS;
    f.piece.beats_per_bar = 4;
    assert(holds(BASS(TRIPLE_METER), &f.piece));
    f.piece.type = OSTINATO_TYPE_OSTINATO;
    assert(holds(BASS(TRIPLE_METER), &f.piece));
}

static void test_invalid_and_check(void) {
    GroundFixture f;
    ground_fixture(&f);
    assert(!holds(BASS(RULE_COUNT), &f.piece));
    assert(!holds((GroundBassRule)-1, &f.piece));
    assert(!holds(BASS(GROUND_REPEATS), NULL));

    unsigned hard = 7, soft = 7;
    assert(!ground_bass_check(NULL, &hard, &soft));
    assert(hard == 7 && soft == 7);
    assert(!ground_bass_check(&f.piece, NULL, &soft));
    assert(!ground_bass_check(&f.piece, &hard, NULL));

    assert(ground_bass_check(&f.piece, &hard, &soft));
    assert(hard == 0 && soft == 0);

    put(f.slots[2], 54, 6, P(F, 1, 2));
    assert(ground_bass_check(&f.piece, &hard, &soft));
    assert(hard == 0);
    assert(soft == (1u << BASS(EXACT_REPEAT)));

    put(f.slots[2], 60, 6, P(E, 0, 2));
    assert(ground_bass_check(&f.piece, &hard, &soft));
    assert(hard == (1u << BASS(GROUND_REPEATS)));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_good_piece();
    test_iterations_whole();
    test_altered_notes();
    test_ground_repeats();
    test_changed_rhythm();
    test_bass_lowest();
    test_triple_meter();
    test_invalid_and_check();
    printf("test_ground_bass_rules: ok\n");
    return 0;
}
