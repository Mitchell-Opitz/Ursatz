#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/framework_reference.h"
#include "core/provenance_reference.h"
#include "core/value.h"
#include "identity/entity_id.h"
#include "interpretation/interpretation.h"
#include "interpretation/uncertainty.h"

/*
 * Context + theory-tagged test:
 *
 *   1. Non-mutation of target: interpretation_target_id returns
 *      exactly the EntityID reference passed in, and Interpretation
 *      exposes no accessor that resolves target to a mutable object --
 *      the only way to reach "target" through this API is the opaque
 *      id reference (Law 8).
 *   2. Coexistence: two competing Interpretations (different claims,
 *      different status) can both be constructed pointing at the same
 *      target without either construction failing or one displacing
 *      the other.
 */

static Value *make_claim(const char *content) {
    Value *claim;
    assert(value_create("Claim", content, strlen(content) + 1, &claim) == VALUE_OK);
    return claim;
}

static Uncertainty *make_uncertainty(void) {
    Uncertainty *uncertainty;
    assert(uncertainty_create_scalar_confidence(1, 2, &uncertainty) == UNCERTAINTY_OK);
    return uncertainty;
}

int main(void) {
    EntityID *target_id = NULL;
    assert(entity_id_create("sonority-1", &target_id) == ENTITY_ID_OK);

    EntityID *interpretation_a_id;
    assert(entity_id_create("interpretation-a", &interpretation_a_id) == ENTITY_ID_OK);
    EntityID *target_ref_a;
    assert(entity_id_create("sonority-1", &target_ref_a) == ENTITY_ID_OK);
    ProvenanceReference *provenance_a;
    assert(provenance_reference_create_absent(&provenance_a) == PROVENANCE_REFERENCE_OK);

    Interpretation *interpretation_a;
    InterpretationResult result_a =
        interpretation_create(interpretation_a_id, target_ref_a, make_claim("V7"), NULL, NULL, NULL,
                               make_uncertainty(), "analyst-jane", "roman-numeral-analysis",
                               INTERPRETATION_STATUS_PROPOSED, provenance_a, &interpretation_a);
    assert(result_a == INTERPRETATION_OK);

    EntityID *interpretation_b_id;
    assert(entity_id_create("interpretation-b", &interpretation_b_id) == ENTITY_ID_OK);
    EntityID *target_ref_b;
    assert(entity_id_create("sonority-1", &target_ref_b) == ENTITY_ID_OK);
    ProvenanceReference *provenance_b;
    assert(provenance_reference_create_absent(&provenance_b) == PROVENANCE_REFERENCE_OK);

    Interpretation *interpretation_b;
    InterpretationResult result_b = interpretation_create(
        interpretation_b_id, target_ref_b, make_claim("passing-tone-cluster"), NULL, NULL, NULL, make_uncertainty(),
        "analyst-marcus", "voice-leading-analysis", INTERPRETATION_STATUS_DISPUTED, provenance_b,
        &interpretation_b);
    assert(result_b == INTERPRETATION_OK);

    /* Non-mutation: target comes back as exactly the reference passed in. */
    assert(interpretation_target_id(interpretation_a) == target_ref_a);
    assert(entity_id_equals(interpretation_target_id(interpretation_a), target_id));
    assert(entity_id_equals(interpretation_target_id(interpretation_b), target_id));

    /* Coexistence: both competing Interpretations point at the same
       target and both remain independently accessible with their own
       distinct claim/status. */
    assert(entity_id_equals(interpretation_target_id(interpretation_a), interpretation_target_id(interpretation_b)));
    assert(interpretation_status(interpretation_a) == INTERPRETATION_STATUS_PROPOSED);
    assert(interpretation_status(interpretation_b) == INTERPRETATION_STATUS_DISPUTED);

    size_t claim_a_size;
    const void *claim_a_content = value_content(interpretation_claim(interpretation_a), &claim_a_size);
    assert(strcmp((const char *)claim_a_content, "V7") == 0);

    size_t claim_b_size;
    const void *claim_b_content = value_content(interpretation_claim(interpretation_b), &claim_b_size);
    assert(strcmp((const char *)claim_b_content, "passing-tone-cluster") == 0);

    /* framework_reference is optional: neither interpretation_a nor
       interpretation_b supplied one, so both come back NULL rather than
       a resolved or default Framework. */
    assert(interpretation_framework_reference(interpretation_a) == NULL);
    assert(interpretation_framework_reference(interpretation_b) == NULL);

    interpretation_destroy(interpretation_a);
    interpretation_destroy(interpretation_b);
    entity_id_destroy(target_id);

    /* When a FrameworkReference is supplied, Interpretation holds and
       returns exactly that instance -- not a copy, not a resolved
       Framework. */
    EntityID *interpretation_c_id;
    assert(entity_id_create("interpretation-c", &interpretation_c_id) == ENTITY_ID_OK);
    EntityID *target_ref_c;
    assert(entity_id_create("sonority-2", &target_ref_c) == ENTITY_ID_OK);
    EntityID *framework_id;
    assert(entity_id_create("framework-common-practice", &framework_id) == ENTITY_ID_OK);
    FrameworkReference *framework_reference;
    assert(framework_reference_create(framework_id, &framework_reference) == FRAMEWORK_REFERENCE_OK);
    ProvenanceReference *provenance_c;
    assert(provenance_reference_create_absent(&provenance_c) == PROVENANCE_REFERENCE_OK);

    Interpretation *interpretation_c;
    InterpretationResult result_c = interpretation_create(
        interpretation_c_id, target_ref_c, make_claim("ii-V-I"), NULL, NULL, framework_reference,
        make_uncertainty(), "analyst-jane", "roman-numeral-analysis", INTERPRETATION_STATUS_PROPOSED, provenance_c,
        &interpretation_c);
    assert(result_c == INTERPRETATION_OK);

    assert(interpretation_framework_reference(interpretation_c) == framework_reference);
    assert(entity_id_equals(framework_reference_target_id(interpretation_framework_reference(interpretation_c)),
                             framework_id));

    interpretation_destroy(interpretation_c);

    printf("test_interpretation: ok\n");
    return 0;
}