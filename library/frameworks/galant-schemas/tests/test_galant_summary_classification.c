#include <assert.h>
#include <stdio.h>

#include "galant_classification.h"
#include "galant_spec_exemplar.h"
#include "galant_specs_a.h"
#include "galant_specs_b.h"

#define BIT(id) (1u << (id))
#define ALL_COUNT (SPECS_A_COUNT + SPECS_B_COUNT)

static const PrototypeSpec *spec_at(size_t i) {
    return i < SPECS_A_COUNT ? &SPECS_A[i] : &SPECS_B[i - SPECS_A_COUNT];
}

static SpecExemplar g_exemplars[ALL_COUNT];

static void build_exemplars(void) {
    for (size_t i = 0; i < ALL_COUNT; i++) {
        spec_exemplar(&g_exemplars[i], spec_at(i));
        assert(galant_exemplar_valid(&g_exemplars[i].exemplar));
    }
}

static void test_each_exemplar_resembles_only_its_own_schema(void) {
    for (size_t i = 0; i < ALL_COUNT; i++) {
        unsigned schemas = 0;
        assert(galant_classify(&g_exemplars[i].exemplar, &schemas));
        assert(schemas == BIT(spec_at(i)->id));
    }
}

static void test_resembles_for_every_pair(void) {
    for (size_t i = 0; i < ALL_COUNT; i++) {
        for (size_t j = 0; j < ALL_COUNT; j++) {
            assert(galant_resembles(&g_exemplars[i].exemplar, spec_at(j)->id) == (i == j));
        }
    }
}

static const SpecExemplar *exemplar_of(GalantSchemaId id) {
    for (size_t i = 0; i < ALL_COUNT; i++) {
        if (spec_at(i)->id == id) {
            return &g_exemplars[i];
        }
    }
    assert(0);
    return NULL;
}

static void test_near_neighbors(void) {
    /* The Meyer and the Aprile share a bass but not a melody or a beat pattern. */
    assert(!galant_resembles(&exemplar_of(GALANT_SCHEMA_MEYER)->exemplar, GALANT_SCHEMA_APRILE));
    assert(!galant_resembles(&exemplar_of(GALANT_SCHEMA_APRILE)->exemplar, GALANT_SCHEMA_MEYER));

    /* The Fonte differs from the Sol-Fa-Mi only by a raised first bass note. */
    assert(!galant_resembles(&exemplar_of(GALANT_SCHEMA_FONTE)->exemplar, GALANT_SCHEMA_SOL_FA_MI));
    assert(!galant_resembles(&exemplar_of(GALANT_SCHEMA_SOL_FA_MI)->exemplar, GALANT_SCHEMA_FONTE));

    /* The semplice, the deceptive, and the evaded cadence share a melody and
       differ in the last bass note. */
    const GalantSchemaId cadences[3] = {GALANT_SCHEMA_CADENZA_SEMPLICE, GALANT_SCHEMA_DECEPTIVE_CADENCE,
                                        GALANT_SCHEMA_EVADED_CADENCE};
    for (int a = 0; a < 3; a++) {
        for (int b = 0; b < 3; b++) {
            assert(galant_resembles(&exemplar_of(cadences[a])->exemplar, cadences[b]) == (a == b));
        }
    }

    /* The Prinner with an added dominant has five stages, the Prinner four. */
    assert(!galant_resembles(&exemplar_of(GALANT_SCHEMA_PRINNER_WITH_DOMINANT)->exemplar, GALANT_SCHEMA_PRINNER));
    assert(!galant_resembles(&exemplar_of(GALANT_SCHEMA_PRINNER)->exemplar, GALANT_SCHEMA_PRINNER_WITH_DOMINANT));
}

static void test_monte_second_notes(void) {
    SpecExemplar monte = *exemplar_of(GALANT_SCHEMA_MONTE);
    monte.exemplar.stages = monte.stages;
    assert(galant_resembles(&monte.exemplar, GALANT_SCHEMA_MONTE));
    /* Without the do-te, it is not a Monte. */
    monte.stages[0].melody_next = NULL;
    assert(!galant_resembles(&monte.exemplar, GALANT_SCHEMA_MONTE));
}

static void test_soft_differences_still_resemble(void) {
    /* An Aprile with the dominant in the bass of the second stage rather than
       re keeps the Aprile's melody and beats: its bass is less fixed. */
    SpecExemplar aprile = *exemplar_of(GALANT_SCHEMA_APRILE);
    aprile.exemplar.stages = aprile.stages;
    aprile.stages[1].bass = note(PITCH_LETTER_G, 0, 2);
    unsigned schemas = 0;
    assert(galant_classify(&aprile.exemplar, &schemas));
    assert(schemas == BIT(GALANT_SCHEMA_APRILE));
}

static void test_minor_key_prinner(void) {
    /* A Prinner on le and me in A minor, as the Prinner accepts. */
    const PitchSpelling *melody[4] = {P(F, 0, 5), P(E, 0, 5), P(D, 0, 5), P(C, 0, 5)};
    const PitchSpelling *bass[4] = {P(D, 0, 3), P(C, 0, 3), P(B, 0, 2), P(A, 0, 2)};
    const BeatStrength strengths[4] = {S, W, S, W};
    GalantFixture f;
    galant_fixture(&f, P(A, 0, 4), melody, bass, strengths);
    unsigned schemas = 0;
    assert(galant_classify(&f.exemplar, &schemas));
    assert(schemas == BIT(GALANT_SCHEMA_PRINNER));
}

int main(void) {
    build_exemplars();
    test_each_exemplar_resembles_only_its_own_schema();
    test_resembles_for_every_pair();
    test_near_neighbors();
    test_monte_second_notes();
    test_soft_differences_still_resemble();
    test_minor_key_prinner();
    printf("test_galant_summary_classification: ok\n");
    return 0;
}
