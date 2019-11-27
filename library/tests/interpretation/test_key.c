#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/framework_reference.h"
#include "core/provenance_reference.h"
#include "core/value.h"
#include "identity/entity_id.h"
#include "interpretation/interpretation.h"
#include "interpretation/key.h"
#include "interpretation/uncertainty.h"

/*
 * Relationship test: a Key's underlying fact reference (the Context it
 * is about) resolves through its embedded Interpretation's own
 * target_id -- no separate context_id field exists on Key itself. A
 * Key requires a non-NULL framework_reference, unlike the base
 * Interpretation type where it is optional.
 */

static Value *make_claim(const char *content) {
    Value *claim;
    assert(value_create("Claim", content, strlen(content) + 1, &claim) == VALUE_OK);
    return claim;
}

static Uncertainty *make_uncertainty(void) {
    Uncertainty *uncertainty;
    assert(uncertainty_create_scalar_confidence(1, 1, &uncertainty) == UNCERTAINTY_OK);
    return uncertainty;
}

static Interpretation *make_interpretation(const char *own_id, const char *context_id, const char *framework_id) {
    EntityID *id;
    assert(entity_id_create(own_id, &id) == ENTITY_ID_OK);
    EntityID *target_id;
    assert(entity_id_create(context_id, &target_id) == ENTITY_ID_OK);
    ProvenanceReference *provenance;
    assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);

    FrameworkReference *framework_reference = NULL;
    if (framework_id != NULL) {
        EntityID *framework_entity_id;
        assert(entity_id_create(framework_id, &framework_entity_id) == ENTITY_ID_OK);
        assert(framework_reference_create(framework_entity_id, &framework_reference) == FRAMEWORK_REFERENCE_OK);
    }

    Interpretation *interpretation;
    assert(interpretation_create(id, target_id, make_claim("D major"), NULL, NULL, framework_reference,
                                  make_uncertainty(), "analyst-jane", "key-estimation",
                                  INTERPRETATION_STATUS_PROPOSED, provenance, &interpretation) == INTERPRETATION_OK);
    return interpretation;
}

int main(void) {
    Interpretation *interpretation = make_interpretation("interpretation-1", "context-1", "framework-common-practice");

    Key *key;
    assert(key_create(interpretation, "D major", &key) == KEY_OK);

    assert(key_interpretation(key) == interpretation);
    assert(strcmp(key_label(key), "D major") == 0);

    EntityID *expected_context_id;
    assert(entity_id_create("context-1", &expected_context_id) == ENTITY_ID_OK);
    assert(entity_id_equals(interpretation_target_id(key_interpretation(key)), expected_context_id));
    entity_id_destroy(expected_context_id);

    key_destroy(key);

    /* A framework_reference is required: unlike the base Interpretation
       type, key_create rejects an interpretation without one. */
    Interpretation *no_framework = make_interpretation("interpretation-2", "context-2", NULL);
    Key *rejected;
    assert(key_create(no_framework, "D major", &rejected) == KEY_ERR_NULL_FRAMEWORK_REFERENCE);
    interpretation_destroy(no_framework);

    /* key_take_interpretation destroys only the Key shell and hands back
       the embedded Interpretation, still valid and unaffected. */
    Interpretation *taken_source = make_interpretation("interpretation-3", "context-3", "framework-common-practice");
    Key *taken_key;
    assert(key_create(taken_source, "A minor", &taken_key) == KEY_OK);
    Interpretation *taken = key_take_interpretation(taken_key);
    assert(taken == taken_source);
    size_t claim_size;
    assert(strcmp((const char *)value_content(interpretation_claim(taken), &claim_size), "D major") == 0);
    interpretation_destroy(taken);

    printf("test_key: ok\n");
    return 0;
}