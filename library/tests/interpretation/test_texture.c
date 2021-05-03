#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/framework_reference.h"
#include "core/provenance_reference.h"
#include "core/value.h"
#include "identity/entity_id.h"
#include "interpretation/texture.h"
#include "interpretation/interpretation.h"
#include "interpretation/uncertainty.h"

/*
 * Relationship test: a Texture's underlying fact reference (the passage
 * it is about) resolves through its embedded Interpretation's own
 * target_id -- no separate passage_id field exists on Texture itself.
 * A Texture requires a non-NULL framework_reference, unlike the base
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

static Interpretation *make_interpretation(const char *own_id, const char *passage_id, const char *framework_id) {
    EntityID *id;
    assert(entity_id_create(own_id, &id) == ENTITY_ID_OK);
    EntityID *target_id;
    assert(entity_id_create(passage_id, &target_id) == ENTITY_ID_OK);
    ProvenanceReference *provenance;
    assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);

    FrameworkReference *framework_reference = NULL;
    if (framework_id != NULL) {
        EntityID *framework_entity_id;
        assert(entity_id_create(framework_id, &framework_entity_id) == ENTITY_ID_OK);
        assert(framework_reference_create(framework_entity_id, &framework_reference) == FRAMEWORK_REFERENCE_OK);
    }

    Interpretation *interpretation;
    assert(interpretation_create(id, target_id, make_claim("homorhythm"), NULL, NULL, framework_reference,
                                  make_uncertainty(), "analyst-jane", "texture-analysis",
                                  INTERPRETATION_STATUS_PROPOSED, provenance, &interpretation) == INTERPRETATION_OK);
    return interpretation;
}

int main(void) {
    Interpretation *interpretation = make_interpretation("interpretation-1", "passage-1", "framework-standard-texture");

    Texture *chord;
    assert(texture_create(interpretation, "homorhythm", &chord) == TEXTURE_OK);

    assert(texture_interpretation(chord) == interpretation);
    assert(strcmp(texture_label(chord), "homorhythm") == 0);

    EntityID *expected_passage_id;
    assert(entity_id_create("passage-1", &expected_passage_id) == ENTITY_ID_OK);
    assert(entity_id_equals(interpretation_target_id(texture_interpretation(chord)), expected_passage_id));
    entity_id_destroy(expected_passage_id);

    texture_destroy(chord);

    /* A framework_reference is required: unlike the base Interpretation
       type, texture_create rejects an interpretation without one. */
    Interpretation *no_framework = make_interpretation("interpretation-2", "passage-2", NULL);
    Texture *rejected;
    assert(texture_create(no_framework, "homorhythm", &rejected) == TEXTURE_ERR_NULL_FRAMEWORK_REFERENCE);
    interpretation_destroy(no_framework);

    /* texture_take_interpretation destroys only the Texture shell and hands
       back the embedded Interpretation, still valid and unaffected. */
    Interpretation *taken_source = make_interpretation("interpretation-3", "passage-3", "framework-standard-texture");
    Texture *taken_chord;
    assert(texture_create(taken_source, "polyphony", &taken_chord) == TEXTURE_OK);
    Interpretation *taken = texture_take_interpretation(taken_chord);
    assert(taken == taken_source);
    size_t claim_size;
    assert(strcmp((const char *)value_content(interpretation_claim(taken), &claim_size), "homorhythm") == 0);
    interpretation_destroy(taken);

    printf("test_texture: ok\n");
    return 0;
}