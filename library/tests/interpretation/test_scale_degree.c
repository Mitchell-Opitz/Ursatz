#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/framework_reference.h"
#include "core/provenance_reference.h"
#include "core/value.h"
#include "identity/entity_id.h"
#include "interpretation/interpretation.h"
#include "interpretation/scale_degree.h"
#include "interpretation/uncertainty.h"
#include "pitch/pitch_identity.h"

/*
 * Relationship test: a ScaleDegree's underlying fact reference (the
 * Scale it is about) resolves through its embedded Interpretation's
 * own target_id -- no separate scale_id field exists on ScaleDegree
 * itself. Its pitch field is owned by value (PitchIdentity is an
 * opaque token, not an Entity to reference). A ScaleDegree requires a
 * non-NULL framework_reference, unlike the base Interpretation type
 * where it is optional.
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

static Interpretation *make_interpretation(const char *own_id, const char *scale_id, const char *framework_id) {
    EntityID *id;
    assert(entity_id_create(own_id, &id) == ENTITY_ID_OK);
    EntityID *target_id;
    assert(entity_id_create(scale_id, &target_id) == ENTITY_ID_OK);
    ProvenanceReference *provenance;
    assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);

    FrameworkReference *framework_reference = NULL;
    if (framework_id != NULL) {
        EntityID *framework_entity_id;
        assert(entity_id_create(framework_id, &framework_entity_id) == ENTITY_ID_OK);
        assert(framework_reference_create(framework_entity_id, &framework_reference) == FRAMEWORK_REFERENCE_OK);
    }

    Interpretation *interpretation;
    assert(interpretation_create(id, target_id, make_claim("V"), NULL, NULL, framework_reference,
                                  make_uncertainty(), "analyst-jane", "roman-numeral-analysis",
                                  INTERPRETATION_STATUS_PROPOSED, provenance, &interpretation) == INTERPRETATION_OK);
    return interpretation;
}

static PitchIdentity *make_pitch(const char *token) {
    PitchIdentity *identity;
    assert(pitch_identity_create(token, 2, &identity) == PITCH_IDENTITY_OK);
    return identity;
}

int main(void) {
    Interpretation *interpretation = make_interpretation("interpretation-1", "scale-1", "framework-common-practice");
    PitchIdentity *pitch = make_pitch("g\0");

    ScaleDegree *scale_degree;
    assert(scale_degree_create(interpretation, pitch, "V", &scale_degree) == SCALE_DEGREE_OK);

    assert(scale_degree_interpretation(scale_degree) == interpretation);
    assert(scale_degree_pitch(scale_degree) == pitch);
    assert(strcmp(scale_degree_label(scale_degree), "V") == 0);

    EntityID *expected_scale_id;
    assert(entity_id_create("scale-1", &expected_scale_id) == ENTITY_ID_OK);
    assert(entity_id_equals(interpretation_target_id(scale_degree_interpretation(scale_degree)), expected_scale_id));
    entity_id_destroy(expected_scale_id);

    scale_degree_destroy(scale_degree);

    /* A framework_reference is required: unlike the base Interpretation
       type, scale_degree_create rejects an interpretation without one. */
    Interpretation *no_framework = make_interpretation("interpretation-2", "scale-2", NULL);
    PitchIdentity *pitch_2 = make_pitch("a\0");
    ScaleDegree *rejected;
    assert(scale_degree_create(no_framework, pitch_2, "V", &rejected) == SCALE_DEGREE_ERR_NULL_FRAMEWORK_REFERENCE);
    interpretation_destroy(no_framework);
    pitch_identity_destroy(pitch_2);

    printf("test_scale_degree: ok\n");
    return 0;
}