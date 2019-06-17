#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "core/value.h"
#include "generation/constraint_extraction.h"
#include "generation/generation_plan.h"
#include "generation/generator.h"
#include "generation/intent.h"
#include "identity/entity_id.h"

/*
 * Interface-contract test: a concrete Generator declares its metadata
 * (supported inputs/outputs, configuration, constraints, determinism,
 * version -- TDD Sec160) alongside a GeneratorFn, invoked as
 * generate(plan, random_seed, configuration) -> out. This mock
 * generator stands in for a real one (out of scope for this pass)
 * purely to exercise the interface's shape: plan/configuration are
 * borrowed, a NULL plan is rejected without touching *out, and a
 * successful call hands back a CandidateSet pointer the caller owns.
 * CandidateSet's own layout arrives with Candidate (a later commit) --
 * this test never dereferences it, only compares pointer identity.
 *
 * Generation-regression test: a fixed plan/seed always yields the same
 * CandidateSet handle from this mock -- pinning the mock generator's
 * behavior so a future refactor of the interface plumbing cannot
 * silently change what a caller receives.
 */

static int mock_candidate_set_sentinel;

static GeneratorResult mock_generate(const GenerationPlan *plan, unsigned long random_seed,
                                      const Value *configuration, CandidateSet **out) {
    (void)random_seed;
    (void)configuration;

    if (plan == NULL) {
        return GENERATOR_ERR_NULL_PLAN;
    }
    if (out == NULL) {
        return GENERATOR_ERR_NULL_OUT;
    }

    *out = (CandidateSet *)&mock_candidate_set_sentinel;
    return GENERATOR_OK;
}

static ProvenanceReference *make_absent_provenance(void) {
    ProvenanceReference *provenance;
    assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);
    return provenance;
}

static EntityID *make_id(const char *value) {
    EntityID *id;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

int main(void) {
    /* Generator interface contract: a concrete generator declares its metadata (TDD Sec160). */
    Generator mock = {"mock-generator", "GenerationPlan", "CandidateSet", "none", "none", true, "1.0.0",
                       mock_generate};
    assert(strcmp(mock.name, "mock-generator") == 0);
    assert(strcmp(mock.supported_inputs, "GenerationPlan") == 0);
    assert(strcmp(mock.supported_outputs, "CandidateSet") == 0);
    assert(strcmp(mock.configuration_description, "none") == 0);
    assert(strcmp(mock.constraints_description, "none") == 0);
    assert(mock.deterministic);
    assert(strcmp(mock.version, "1.0.0") == 0);

    /* NULL plan is rejected without touching *out. */
    CandidateSet *untouched = (CandidateSet *)0x1;
    assert(mock.generate(NULL, 0, NULL, &untouched) == GENERATOR_ERR_NULL_PLAN);
    assert(untouched == (CandidateSet *)0x1);

    /* Generation-regression: a valid plan always yields the same CandidateSet handle from this mock. */
    Intent *intent;
    assert(intent_create(make_id("intent-mock"), "counter-melody", "8-measure phrase", NULL, NULL, NULL, NULL, NULL,
                          0, NULL, 0, NULL, make_absent_provenance(), &intent) == INTENT_OK);
    ConstraintExtraction *constraints;
    assert(constraint_extraction_create(NULL, 0, NULL, 0, &constraints) == CONSTRAINT_EXTRACTION_OK);
    GenerationPlan *plan;
    assert(generation_plan_create(make_id("generation-plan-mock"), intent, constraints, make_absent_provenance(),
                                   &plan) == GENERATION_PLAN_OK);

    CandidateSet *result;
    assert(mock.generate(plan, 42, NULL, &result) == GENERATOR_OK);
    assert(result == (CandidateSet *)&mock_candidate_set_sentinel);

    generation_plan_destroy(plan);

    printf("test_generator: ok\n");
    return 0;
}