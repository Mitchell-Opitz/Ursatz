#include "scale_analysis_claim.h"


#include "scale_framework.h"
#include "identity/entity_id.h"
#include "interpretation/claim_interpretation.h"
#include "theory/framework.h"

Interpretation *scale_analysis_interpretation(const char *interpretation_id, const char *target_id,
                                                      const char *claim_type, const void *content, size_t size,
                                                      double uncertainty_value, const char *method) {
    const Framework *framework = scale_harmonization_framework();
    if (framework == NULL) {
        return NULL;
    }
    return claim_interpretation_create(entity_id_value(framework_id(framework)), "scale-harmonization", interpretation_id, target_id,
                                       claim_type, content, size, uncertainty_value, method);
}

Relationship *scale_analysis_relationship(const char *kind, const char *key, const char *source_id,
                                                  const char *target_id, const char *predicate) {
    return claim_interpretation_relationship("scale-harmonization", kind, key, source_id, target_id, predicate);
}
