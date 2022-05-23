#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "galant_specs_a.h"
#include "galant_specs_b.h"
#include "galant_test_util.h"

static void test_pre_cadential_and_cadence_data(void) {
    for (size_t i = 0; i < SPECS_B_COUNT; i++) {
        verify_spec(&SPECS_B[i]);
    }
}

static void test_every_schema_is_specified_once(void) {
    assert(SPECS_A_COUNT + SPECS_B_COUNT == GALANT_SCHEMA_COUNT);
    int seen[GALANT_SCHEMA_COUNT] = {0};
    for (size_t i = 0; i < SPECS_A_COUNT; i++) {
        seen[SPECS_A[i].id]++;
    }
    for (size_t i = 0; i < SPECS_B_COUNT; i++) {
        seen[SPECS_B[i].id]++;
    }
    for (int id = 0; id < GALANT_SCHEMA_COUNT; id++) {
        assert(seen[id] == 1);
    }
}

static void test_names_are_unique(void) {
    for (int a = 0; a < GALANT_SCHEMA_COUNT; a++) {
        for (int b = a + 1; b < GALANT_SCHEMA_COUNT; b++) {
            assert(strcmp(galant_schema_name((GalantSchemaId)a), galant_schema_name((GalantSchemaId)b)) != 0);
        }
    }
}

static void test_functions_by_group(void) {
    int counts[SCHEMA_FUNCTION_COUNT] = {0};
    for (int id = 0; id < GALANT_SCHEMA_COUNT; id++) {
        counts[galant_prototype((GalantSchemaId)id)->function]++;
    }
    assert(counts[SCHEMA_FUNCTION_OPENING_GAMBIT] == 8);
    assert(counts[SCHEMA_FUNCTION_CONTINUATION] == 7);
    assert(counts[SCHEMA_FUNCTION_PRE_CADENTIAL] == 7);
    assert(counts[SCHEMA_FUNCTION_CADENCE] == 3);
    assert(counts[SCHEMA_FUNCTION_POST_CADENTIAL] == 1);
}

static void test_prototypes_fit_the_bit_mask(void) {
    /* A classification reports one bit for each schema in an unsigned. */
    assert(GALANT_SCHEMA_COUNT <= 32);
}

static void test_passo_indietro_is_the_start_of_a_prinner(void) {
    /* The passo indietro is essentially the first two stages of a prinner:
       strong then weak, the bass falling fa to mi. */
    const GalantPrototype *passo = galant_prototype(GALANT_SCHEMA_PASSO_INDIETRO);
    const GalantPrototype *prinner = galant_prototype(GALANT_SCHEMA_PRINNER);
    assert(passo->stage_count == 2);
    assert(passo->stages[0].strength == prinner->stages[0].strength);
    assert(passo->stages[1].strength == prinner->stages[1].strength);
    assert(passo->stages[0].bass.degree == prinner->stages[0].bass.degree);
    assert(passo->stages[1].bass.degree == prinner->stages[1].bass.degree);
}

static void test_comma_is_the_closing_part_of_a_meyer(void) {
    /* The comma is fa mi over ti do, as the closing part of the Meyer. */
    const GalantPrototype *comma = galant_prototype(GALANT_SCHEMA_COMMA);
    const GalantPrototype *meyer = galant_prototype(GALANT_SCHEMA_MEYER);
    for (size_t i = 0; i < 2; i++) {
        assert(comma->stages[i].strength == meyer->stages[2 + i].strength);
        assert(comma->stages[i].melody.degree == meyer->stages[2 + i].melody.degree);
        assert(comma->stages[i].bass.degree == meyer->stages[2 + i].bass.degree);
    }
}

int main(void) {
    test_pre_cadential_and_cadence_data();
    test_every_schema_is_specified_once();
    test_names_are_unique();
    test_functions_by_group();
    test_prototypes_fit_the_bit_mask();
    test_passo_indietro_is_the_start_of_a_prinner();
    test_comma_is_the_closing_part_of_a_meyer();
    printf("test_galant_prototypes_cadential: ok\n");
    return 0;
}
