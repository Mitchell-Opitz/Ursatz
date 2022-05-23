#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "galant_matching_rules.h"
#include "galant_prototypes.h"
#include "galant_rule_check.h"
#include "galant_test_util.h"

#define MATCH(name) GALANT_MATCH_##name

static bool holds(GalantMatchRule rule, GalantSchemaId id, const GalantExemplar *exemplar) {
    GalantMatch match = {galant_prototype(id), exemplar};
    return galant_matching_rule_holds(rule, &match);
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(galant_matching_rule, galant_matching_constraint, GALANT_MATCH_RULE_COUNT);
    for (int i = 0; i < GALANT_MATCH_RULE_COUNT; i++) {
        ConstraintType type = constraint_type(galant_matching_constraint((GalantMatchRule)i));
        assert(type == (i <= MATCH(BASS_DEFINING) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT));
    }
}

static void test_names(void) {
    assert(strcmp(galant_matching_rule_name(MATCH(STAGE_COUNT)), "stage-count") == 0);
    assert(strcmp(galant_matching_rule_name(MATCH(METER)), "meter") == 0);
    assert(strcmp(galant_matching_rule_name(MATCH(MELODY)), "melody") == 0);
    assert(strcmp(galant_matching_rule_name(MATCH(BASS_DEFINING)), "bass-defining") == 0);
    assert(strcmp(galant_matching_rule_name(MATCH(BASS_USUAL)), "bass-usual") == 0);
    assert(strcmp(galant_matching_rule_name(MATCH(TENTHS)), "tenths") == 0);
    assert(galant_matching_rule_name(MATCH(RULE_COUNT)) == NULL);
}

static void test_good_exemplars(void) {
    GalantFixture meyer, prinner, minor, modulating;
    meyer_in_c(&meyer);
    prinner_in_c(&prinner);
    prinner_in_a_minor(&minor);
    modulating_prinner_in_c(&modulating);
    for (int i = 0; i < GALANT_MATCH_RULE_COUNT; i++) {
        assert(holds((GalantMatchRule)i, GALANT_SCHEMA_MEYER, &meyer.exemplar));
        assert(holds((GalantMatchRule)i, GALANT_SCHEMA_PRINNER, &prinner.exemplar));
        assert(holds((GalantMatchRule)i, GALANT_SCHEMA_PRINNER, &minor.exemplar));
        assert(holds((GalantMatchRule)i, GALANT_SCHEMA_MODULATING_PRINNER, &modulating.exemplar));
    }
}

static void test_stage_count(void) {
    GalantFixture f;
    meyer_in_c(&f);
    f.exemplar.stage_count = 3;
    assert(!holds(MATCH(STAGE_COUNT), GALANT_SCHEMA_MEYER, &f.exemplar));
    assert(!holds(MATCH(METER), GALANT_SCHEMA_MEYER, &f.exemplar));
    assert(!holds(MATCH(MELODY), GALANT_SCHEMA_MEYER, &f.exemplar));
    assert(!holds(MATCH(BASS_USUAL), GALANT_SCHEMA_MEYER, &f.exemplar));
    assert(!holds(MATCH(BASS_DEFINING), GALANT_SCHEMA_PRINNER, &f.exemplar));
}

static void test_meter(void) {
    GalantFixture f;
    meyer_in_c(&f);
    /* The strong and weak beats swapped. */
    f.stages[0].strength = S;
    f.stages[1].strength = W;
    f.stages[2].strength = S;
    f.stages[3].strength = W;
    assert(!holds(MATCH(METER), GALANT_SCHEMA_MEYER, &f.exemplar));
    /* A medium beat is neither. */
    meyer_in_c(&f);
    f.stages[1].strength = BEAT_STRENGTH_MEDIUM;
    assert(!holds(MATCH(METER), GALANT_SCHEMA_MEYER, &f.exemplar));
}

