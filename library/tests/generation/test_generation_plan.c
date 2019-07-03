#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "generation/constraint_extraction.h"
#include "generation/generation_plan.h"
#include "generation/intent.h"
#include "identity/entity_id.h"
#include "rules/constraint.h"
#include "theory/framework.h"

/*
 * Construction test: GenerationPlan bundles an Intent and a
 * ConstraintExtraction into the single object a Generator consumes.
 */

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

static Intent *make_intent(const char *id_value) {
    Intent *intent;
    assert(intent_create(make_id(id_value), "counter-melody", "8-measure phrase", NULL, NULL, NULL, NULL, NULL, 0,
                          NULL, 0, NULL, make_absent_provenance(), &intent) == INTENT_OK);
    return intent;
}

static ConstraintExtraction *make_constraints(const Framework *framework) {
    Constraint *constraint;
    assert(constraint_create(make_id("constraint-vocal-range"), framework, NULL, CONSTRAINT_TYPE_HARD, 0, 0, NULL,
                              &constraint) == CONSTRAINT_OK);
    Constraint *constraints[1] = {constraint};
    ConstraintExtraction *extraction;
    assert(constraint_extraction_create(constraints, 1, NULL, 0, &extraction) == CONSTRAINT_EXTRACTION_OK);
    return extraction;
}

int main(void) {
    Framework *framework;
    assert(framework_create(make_id("framework-generation-test"), "generation-test", &framework) == FRAMEWORK_OK);

    EntityID *id = make_id("generation-plan-counter-melody");
    Intent *intent = make_intent("intent-counter-melody");
    ConstraintExtraction *constraints = make_constraints(framework);

    GenerationPlan *plan;
    assert(generation_plan_create(id, intent, constraints, make_absent_provenance(), &plan) == GENERATION_PLAN_OK);

    assert(strcmp(entity_id_value(generation_plan_id(plan)), "generation-plan-counter-melody") == 0);
    assert(generation_plan_intent(plan) == intent);
    assert(strcmp(intent_purpose(generation_plan_intent(plan)), "counter-melody") == 0);
    assert(generation_plan_constraints(plan) == constraints);
    assert(constraint_extraction_constraint_count(generation_plan_constraints(plan)) == 1);
    assert(provenance_reference_state(generation_plan_provenance(plan)) == PROVENANCE_REFERENCE_ABSENT);

    EntityID *other_id = make_id("generation-plan-other");
    Intent *other_intent = make_intent("intent-other");
    ConstraintExtraction *other_constraints = make_constraints(framework);
    GenerationPlan *other;
    assert(generation_plan_create(other_id, other_intent, other_constraints, make_absent_provenance(), &other) ==
           GENERATION_PLAN_OK);

    assert(generation_plan_identity_equals(plan, plan));
    assert(!generation_plan_identity_equals(plan, other));

    EntityID *no_intent_id = make_id("generation-plan-rejects-no-intent");
    ConstraintExtraction *no_intent_constraints = make_constraints(framework);
    ProvenanceReference *no_intent_provenance = make_absent_provenance();
    GenerationPlan *rejected_no_intent;
    assert(generation_plan_create(no_intent_id, NULL, no_intent_constraints, no_intent_provenance,
                                   &rejected_no_intent) == GENERATION_PLAN_ERR_NULL_INTENT);
    entity_id_destroy(no_intent_id);
    constraint_extraction_destroy(no_intent_constraints);
    provenance_reference_destroy(no_intent_provenance);

    EntityID *no_constraints_id = make_id("generation-plan-rejects-no-constraints");
    Intent *no_constraints_intent = make_intent("intent-rejected");
    ProvenanceReference *no_constraints_provenance = make_absent_provenance();
    GenerationPlan *rejected_no_constraints;
    assert(generation_plan_create(no_constraints_id, no_constraints_intent, NULL, no_constraints_provenance,
                                   &rejected_no_constraints) == GENERATION_PLAN_ERR_NULL_CONSTRAINTS);
    entity_id_destroy(no_constraints_id);
    intent_destroy(no_constraints_intent);
    provenance_reference_destroy(no_constraints_provenance);

    generation_plan_destroy(plan);
    generation_plan_destroy(other);
    framework_destroy(framework);

    printf("test_generation_plan: ok\n");
    return 0;
}