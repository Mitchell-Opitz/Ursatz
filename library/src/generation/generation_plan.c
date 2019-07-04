#include "generation/generation_plan.h"

#include <stdlib.h>

struct GenerationPlan {
    EntityID *id;
    Intent *intent;
    ConstraintExtraction *constraints;
    ProvenanceReference *provenance;
};

GenerationPlanResult generation_plan_create(EntityID *id, Intent *intent, ConstraintExtraction *constraints,
                                             ProvenanceReference *provenance, GenerationPlan **out) {
    if (id == NULL) {
        return GENERATION_PLAN_ERR_NULL_ID;
    }
    if (intent == NULL) {
        return GENERATION_PLAN_ERR_NULL_INTENT;
    }
    if (constraints == NULL) {
        return GENERATION_PLAN_ERR_NULL_CONSTRAINTS;
    }
    if (provenance == NULL) {
        return GENERATION_PLAN_ERR_NULL_PROVENANCE;
    }

    GenerationPlan *plan = (GenerationPlan *)malloc(sizeof(GenerationPlan));
    if (plan == NULL) {
        return GENERATION_PLAN_ERR_ALLOC;
    }

    plan->id = id;
    plan->intent = intent;
    plan->constraints = constraints;
    plan->provenance = provenance;

    *out = plan;
    return GENERATION_PLAN_OK;
}

void generation_plan_destroy(GenerationPlan *plan) {
    if (plan == NULL) {
        return;
    }
    entity_id_destroy(plan->id);
    intent_destroy(plan->intent);
    constraint_extraction_destroy(plan->constraints);
    provenance_reference_destroy(plan->provenance);
    free(plan);
}

const EntityID *generation_plan_id(const GenerationPlan *plan) {
    return plan->id;
}

const Intent *generation_plan_intent(const GenerationPlan *plan) {
    return plan->intent;
}

const ConstraintExtraction *generation_plan_constraints(const GenerationPlan *plan) {
    return plan->constraints;
}

const ProvenanceReference *generation_plan_provenance(const GenerationPlan *plan) {
    return plan->provenance;
}

bool generation_plan_identity_equals(const GenerationPlan *a, const GenerationPlan *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return entity_id_equals(a->id, b->id);
}