static void test_melody(void) {
    GalantFixture f;
    meyer_in_c(&f);
    /* Mi instead of fa in the third stage. */
    f.stages[2].melody = P(E, 0, 4);
    assert(!holds(MATCH(MELODY), GALANT_SCHEMA_MEYER, &f.exemplar));

    /* A lowered mi where the Meyer asks for mi. */
    meyer_in_c(&f);
    f.stages[3].melody = P(E, -1, 4);
    assert(!holds(MATCH(MELODY), GALANT_SCHEMA_MEYER, &f.exemplar));
}

static void test_prinner_alterations(void) {
    GalantFixture f;
    prinner_in_c(&f);
    /* The Prinner accepts la or le, and mi or me. */
    f.stages[0].melody = P(A, -1, 4);
    f.stages[3].melody = P(E, -1, 4);
    assert(holds(MATCH(MELODY), GALANT_SCHEMA_PRINNER, &f.exemplar));
    /* But not a raised one. */
    f.stages[0].melody = P(A, 1, 4);
    assert(!holds(MATCH(MELODY), GALANT_SCHEMA_PRINNER, &f.exemplar));
}

static void test_bass_defining(void) {
    GalantFixture f;
    prinner_in_c(&f);
    /* Do instead of fa under the first stage. */
    f.stages[0].bass = P(C, 0, 3);
    assert(!holds(MATCH(BASS_DEFINING), GALANT_SCHEMA_PRINNER, &f.exemplar));
    /* The Meyer's bass is advice only, so the defining rule holds for it. */
    meyer_in_c(&f);
    f.stages[1].bass = P(G, 0, 2);
    assert(holds(MATCH(BASS_DEFINING), GALANT_SCHEMA_MEYER, &f.exemplar));
}

static void test_bass_usual(void) {
    GalantFixture f;
    meyer_in_c(&f);
    /* The dominant in the bass of the second stage, rather than re. */
    f.stages[1].bass = P(G, 0, 2);
    assert(!holds(MATCH(BASS_USUAL), GALANT_SCHEMA_MEYER, &f.exemplar));
    assert(holds(MATCH(BASS_DEFINING), GALANT_SCHEMA_MEYER, &f.exemplar));
    /* A defining bass is judged by the hard rule, not this one. */
    prinner_in_c(&f);
    f.stages[0].bass = P(C, 0, 3);
    assert(holds(MATCH(BASS_USUAL), GALANT_SCHEMA_PRINNER, &f.exemplar));
}

static void test_tenths(void) {
    GalantFixture f;
    prinner_in_c(&f);
    /* The second stage's melody is a ninth above the bass. */
    f.stages[1].melody = P(F, 0, 4);
    assert(!holds(MATCH(TENTHS), GALANT_SCHEMA_PRINNER, &f.exemplar));

    /* A third is as good as a tenth. */
    prinner_in_c(&f);
    f.stages[0].bass = P(F, 0, 4);
    f.stages[0].melody = P(A, 0, 4);
    assert(holds(MATCH(TENTHS), GALANT_SCHEMA_PRINNER, &f.exemplar));

    /* A melody below the bass is not a tenth. */
    f.stages[0].melody = P(D, 0, 4);
    assert(!holds(MATCH(TENTHS), GALANT_SCHEMA_PRINNER, &f.exemplar));

    /* The Meyer does not ask for tenths. */
    meyer_in_c(&f);
    assert(holds(MATCH(TENTHS), GALANT_SCHEMA_MEYER, &f.exemplar));
}

static void test_meyer_in_minor_is_not_accepted(void) {
    /* The Meyer's last melody degree is mi, not me, so a minor-key Meyer that
       ends on me is not an exemplar of the prototype as encoded. */
    GalantFixture f;
    meyer_in_c(&f);
    f.stages[3].melody = P(E, -1, 4);
    GalantMatch match = {galant_prototype(GALANT_SCHEMA_MEYER), &f.exemplar};
    unsigned hard, soft;
    assert(galant_matching_check(&match, &hard, &soft));
    assert(hard == (1u << MATCH(MELODY)));
}

