#include "galant_analysis_claim.h"


#include "galant_framework.h"
#include "identity/entity_id.h"
#include "interpretation/claim_interpretation.h"
#include "theory/framework.h"

Interpretation *galant_analysis_interpretation(const char *interpretation_id, const char *target_id,
                                                      const char *claim_type, const void *content, size_t size,
                                                      double uncertainty_value, const char *method) {
    const Framework *framework = galant_schemas_framework();
    if (framework == NULL) {
        return NULL;
    }
    return claim_interpretation_create(entity_id_value(framework_id(framework)), "galant-schemas", interpretation_id, target_id,
                                       claim_type, content, size, uncertainty_value, method);
}

Relationship *galant_analysis_relationship(const char *kind, const char *key, const char *source_id,
                                                  const char *target_id, const char *predicate) {
    return claim_interpretation_relationship("galant-schemas", kind, key, source_id, target_id, predicate);
}
