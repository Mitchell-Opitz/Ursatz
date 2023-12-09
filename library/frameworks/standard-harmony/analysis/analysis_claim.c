#include "analysis_claim.h"

#include "core/framework_reference.h"
#include "core/provenance_reference.h"
#include "core/value.h"
#include "harmony_framework.h"
#include "identity/entity_id.h"
#include "interpretation/uncertainty.h"
#include "theory/framework.h"

#define CONFIDENCE_DENOMINATOR 1000000UL

Interpretation *harmony_analysis_make_interpretation(const char *interpretation_id, const char *target_id,
                                                      const char *claim_type, const void *content, size_t size,
                                                      double uncertainty_value, const char *method) {
    if (interpretation_id == NULL || interpretation_id[0] == '\0' || target_id == NULL || target_id[0] == '\0' ||
        claim_type == NULL || content == NULL || size == 0 || method == NULL || method[0] == '\0' ||
        !(uncertainty_value >= 0.0 && uncertainty_value <= 1.0)) {
        return NULL;
    }
    const Framework *framework = standard_harmony_framework();
    if (framework == NULL) {
        return NULL;
    }

    EntityID *id = NULL;
    EntityID *target = NULL;
    EntityID *framework_target = NULL;
    FrameworkReference *framework_reference = NULL;
    Value *claim = NULL;
    Uncertainty *uncertainty = NULL;
    ProvenanceReference *provenance = NULL;
    Interpretation *interpretation = NULL;

    if (entity_id_create(interpretation_id, &id) != ENTITY_ID_OK ||
        entity_id_create(target_id, &target) != ENTITY_ID_OK ||
        entity_id_create(entity_id_value(framework_id(framework)), &framework_target) != ENTITY_ID_OK) {
        goto fail;
    }
    if (framework_reference_create(framework_target, &framework_reference) != FRAMEWORK_REFERENCE_OK) {
        goto fail;
    }
    framework_target = NULL; /* owned by the reference now */
    unsigned long confidence = (unsigned long)((1.0 - uncertainty_value) * (double)CONFIDENCE_DENOMINATOR + 0.5);
    if (value_create(claim_type, content, size, &claim) != VALUE_OK ||
        uncertainty_create_scalar_confidence(confidence, CONFIDENCE_DENOMINATOR, &uncertainty) != UNCERTAINTY_OK ||
        provenance_reference_create_absent(&provenance) != PROVENANCE_REFERENCE_OK) {
        goto fail;
    }
    if (interpretation_create(id, target, claim, NULL, NULL, framework_reference, uncertainty, "standard-harmony-v1",
                              method, INTERPRETATION_STATUS_MACHINE_GENERATED, provenance,
                              &interpretation) != INTERPRETATION_OK) {
        goto fail;
    }
    return interpretation;

fail:
    entity_id_destroy(id);
    entity_id_destroy(target);
    entity_id_destroy(framework_target);
    framework_reference_destroy(framework_reference);
    value_destroy(claim);
    uncertainty_destroy(uncertainty);
    provenance_reference_destroy(provenance);
    return NULL;
}