static void test_second_melody_note(void) {
    /* A prototype whose first stage has two melody notes, do then te, over a
       bass of mi; its second stage has one. */
    GalantPrototype prototype = {GALANT_SCHEMA_MEYER,
                                 "two-notes",
                                 SCHEMA_FUNCTION_CONTINUATION,
                                 2,
                                 {{BEAT_STRENGTH_WEAK, {1, GALANT_ALTERATION_NATURAL}, {3, GALANT_ALTERATION_NATURAL},
                                   NULL, "I", NULL, {7, GALANT_ALTERATION_LOWERED}},
                                  {BEAT_STRENGTH_STRONG, {6, GALANT_ALTERATION_NATURAL},
                                   {4, GALANT_ALTERATION_NATURAL}, NULL, "IV", NULL,
                                   {0, GALANT_ALTERATION_NATURAL}}},
                                 false,
                                 false};
    GalantExemplarStage stages[2] = {{P(C, 0, 5), P(E, 0, 3), W, P(B, -1, 4)},
                                     {P(A, 0, 4), P(F, 0, 3), S, NULL}};
    GalantExemplar exemplar = {stages, 2, P(C, 0, 4)};
    GalantMatch match = {&prototype, &exemplar};
    assert(galant_matching_rule_holds(MATCH(MELODY), &match));

    /* The second note is missing. */
    stages[0].melody_next = NULL;
    assert(!galant_matching_rule_holds(MATCH(MELODY), &match));

    /* The second note is te's natural, ti. */
    stages[0].melody_next = P(B, 0, 4);
    assert(!galant_matching_rule_holds(MATCH(MELODY), &match));

    /* A second note where the prototype has none. */
    stages[0].melody_next = P(B, -1, 4);
    stages[1].melody_next = P(G, 0, 4);
    assert(!galant_matching_rule_holds(MATCH(MELODY), &match));
}

static void test_invalid_and_check(void) {
    GalantFixture f;
    meyer_in_c(&f);
    GalantMatch good = {galant_prototype(GALANT_SCHEMA_MEYER), &f.exemplar};
    assert(galant_match_valid(&good));
    assert(!galant_match_valid(NULL));

    GalantMatch no_prototype = {NULL, &f.exemplar};
    assert(!galant_match_valid(&no_prototype));
    GalantMatch no_exemplar = {galant_prototype(GALANT_SCHEMA_MEYER), NULL};
    assert(!galant_match_valid(&no_exemplar));
    GalantExemplar broken = f.exemplar;
    broken.tonic = NULL;
    GalantMatch broken_match = {galant_prototype(GALANT_SCHEMA_MEYER), &broken};
    assert(!galant_match_valid(&broken_match));

    assert(!galant_matching_rule_holds(MATCH(RULE_COUNT), &good));
    assert(!galant_matching_rule_holds((GalantMatchRule)-1, &good));
    assert(!galant_matching_rule_holds(MATCH(MELODY), NULL));

    unsigned hard = 7, soft = 7;
    assert(!galant_matching_check(NULL, &hard, &soft));
    assert(hard == 7 && soft == 7);
    assert(!galant_matching_check(&good, NULL, &soft));
    assert(!galant_matching_check(&good, &hard, NULL));

    assert(galant_matching_check(&good, &hard, &soft));
    assert(hard == 0 && soft == 0);

    /* Wrong beats: a hard failure; a different bass: advice. */
    f.stages[0].strength = S;
    f.stages[1].bass = P(G, 0, 2);
    assert(galant_matching_check(&good, &hard, &soft));
    assert(hard == (1u << MATCH(METER)));
    assert(soft == (1u << MATCH(BASS_USUAL)));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_good_exemplars();
    test_stage_count();
    test_meter();
    test_melody();
    test_prinner_alterations();
    test_bass_defining();
    test_bass_usual();
    test_tenths();
    test_meyer_in_minor_is_not_accepted();
    test_second_melody_note();
    test_invalid_and_check();
    printf("test_galant_matching_rules: ok\n");
    return 0;
}
