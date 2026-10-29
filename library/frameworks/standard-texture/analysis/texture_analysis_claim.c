#include "texture_analysis_claim.h"


#include "framework.h"
#include "identity/entity_id.h"
#include "interpretation/claim_interpretation.h"
#include "theory/framework.h"

Interpretation *standard_texture_analysis_interpretation(const char *interpretation_id, const char *target_id,
                                                      const char *claim_type, const void *content, size_t size,
                                                      double uncertainty_value, const char *method) {
    const Framework *framework = standard_texture_framework();
    if (framework == NULL) {
        return NULL;
    }
    return claim_interpretation_create(entity_id_value(framework_id(framework)), "standard-texture", interpretation_id, target_id,
                                       claim_type, content, size, uncertainty_value, method);
}

Relationship *standard_texture_analysis_relationship(const char *kind, const char *key, const char *source_id,
                                                  const char *target_id, const char *predicate) {
    return claim_interpretation_relationship("standard-texture", kind, key, source_id, target_id, predicate);
}
