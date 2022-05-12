#include <assert.h>
#include <stdio.h>

#include "galant_classification.h"
#include "galant_test_util.h"

#define BIT(id) (1u << (id))

static void test_each_exemplar_resembles_its_prototype(void) {
    GalantFixture meyer, prinner, minor, modulating;
    meyer_in_c(&meyer);
    prinner_in_c(&prinner);
    prinner_in_a_minor(&minor);
    modulating_prinner_in_c(&modulating);

    unsigned schemas = 99;
    assert(galant_classify(&meyer.exemplar, &schemas));
    assert(schemas == BIT(GALANT_SCHEMA_MEYER));
    assert(galant_classify(&prinner.exemplar, &schemas));
    assert(schemas == BIT(GALANT_SCHEMA_PRINNER));
    assert(galant_classify(&minor.exemplar, &schemas));
    assert(schemas == BIT(GALANT_SCHEMA_PRINNER));
    assert(galant_classify(&modulating.exemplar, &schemas));
    assert(schemas == BIT(GALANT_SCHEMA_MODULATING_PRINNER));
}

static void test_resembles(void) {
    GalantFixture prinner;
    prinner_in_c(&prinner);
    assert(galant_resembles(&prinner.exemplar, GALANT_SCHEMA_PRINNER));
    assert(!galant_resembles(&prinner.exemplar, GALANT_SCHEMA_MEYER));
    assert(!galant_resembles(&prinner.exemplar, GALANT_SCHEMA_MODULATING_PRINNER));
    assert(!galant_resembles(&prinner.exemplar, GALANT_SCHEMA_COUNT));
    assert(!galant_resembles(NULL, GALANT_SCHEMA_PRINNER));
}

static void test_soft_rules_do_not_count(void) {
    /* A Meyer with the dominant in the bass of the second stage breaks only a
       soft rule, so it still resembles the Meyer. */
    GalantFixture f;
    meyer_in_c(&f);
    f.stages[1].bass = P(G, 0, 2);
    unsigned schemas = 0;
    assert(galant_classify(&f.exemplar, &schemas));
    assert(schemas == BIT(GALANT_SCHEMA_MEYER));

    /* A Prinner whose second melody note drops below its bass is off the
       tenths, which is a soft rule too. */
    prinner_in_c(&f);
    f.stages[1].melody = P(G, 0, 2);
    assert(galant_classify(&f.exemplar, &schemas));
    assert(schemas == BIT(GALANT_SCHEMA_PRINNER));
}

static void test_no_prototype(void) {
    /* Four stages on the same notes, on the same beats: no schema. */
    GalantFixture f;
    const PitchSpelling *c4 = P(C, 0, 4);
    const PitchSpelling *melody[4] = {c4, c4, c4, c4};
    const PitchSpelling *bass[4] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};
    const BeatStrength strengths[4] = {S, S, S, S};
    galant_fixture(&f, c4, melody, bass, strengths);
    unsigned schemas = 99;
    assert(galant_classify(&f.exemplar, &schemas));
    assert(schemas == 0);
}

static void test_invalid_input(void) {
    GalantFixture f;
    meyer_in_c(&f);
    unsigned schemas = 99;
    assert(!galant_classify(NULL, &schemas));
    assert(!galant_classify(&f.exemplar, NULL));
    GalantExemplar broken = f.exemplar;
    broken.tonic = NULL;
    assert(!galant_classify(&broken, &schemas));
    assert(schemas == 99);
}

int main(void) {
    test_each_exemplar_resembles_its_prototype();
    test_resembles();
    test_soft_rules_do_not_count();
    test_no_prototype();
    test_invalid_input();
    printf("test_galant_classification: ok\n");
    return 0;
}
