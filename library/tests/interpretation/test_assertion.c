#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/value.h"
#include "identity/entity_id.h"
#include "interpretation/assertion.h"

/*
 * Structural (context, theory-tagged) test: confirms an Assertion
 * references its subject without embedding/duplicating it --
 * assertion_subject_id returns exactly the EntityID reference passed
 * in, and Assertion exposes no accessor that returns a resolved
 * subject object.
 */

int main(void) {
    EntityID *id;
    assert(entity_id_create("assertion-1", &id) == ENTITY_ID_OK);
    EntityID *subject_id;
    assert(entity_id_create("cadence-1", &subject_id) == ENTITY_ID_OK);

    const char content[] = "authentic";
    Value *claim;
    assert(value_create("Claim", content, sizeof(content), &claim) == VALUE_OK);

    Assertion *assertion;
    AssertionResult result = assertion_create(id, subject_id, claim, "analyst-jane", &assertion);
    assert(result == ASSERTION_OK);

    assert(assertion_subject_id(assertion) == subject_id);
    assert(entity_id_equals(assertion_subject_id(assertion), subject_id));

    size_t claim_size;
    const void *claim_content = value_content(assertion_claim(assertion), &claim_size);
    assert(claim_size == sizeof(content));
    assert(memcmp(claim_content, content, claim_size) == 0);

    assert(strcmp(assertion_agent_identifier(assertion), "analyst-jane") == 0);

    assertion_destroy(assertion);

    /* Missing subject is rejected. */
    EntityID *id_2;
    assert(entity_id_create("assertion-2", &id_2) == ENTITY_ID_OK);
    const char content_2[] = "plagal";
    Value *claim_2;
    assert(value_create("Claim", content_2, sizeof(content_2), &claim_2) == VALUE_OK);

    Assertion *rejected;
    assert(assertion_create(id_2, NULL, claim_2, "analyst-jane", &rejected) == ASSERTION_ERR_NULL_SUBJECT_ID);

    entity_id_destroy(id_2);
    value_destroy(claim_2);

    printf("test_assertion: ok\n");
    return 0;
}